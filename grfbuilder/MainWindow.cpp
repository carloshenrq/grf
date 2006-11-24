#include <QtGui>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QProgressDialog>
#include <QDir>
#include <QImage>
#include "MainWindow.h"
#ifdef __WIN32
#include <windows.h> /* Sleep() */
#else
#include <unistd.h> /* usleep() */
#endif

/* libgrf */
#include <libgrf.h>

#include <stdio.h>

void MainWindow::myLocaleChange() {
	QString newlng(QObject::sender()->objectName());
//	printf("foo %s\n", QObject::sender()->objectName().toLatin1().data());
	QCoreApplication::removeTranslator(&this->translator);
	if (newlng == QString("en")) {
		this->RetranslateStrings();
		return;
	}
	if (this->translator.load(QString("grfbuilder_") + newlng)) {
		QCoreApplication::installTranslator(&this->translator);
		this->RetranslateStrings();
	}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
	this->grf = NULL;
	this->image_viewer = NULL;
	ui.setupUi(this);
	this->setCompressionLevel(5);
	this->setRepackType(GRF_REPACK_FAST);
	ui.view_allfiles->setColumnHidden(0, true);
	ui.viewSearch->setColumnHidden(0, true);
	ui.view_filestree->setColumnHidden(2, true);

	// Locales
	QAction *lng;
	#define GRFBUILDER_SET_LOCALE(_x,_y) lng = new QAction(this); lng->setObjectName(QString::fromUtf8(_x));  \
	ui.menuLanguage->addAction(lng); \
	lng->setText(QString::fromUtf8(_y)); \
	QObject::connect(lng, SIGNAL(triggered()), this, SLOT(myLocaleChange()));
	GRFBUILDER_SET_LOCALE(L_TRADITIONAL_CHINESE_LOC, L_TRADITIONAL_CHINESE_NAME);
	GRFBUILDER_SET_LOCALE(L_ENGLISH_LOC, L_ENGLISH_NAME);
	GRFBUILDER_SET_LOCALE(L_FRENCH_LOC, L_FRENCH_NAME);
	GRFBUILDER_SET_LOCALE(L_GERMAN_LOC, L_GERMAN_NAME);
	GRFBUILDER_SET_LOCALE(L_SPANISH_LOC, L_SPANISH_NAME);
}

void MainWindow::RetranslateStrings() {
	uint32_t version=grf_version();
	uint8_t major, minor, revision;
	major = (version >> 16) & 0xff;
	minor = (version >> 8) & 0xff;
	revision = version & 0xff;
	ui.retranslateUi(this);
	((QDialog*)this)->setWindowTitle(tr("GrfBuilder v%1.%2.%3 (libgrf v%4.%5.%6) by MagicalTux"
		).arg(GRFBUILDER_VERSION_MAJOR).arg(GRFBUILDER_VERSION_MINOR).arg(GRFBUILDER_VERSION_REVISION).arg(major).arg(minor).arg(revision));
}

bool MainWindow::progress_callback(void *grf, int pos, int max) {
	ui.progbar->setValue(pos * 100 / max);
	return true;
}

static bool grf_callback_caller(void *MW_, void *grf, int pos, int max, const char *filename) {
	MainWindow *MW = (MainWindow *)MW_;
	return MW->progress_callback(grf, pos, max);
}

QString MainWindow::showSizeAsString(unsigned int s) {
	if (s > (1024*1024*1024*1.4)) {
		return tr("%1 GiB").arg((double)(s / (1024*1024*1024)), 0, 'f', 1);
	}
	if (s > (1024*1024*1.4)) {
		return tr("%1 MiB").arg((double)(s / (1024*1024)), 0, 'f', 1);
	}
	if (s > (1024*1.4)) {
		return tr("%1 kiB").arg((double)(s / (1024)), 0, 'f', 1);
	}
	return tr("%1 B").arg(s);
}

unsigned int MainWindow::fillFilesTree(void *dir, QTreeWidgetItem *parent) {
	void **list = grf_tree_list_node(dir);
	unsigned int total_size = 0;
	for(int i=0;list[i]!=NULL;i++) {
		unsigned int s;
		QTreeWidgetItem *__f = new QTreeWidgetItem(parent);
		__f->setText(0, QString::fromUtf8(euc_kr_to_utf8(grf_tree_get_name(list[i])))); // name
		if (grf_tree_is_dir(list[i])) {
			s=MainWindow::fillFilesTree(list[i], __f);
			total_size += s;
//			__f->setText(1, QString("[%1]").arg(s));
			__f->setText(1, QString("[") + this->showSizeAsString(s) + QString("]")); // realsize
			__f->setText(2, "-1");
		} else {
			void *f = grf_tree_get_file(list[i]);
			s = grf_file_get_size(f);
			total_size += s;
			__f->setText(1, this->showSizeAsString(s)); // realsize
			__f->setText(2, QString("%1").arg(grf_file_get_id(f)));
		}
	}
	delete list;
	return total_size;
}

unsigned int MainWindow::fillFilesTree(void *dir, QTreeWidget *parent) {
	void **list = grf_tree_list_node(dir);
	unsigned int total_size = 0;
	for(int i=0;list[i]!=NULL;i++) {
		unsigned int s;
		QTreeWidgetItem *__f = new QTreeWidgetItem(parent);
		__f->setText(0, QString::fromUtf8(euc_kr_to_utf8(grf_tree_get_name(list[i])))); // name
		if (grf_tree_is_dir(list[i])) {
			s=MainWindow::fillFilesTree(list[i], __f);
			total_size += s;
			__f->setText(1, QString("[") + this->showSizeAsString(s) + QString("]")); // realsize
			__f->setText(2, "-1");
		} else {
			void *f = grf_tree_get_file(list[i]);
			s = grf_file_get_size(f);
			total_size += s;
			__f->setText(1, this->showSizeAsString(s)); // realsize
			__f->setText(2, QString("%1").arg(grf_file_get_id(f)));
		}
	}
	delete list;
	return total_size;
}

void MainWindow::on_tab_sel_currentChanged(int idx) {
	if (idx == 0) return;
	if (this->grf == NULL) {
		ui.tab_sel->setCurrentIndex(0);
		return;
	}
	if (idx == 1) {
		if (this->grf_has_tree) return;
		this->grf_has_tree = true;
		grf_create_tree(this->grf);
		// fill ui.view_filestree recursively
		MainWindow::fillFilesTree(grf_tree_get_root(this->grf), ui.view_filestree);
		// sort the total
		ui.view_filestree->sortItems(0, Qt::AscendingOrder);
	}
	if (idx == 2) {
		this->DoUpdateFilter(ui.listFilter->currentText());
	}
}

void MainWindow::on_action_Open_triggered() {
	this->on_btn_open_clicked();
}

bool MainWindow::repack_progress_callback(void *grf, int pos, int max, const char *filename, QProgressDialog *prog) {
	prog->setValue(pos);
	QCoreApplication::processEvents();
	prog->setLabelText(tr("Moving file %1...").arg(QString::fromUtf8(euc_kr_to_utf8(filename))));
	if (prog->wasCanceled()) return false;
	return true;
}

static bool grf_repack_callback_caller(void *PROG_, void *grf, int pos, int max, const char *filename) {
	QProgressDialog *prog = (QProgressDialog *)PROG_;
	MainWindow *MW = (MainWindow *)prog->parent();
	return MW->repack_progress_callback(grf, pos, max, filename, prog);
}

void MainWindow::on_btn_repack_clicked() {
	double gained_space;
	if (this->grf == NULL) return;
	gained_space = (grf_wasted_space(this->grf) * 100 / this->grf_file.size());
	if (QMessageBox::question(this, tr("GrfBuilder"), tr("Repacking this file will reduce it by %1% (%2). Do you want to continue?").arg(gained_space, 0, 'f', 1).arg(this->showSizeAsString(grf_wasted_space(this->grf))), QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Cancel) return;
	QProgressDialog prog(tr("Repack in progress..."), tr("Cancel"), 0, grf_filecount(this->grf), this);
	prog.setWindowModality(Qt::WindowModal);
	grf_set_callback(this->grf, grf_repack_callback_caller, (void *)&prog);
	grf_repack(this->grf, this->repack_type);
	grf_set_callback(this->grf, grf_callback_caller, (void *)this);
	prog.reset();
	prog.close();
}

void MainWindow::on_actionRepack_triggered() {
	this->on_btn_repack_clicked();
}

void MainWindow::on_actionMerge_Dir_triggered() {
	this->on_btn_mergedir_clicked();
}

struct files_list {
	QFile f;
	QString p; // path inside grf (no euc-kr)
}

void MainWindow::on_btn_mergedir_clicked() {
	QString xpath = QFileDialog::getExistingDirectory(this, tr("Import directory..."));
	if (xpath.isNull()) return;
	// ok, we got a directory, scan it for all files and directories...
	QList
}

void MainWindow::on_action_Merge_GRF_triggered() {
	this->on_btn_mergegrf_clicked();
}

bool MainWindow::merge_progress_callback(void *grf, int pos, int max, const char *filename, QProgressDialog *prog) {
	prog->setValue(pos);
	QCoreApplication::processEvents();
	prog->setLabelText(tr("Merging file %1...").arg(QString::fromUtf8(euc_kr_to_utf8(filename))));
	if (prog->wasCanceled()) return false;
	return true;
}

static bool merge_grf_callback_caller(void *PROG_, void *grf, int pos, int max, const char *filename) {
	if (filename == NULL) return true;
	QProgressDialog *prog = (QProgressDialog *)PROG_;
	MainWindow *MW = (MainWindow *)prog->parent();
	return MW->merge_progress_callback(grf, pos, max, filename, prog);
}

void MainWindow::on_btn_mergegrf_clicked() {
	void *f;
	void *grf;
	if (this->grf == NULL) return;
	QString str = QFileDialog::getOpenFileName(this, tr("Open File"),
		NULL, tr("GRF Files (*.grf *.gpf)"));
	if (str.isNull()) return;
	QFile mgrf_file(str);
	if (!mgrf_file.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(this, tr("GrfBuilder"), tr("Could not open file %1 in read-only mode.").arg(str), QMessageBox::Cancel, QMessageBox::Cancel);
		return;
	}
	grf = grf_new_by_fd(mgrf_file.handle(), false);
	grf_set_callback(grf, grf_callback_caller, (void *)this);
	grf = grf_load_from_new(grf);
	if (grf == NULL) {
		QMessageBox::warning(this, tr("GrfBuilder"), tr("The selected file doesn't look like a valid GRF file."), QMessageBox::Cancel, QMessageBox::Cancel);
		return;
	}
	QProgressDialog prog(tr("Merge in progress..."), tr("Cancel"), 0, grf_filecount(grf), this);
	grf_set_callback(this->grf, merge_grf_callback_caller, (void *)&prog);
	grf_merge(this->grf, grf, this->repack_type);
	grf_free(grf);
	grf_set_callback(this->grf, grf_callback_caller, (void *)this);
	grf_save(this->grf);
	prog.reset();
	prog.close();
	ui.tab_sel->setCurrentIndex(0);
	f = grf_get_file_first(this->grf);
	ui.view_allfiles->clear();
	while(f != NULL) {
		QTreeWidgetItem *__item = new QTreeWidgetItem(ui.view_allfiles);
		__item->setText(0, QString("%1").arg(grf_file_get_id(f)));
		__item->setText(1, this->showSizeAsString(grf_file_get_storage_size(f))); // compsize
		__item->setText(2, this->showSizeAsString(grf_file_get_size(f))); // realsize
		__item->setText(3, QString("%1").arg(grf_file_get_storage_pos(f))); // pos
		if (euc_kr_to_utf8(grf_file_get_filename(f)) == NULL) printf("ARGH %s\n", grf_file_get_filename(f));
		__item->setText(4, QString::fromUtf8(euc_kr_to_utf8(grf_file_get_filename(f)))); // name
//		__item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsEditable);
		f = grf_get_file_next(f);
	}
	ui.view_allfiles->sortItems(4, Qt::AscendingOrder);
	this->grf_has_tree = false;
	ui.view_filestree->clear();
}

void MainWindow::on_btn_open_clicked() {
	QString str = QFileDialog::getOpenFileName(this, tr("Open File"),
			NULL, tr("GRF Files (*.grf *.gpf)"));
	void *f;

	if (str.isNull()) return;

	this->on_btn_close_clicked();

	this->grf_file.setFileName(str);
	if (!this->grf_file.open(QIODevice::ReadWrite)) {
		QMessageBox::warning(this, tr("GrfBuilder"), tr("Could not load this file in read/write mode."), QMessageBox::Cancel, QMessageBox::Cancel);
		return;
	}
	this->grf = grf_new_by_fd(this->grf_file.handle(), true);
	this->grf_has_tree = false;
	grf_set_callback(this->grf, grf_callback_caller, (void *)this);
	grf_set_compression_level(this->grf, this->compression_level);
	this->grf = grf_load_from_new(this->grf);
	if (this->grf == NULL) {
		QMessageBox::warning(this, tr("GrfBuilder"), tr("The selected file doesn't look like a valid GRF file."), QMessageBox::Cancel, QMessageBox::Cancel);
		return;
	}
	ui.tab_sel->setCurrentIndex(0);
	f = grf_get_file_first(this->grf);
	while(f != NULL) {
		QTreeWidgetItem *__item = new QTreeWidgetItem(ui.view_allfiles);
		__item->setText(0, QString("%1").arg(grf_file_get_id(f)));
		__item->setText(1, this->showSizeAsString(grf_file_get_storage_size(f))); // compsize
		__item->setText(2, this->showSizeAsString(grf_file_get_size(f))); // realsize
		__item->setText(3, QString("%1").arg(grf_file_get_storage_pos(f))); // pos
		if (euc_kr_to_utf8(grf_file_get_filename(f)) == NULL) printf("ARGH %s\n", grf_file_get_filename(f));
		__item->setText(4, QString::fromUtf8(euc_kr_to_utf8(grf_file_get_filename(f)))); // name
//		__item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsEditable);
		f = grf_get_file_next(f);
	}
	ui.view_allfiles->sortItems(4, Qt::AscendingOrder);
	// enable buttons
	ui.btn_extract->setEnabled(true);
	ui.btn_delete->setEnabled(true);
	ui.btn_extractall->setEnabled(true);
	ui.btn_repack->setEnabled(true);
	ui.btn_close->setEnabled(true);
	ui.btn_mergegrf->setEnabled(true);
	ui.btn_mergedir->setEnabled(true);
	// menuitems
	ui.action_Extract->setEnabled(true);
	ui.actionDelete->setEnabled(true);
	ui.action_Extract_All->setEnabled(true);
	ui.actionRepack->setEnabled(true);
	ui.action_Close->setEnabled(true);
	ui.action_Merge_GRF->setEnabled(true);
	ui.actionMerge_Dir->setEnabled(true);
}

void MainWindow::on_action_Close_triggered() {
	this->on_btn_close_clicked();
}

void MainWindow::on_btn_close_clicked() {
	if (this->grf != NULL) {
		grf_free(this->grf);
		this->grf_file.close();
		this->grf = NULL;
		this->grf_has_tree = false;
		ui.view_filestree->clear();
		ui.progbar->setValue(0);
		ui.tab_sel->setCurrentIndex(0);
		ui.view_allfiles->clear();
		ui.viewSearch->clear();
		this->last_search.fromAscii("");
		// disable buttons
		ui.btn_extract->setEnabled(false);
		ui.btn_delete->setEnabled(false);
		ui.btn_extractall->setEnabled(false);
		ui.btn_repack->setEnabled(false);
		ui.btn_close->setEnabled(false);
		ui.btn_mergegrf->setEnabled(false);
		ui.btn_mergedir->setEnabled(false);
		// menuitems
		ui.action_Extract->setEnabled(false);
		ui.actionDelete->setEnabled(false);
		ui.action_Extract_All->setEnabled(false);
		ui.actionRepack->setEnabled(false);
		ui.action_Close->setEnabled(false);
		ui.action_Merge_GRF->setEnabled(false);
		ui.actionMerge_Dir->setEnabled(false);
	}
}

void MainWindow::on_actionUnicode_triggered() {
	ui.actionUnicode->setChecked(true);
	ui.actionStandard->setChecked(false);
}

void MainWindow::closeEvent(QCloseEvent *ev) {
	if (this->image_viewer) delete this->image_viewer;
	this->on_btn_close_clicked();
	ev->accept();
}

void MainWindow::on_actionAbout_triggered() {
	uint32_t version=grf_version();
	uint8_t major, minor, revision;
	major = (version >> 16) & 0xff;
	minor = (version >> 8) & 0xff;
	revision = version & 0xff;
	QMessageBox::information(this, tr("GrfBuilder"),
		tr(
			"<p align=\"center\"><b>GrfBuilder v%1.%2.%3 by MagicalTux</b></p>"
			"<p align=\"left\">Linked against libgrf v%4.%5.%6 (also by MagicalTux)<br />"
			"This tool is designed to allow easy read and write access to GRF files.<br />"
			"This was developped for the sole purpose of demonstrating that Gravity need better developpers."
			"</p>"
			"<p align=\"left\">You can contact MagicalTux on <a href=\"http://ookoo.org/cgi-bin/cgi-irc/irc.cgi\">irc://irc.ookoo.org/ooKoo</a></p>"
		).arg(GRFBUILDER_VERSION_MAJOR).arg(GRFBUILDER_VERSION_MINOR).arg(GRFBUILDER_VERSION_REVISION).arg(major).arg(minor).arg(revision)
	);
}

void MainWindow::on_btn_delete_clicked() {
	this->on_actionDelete_triggered();
}

void MainWindow::on_actionDelete_triggered() {
	QList<QTreeWidgetItem *> objlist;
	int lsize, i;
	void *f;
	if (this->grf == NULL) return;

	switch(ui.tab_sel->currentIndex()) {
		case 0: case 2:
			if (ui.tab_sel->currentIndex() == 0) {
				objlist = ui.view_allfiles->selectedItems();
			} else {
				objlist = ui.viewSearch->selectedItems();
			}
			if (objlist.size()==0) break;
			if (objlist.size()==1) {
				// YAY easy to do :o
				void *gfile = grf_get_file_by_id(this->grf, objlist[0]->text(0).toUInt());
				QString name(QString::fromUtf8(euc_kr_to_utf8(grf_file_get_basename(gfile))));
				if (QMessageBox::question(this, tr("GrfBuilder"), tr("Are you sure you want to delete the file `%1'?").arg(name), QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Cancel) return;
				grf_file_delete(gfile);
				break;
			}
			lsize = objlist.size();
			if (QMessageBox::question(this, tr("GrfBuilder"), tr("Are you sure you want to delete the %1 selected files?").arg(lsize), QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Cancel) return;
			for(i=0;i<lsize;i++) {
				QTreeWidgetItem *cur = objlist[i];
				void *cur_file = grf_get_file_by_id(this->grf, cur->text(0).toUInt());
				grf_file_delete(cur_file);
			}
			break;
		case 1:
			QMessageBox::warning(this, tr("GrfBuilder"), QString("Sorry, this is not working yet."), QMessageBox::Cancel, QMessageBox::Cancel);
			break;
	}
	grf_save(this->grf);
	ui.tab_sel->setCurrentIndex(0);
	f = grf_get_file_first(this->grf);
	ui.view_allfiles->clear();
	while(f != NULL) {
		QTreeWidgetItem *__item = new QTreeWidgetItem(ui.view_allfiles);
		__item->setText(0, QString("%1").arg(grf_file_get_id(f)));
		__item->setText(1, this->showSizeAsString(grf_file_get_storage_size(f))); // compsize
		__item->setText(2, this->showSizeAsString(grf_file_get_size(f))); // realsize
		__item->setText(3, QString("%1").arg(grf_file_get_storage_pos(f))); // pos
		if (euc_kr_to_utf8(grf_file_get_filename(f)) == NULL) printf("ARGH %s\n", grf_file_get_filename(f));
		__item->setText(4, QString::fromUtf8(euc_kr_to_utf8(grf_file_get_filename(f)))); // name
//		__item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsEditable);
		f = grf_get_file_next(f);
	}
	ui.view_allfiles->sortItems(4, Qt::AscendingOrder);
	this->grf_has_tree = false;
	ui.view_filestree->clear();
}


void MainWindow::on_btn_extract_clicked() {
	this->on_action_Extract_triggered();
}

void MainWindow::on_action_Extract_triggered() {
	QList<QTreeWidgetItem *> objlist; // <-- BUG ON THIS LINE
	QString xpath, ppath;
	int lsize,i;
	QProgressDialog prog(tr("Extraction in progress..."), tr("Cancel"), 0, 1, this);
	if (this->grf == NULL) return;

	switch(ui.tab_sel->currentIndex()) {
		case 0:
		case 2:
			if (ui.tab_sel->currentIndex() == 0) {
				objlist = ui.view_allfiles->selectedItems();
			} else {
				objlist = ui.viewSearch->selectedItems();
			}
			if (objlist.size()==0) break;
			if (objlist.size()==1) {
				// YAY easy to do :o
				void *gfile = grf_get_file_by_id(this->grf, objlist[0]->text(0).toUInt());
				QFileInfo file;
				if (ui.actionUnicode->isChecked()) {
					file.setFile(QString::fromUtf8(euc_kr_to_utf8(grf_file_get_basename(gfile))));
				} else {
					file.setFile(QString::fromLatin1(grf_file_get_basename(gfile)));
				}
				QString filename = QFileDialog::getSaveFileName(this, tr("Extract file as..."), file.fileName(), tr("%1 files (*.%1)").arg(file.suffix()));
				if (filename.isNull()) return; /* nothing to do */
				QFile out(filename);
				if (!out.open(QIODevice::WriteOnly)) {
					QMessageBox::warning(this, tr("GrfBuilder"), tr("Could not open %1 for writing a file.").arg(filename), QMessageBox::Cancel, QMessageBox::Cancel);
					return;
				}
				if (!grf_file_put_contents_to_fd(gfile, out.handle())) {
					out.remove();
					QMessageBox::warning(this, tr("GrfBuilder"), tr("Could not write data to %1 while extracting a file.").arg(filename), QMessageBox::Cancel, QMessageBox::Cancel);
					return;
				}
				out.close();
				break;
			}
			xpath = QFileDialog::getExistingDirectory(this, tr("Extract to..."));
			if (xpath.isNull()) return;
			ppath = QDir::currentPath();
			QDir::setCurrent(xpath);
			lsize = objlist.size();
			prog.setWindowModality(Qt::WindowModal);
			prog.setRange(0, lsize);
			for(i=0;i<lsize;i++) {
				QTreeWidgetItem *cur = objlist[i];
				void *cur_file = grf_get_file_by_id(this->grf, cur->text(0).toUInt());
				QString name(QString::fromUtf8(euc_kr_to_utf8(grf_file_get_filename(cur_file))));
				QCoreApplication::processEvents();
				if (prog.wasCanceled()) break;
				prog.setLabelText(tr("Extracting file %1...").arg(name));
				prog.setValue(i);
				if (ui.actionUnicode->isChecked()) {
					size_t size;
					size = grf_file_get_size(cur_file);
#ifdef __WIN32
					name.replace("/", "\\");
					QString dirname(name);
					dirname.resize(name.lastIndexOf("\\"));
#else
					name.replace("\\", "/");
					QString dirname(name);
					dirname.resize(name.lastIndexOf("/"));
#endif
					QDir foo(dirname);
					foo.mkpath(".");
					QFile output(name);
					if (!output.open(QIODevice::WriteOnly)) {
						cur_file = grf_get_file_next(cur_file);
						continue;
					}
					if (grf_file_put_contents_to_fd(cur_file, output.handle()) != size) {
						output.close();
						cur_file = grf_get_file_next(cur_file);
						continue;
					}
				} else {
					grf_put_contents_to_file(cur_file, grf_file_get_filename(cur_file));
				}
			}
			prog.reset();
			prog.close();
			QDir::setCurrent(ppath);
			break;
		case 1:
			QMessageBox::warning(this, tr("GrfBuilder"), QString("Sorry, this is not working yet."), QMessageBox::Cancel, QMessageBox::Cancel);
			break;
	}
}

void MainWindow::on_viewSearch_customContextMenuRequested(const QPoint point) {
	QMenu menu(this);
	menu.addAction(ui.action_Extract);
	menu.addAction(ui.actionDelete);
	menu.addAction(ui.action_Extract_All);
	menu.addAction(ui.actionRepack);
	menu.exec(ui.view_allfiles->viewport()->mapToGlobal(point));
}

void MainWindow::on_view_allfiles_customContextMenuRequested(const QPoint point) {
	QMenu menu(this);
	menu.addAction(ui.action_Extract);
	menu.addAction(ui.actionDelete);
	menu.addAction(ui.action_Extract_All);
	menu.addAction(ui.actionRepack);
	menu.exec(ui.view_allfiles->viewport()->mapToGlobal(point));
}

void MainWindow::on_action_Quit_triggered() {
//	QCoreApplication::exit();
	this->close();
}

void MainWindow::on_actionStandard_triggered() {
	ui.actionUnicode->setChecked(false);
	ui.actionStandard->setChecked(true);
}

void MainWindow::on_action_Extract_All_triggered() {
	this->on_btn_extractall_clicked();
}

void MainWindow::on_btn_extractall_clicked() {
	void *cur_file;
	int c=0;
	int n = 1;
	if (this->grf == NULL) return;
	QString xpath = QFileDialog::getExistingDirectory(this, tr("Extract to..."));
	if (xpath.isNull()) return;
	QString ppath = QDir::currentPath();
	QDir::setCurrent(xpath);
	QProgressDialog prog(tr("Extraction in progress..."), tr("Cancel"), 0, grf_filecount(this->grf), this);
	prog.setWindowModality(Qt::WindowModal);
	/* get files list */
	cur_file = grf_get_file_first(this->grf);
	while(cur_file != NULL) {
		c++;
		prog.setValue(c);
		if (prog.wasCanceled()) break;
		QString name(QString::fromUtf8(euc_kr_to_utf8(grf_file_get_filename(cur_file))));
		n -= grf_file_get_size(cur_file);
		if (n<=0) {
			n = 5000000;
			prog.setLabelText(tr("Extracting file %1...").arg(name));
			QCoreApplication::processEvents();
		}
		if (ui.actionUnicode->isChecked()) {
			size_t size;
			size = grf_file_get_size(cur_file);
#ifdef __WIN32
			name.replace("/", "\\");
			QString dirname(name);
			dirname.resize(name.lastIndexOf("\\"));
#else
			name.replace("\\", "/");
			QString dirname(name);
			dirname.resize(name.lastIndexOf("/"));
#endif
			QDir foo(dirname);
			foo.mkpath(".");
			QFile output(name);
			if (!output.open(QIODevice::WriteOnly)) {
				cur_file = grf_get_file_next(cur_file);
				continue;
			}
			if (grf_file_put_contents_to_fd(cur_file, output.handle()) != size) {
				output.close();
				cur_file = grf_get_file_next(cur_file);
				continue;
			}
		} else {
			grf_put_contents_to_file(cur_file, grf_file_get_filename(cur_file));
		}
		cur_file = grf_get_file_next(cur_file);
	}
	prog.reset();
	prog.close();
	QDir::setCurrent(ppath);
}

void MainWindow::do_display_wav(void *f) {
	QFile tmp;
#ifndef __WIN32
	if (!QSound::isAvailable()) {
		QMessageBox::warning(this, tr("GrfBuilder"), tr("Your computer has no audio support. Please make sure you have an audio device available and retry."), QMessageBox::Cancel, QMessageBox::Cancel);
		return;
	}
	QMessageBox mb(tr("GrfBuilder"), tr("Currently playing file `%1'. Press \"Ok\" to stop.").arg(QString::fromUtf8(euc_kr_to_utf8(grf_file_get_filename(f)))), QMessageBox::Information, 0, 0, 0, this);
	mb.show();
#endif
	for(int i=0;1;i++) {
		tmp.setFileName(QString("%1_tmp.wav").arg(i));
		if (!tmp.exists()) break;
	}
	if (!tmp.open(QIODevice::WriteOnly)) return;
	if (grf_file_put_contents_to_fd(f, tmp.handle()) != grf_file_get_size(f)) {
		tmp.remove();
		return;
	}
	tmp.close();
	QSound snd(tmp.fileName());
	snd.play();
#ifndef __WIN32
	while(1) {
		QCoreApplication::processEvents();
		if (snd.isFinished()) break;
		if (!mb.isVisible()) break;
		usleep(10000);
	}
	snd.stop();
#endif
	tmp.remove();
}

void MainWindow::on_viewSearch_doubleClicked(const QModelIndex idx) {
	this->doOpenFileById(ui.viewSearch->topLevelItem(idx.row())->text(0).toInt());
}

static QTreeWidgetItem *getFilestreeItemRecursive(const QModelIndex idx, const QTreeWidget *cur) {
	const QTreeWidgetItem *bla;
	if (idx.parent() != QModelIndex()) {
		bla = getFilestreeItemRecursive(idx.parent(), cur);
		return bla->child(idx.row());
	}
	return cur->topLevelItem(idx.row());
}

static QTreeWidgetItem *getFilestreeItemRecursive(const QModelIndex idx, const QTreeWidgetItem *cur) {
	const QTreeWidgetItem *bla = cur;
	if (idx.parent() != QModelIndex()) {
		bla = getFilestreeItemRecursive(idx.parent(), cur);
	}
	return bla->child(idx.row());
}

void MainWindow::on_view_filestree_doubleClicked(const QModelIndex idx) {
	QTreeWidgetItem *item = getFilestreeItemRecursive(idx, ui.view_filestree);
	if (item->text(2).toInt() == -1) return;
	this->doOpenFileById(item->text(2).toInt());
}

void MainWindow::on_view_allfiles_doubleClicked(const QModelIndex idx) {
	// item = ui.view_allfiles->currentItem()
	// item = ui.view_allfiles->topLevelItem(idx.row())
//	int id=ui.view_allfiles->topLevelItem(idx.row())->text(0).toInt();
//	printf("x=%d\n", ui.view_allfiles->topLevelItem(idx.row())->text(0).toInt());
	this->doOpenFileById(ui.view_allfiles->topLevelItem(idx.row())->text(0).toInt());
}

void MainWindow::doOpenFileById(int id) {
	void *f = grf_get_file_by_id(this->grf, id);
	bool is_image = false;
	QString name(grf_file_get_filename(f));
	name = name.toLower();
	if (name.endsWith(".wav")) return this->do_display_wav(grf_get_file_by_id(this->grf, id));
	if (
			(!name.endsWith(".bmp"))
		&&	(!name.endsWith(".jpg"))
		&&	(!name.endsWith(".jpeg"))
		&&	(!name.endsWith(".png"))
		&&	(!name.endsWith(".gif"))
		&&	(!name.endsWith(".gat"))
		) return;
	QByteArray im_data(grf_file_get_size(f), 0);
	if (grf_file_get_contents(f, im_data.data()) != grf_file_get_size(f)) return;
	QImage im;
	if (name.endsWith(".gat")) {
		const char *data = im_data.constData();
		int sx = *(int*)(data+6);
		int sy = *(int*)(data+10);
		im = QImage(sx, sy, QImage::Format_Indexed8);
		im.setNumColors(256);
		for(int i=0;i<255;i++) im.setColor(i, qRgb(i,i,i));
		im.setColor(255, qRgb(255,0,0));
		for(int y=0;y<sy;y++) {
			for(int x=0;x<sx;x++) {
//				int type = *(int*)(data + (((y*sx + x) * 20)+14+16));
//				unsigned char height = (int)(*(float*)(data + (((y*sx + x) * 20)+14+12)) + 128);
				float height = 0;
				for(int i=0;i<16;i+=4) height += *(float*)(data + (((y*sx + x) * 20)+14+i));
				height=255-((height/4)+127);
				if (height<0) height = 0;
				if (height>=255) height = 254;
//				im.setPixel(x,sy-y-1, type?0:255);
				im.setPixel(x,sy-y-1, (int)height);
			}
		}
	} else {
		if (!im.loadFromData(im_data)) return;
		is_image = true;
	}

	if (this->image_viewer) delete this->image_viewer;

	QLabel *label;
	QHBoxLayout *hboxLayout;
	QVBoxLayout *vboxLayout;
	QSpacerItem *spacerItem;
	QPushButton *closeButton;
	QDialog *Dialog = new QDialog;

	Dialog->setObjectName(QString::fromUtf8("Dialog"));
	Dialog->resize(QSize(16, 16).expandedTo(Dialog->minimumSizeHint()));
	vboxLayout = new QVBoxLayout(Dialog);
	vboxLayout->setSpacing(6);
	vboxLayout->setMargin(9);
	vboxLayout->setObjectName(QString::fromUtf8("vboxLayout"));
	label = new QLabel(Dialog);
	label->setObjectName(QString::fromUtf8("label"));

	vboxLayout->addWidget(label);

	hboxLayout = new QHBoxLayout();
	hboxLayout->setSpacing(6);
	hboxLayout->setMargin(0);
	hboxLayout->setObjectName(QString::fromUtf8("hboxLayout"));
	spacerItem = new QSpacerItem(131, 31, QSizePolicy::Expanding, QSizePolicy::Minimum);

	hboxLayout->addItem(spacerItem);

	closeButton = new QPushButton(Dialog);
	closeButton->setObjectName(QString::fromUtf8("closeButton"));

	hboxLayout->addWidget(closeButton);

	vboxLayout->addLayout(hboxLayout);

	QObject::connect(closeButton, SIGNAL(clicked()), Dialog, SLOT(reject()));

	Dialog->setWindowTitle(QApplication::translate("Dialog", "Image Preview", 0, QApplication::UnicodeUTF8));
	closeButton->setText(QApplication::translate("Dialog", "Close", 0, QApplication::UnicodeUTF8));
	QPixmap im_pixmap(QPixmap::fromImage(im));
	if ((ui.actionImages_Transparency->isChecked()) && (is_image)) {
		QImage mask(im.width(), im.height(), QImage::Format_MonoLSB);
		for(int x=im.width()-1; x>=0; x--) {
			for (int y=im.height()-1; y>=0;y--) {
				mask.setPixel(x, y, (im.pixel(x,y) & RGB_MASK) == 0xff00ff);
			}
		}
		im_pixmap.setMask(QBitmap::fromImage(mask));
	}
	label->setPixmap(im_pixmap);

	Dialog->show();
	this->image_viewer = Dialog;
};

void MainWindow::DoUpdateFilter(QString text) {
	if (this->last_search == text) return;
	this->last_search = text;
	QTreeWidgetItemIterator it(ui.view_allfiles, QTreeWidgetItemIterator::NoChildren);
	QRegExp r(text, Qt::CaseInsensitive, QRegExp::Wildcard);
	bool match;
	int i=0, max=grf_filecount(this->grf);
	ui.viewSearch->clear();
	while (*it) {
		void *cur_file = grf_get_file_by_id(this->grf, (*it)->text(0).toUInt());
		match = r.exactMatch(QString::fromUtf8(euc_kr_to_utf8(grf_file_get_filename(cur_file))));
		if (!match) match = r.exactMatch(QString::fromUtf8(euc_kr_to_utf8(grf_file_get_basename(cur_file))));
		if (match) {
			QTreeWidgetItem *__item = new QTreeWidgetItem(ui.viewSearch);
			__item->setText(0, (*it)->text(0));
			__item->setText(1, (*it)->text(1)); // compsize
			__item->setText(2, (*it)->text(2)); // realsize
			__item->setText(3, (*it)->text(3)); // pos
			__item->setText(4, (*it)->text(4)); // filename
		}
		ui.progbar->setValue((i++) * 100 / max);
		++it;
	}
	ui.viewSearch->sortItems(4, Qt::AscendingOrder);
	ui.progbar->setValue(100);
}

void MainWindow::on_listFilter_currentIndexChanged(QString text) {
	this->DoUpdateFilter(text);
	ui.tab_sel->setCurrentIndex(2);
}

void MainWindow::setRepackType(int type) {
	this->repack_type = type;
	ui.actionMove_files->setChecked(type == GRF_REPACK_FAST);
	ui.actionDecrypt->setChecked(type == GRF_REPACK_DECRYPT);
	ui.actionRecompress->setChecked(type == GRF_REPACK_RECOMPRESS);
}

void MainWindow::on_actionMove_files_triggered() { this->setRepackType(GRF_REPACK_FAST); }
void MainWindow::on_actionDecrypt_triggered() { this->setRepackType(GRF_REPACK_DECRYPT); }
void MainWindow::on_actionRecompress_triggered() { this->setRepackType(GRF_REPACK_RECOMPRESS); }

void MainWindow::setCompressionLevel(int lvl) {
	this->compression_level = lvl;
	if (this->grf != NULL) grf_set_compression_level(this->grf, this->compression_level);
	ui.actionC0->setChecked(lvl==0);
	ui.actionC1->setChecked(lvl==1);
	ui.actionC2->setChecked(lvl==2);
	ui.actionC3->setChecked(lvl==3);
	ui.actionC4->setChecked(lvl==4);
	ui.actionC5->setChecked(lvl==5);
	ui.actionC6->setChecked(lvl==6);
	ui.actionC7->setChecked(lvl==7);
	ui.actionC8->setChecked(lvl==8);
	ui.actionC9->setChecked(lvl==9);
}

void MainWindow::on_actionC0_triggered() { this->setCompressionLevel(0); }
void MainWindow::on_actionC1_triggered() { this->setCompressionLevel(1); }
void MainWindow::on_actionC2_triggered() { this->setCompressionLevel(2); }
void MainWindow::on_actionC3_triggered() { this->setCompressionLevel(3); }
void MainWindow::on_actionC4_triggered() { this->setCompressionLevel(4); }
void MainWindow::on_actionC5_triggered() { this->setCompressionLevel(5); }
void MainWindow::on_actionC6_triggered() { this->setCompressionLevel(6); }
void MainWindow::on_actionC7_triggered() { this->setCompressionLevel(7); }
void MainWindow::on_actionC8_triggered() { this->setCompressionLevel(8); }
void MainWindow::on_actionC9_triggered() { this->setCompressionLevel(9); }

