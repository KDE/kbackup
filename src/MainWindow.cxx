//**************************************************************************
//   (c) 2006 - 2009 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#include <MainWindow.hxx>
#include <Selector.hxx>
#include <Archiver.hxx>
#include <MainWidget.hxx>
#include <SettingsDialog.hxx>

#include <qsplitter.h>
#include <qspinbox.h>
#include <qtooltip.h>
#include <qmovie.h>
#include <qtimer.h>
#include <qcursor.h>
#include <qcheckbox.h>
#include <QTextStream>
#include <QMenu>

#include <kapplication.h>
#include <kstandardaction.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <ktoggleaction.h>
#include <krecentfilesaction.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <klineedit.h>
#include <kio/global.h>
#include <ksystemtrayicon.h>
#include <kglobal.h>
#include <kaboutdata.h>
#include <kiconloader.h>
#include <kstringhandler.h>
#include <kurl.h>

//#include <iostream>
//using namespace std;
//--------------------------------------------------------------------------------

MainWindow::MainWindow()
  : autorun(false)
{
  new Archiver(this);

  KStandardAction::quit(this, SLOT(maybeQuit()), actionCollection());

  KAction *action;

  action = actionCollection()->addAction("newProfile", this, SLOT(newProfile()));
  action->setText(i18n("New Profile"));
  action->setIcon(KIcon("document-new"));

  action = actionCollection()->addAction("loadProfile", this, SLOT(loadProfile()));
  action->setText(i18n("Load Profile"));
  action->setIcon(KIcon("document-open"));

  action = actionCollection()->addAction("saveProfile", this, SLOT(saveProfile()));
  action->setText(i18n("Save Profile"));
  action->setIcon(KIcon("document-save"));

  action = actionCollection()->addAction("saveProfileAs", this, SLOT(saveProfileAs()));
  action->setText(i18n("Save Profile As..."));
  action->setIcon(KIcon("document-save-as"));

  action = actionCollection()->addAction("profileSettings", this, SLOT(profileSettings()));
  action->setText(i18n("Profile Settings"));

  action = actionCollection()->addAction("enableAllMessages", this, SLOT(enableAllMessages()));
  action->setText(i18n("Enable All Messages"));

  docked = new KToggleAction(i18n("Dock in System Tray"), this);
  actionCollection()->addAction("dockInSysTray", docked);
  connect(docked, SIGNAL(triggered()), this, SLOT(dockInSysTray()));
  docked->setChecked(KGlobal::config()->group("").readEntry<bool>("dockInSysTray", false));

  recentFiles = KStandardAction::openRecent(this, SLOT(recentProfileSelected(const KUrl &)), actionCollection());
  recentFiles->setObjectName("recentProfiles");
  recentFiles->loadEntries(KGlobal::config()->group(""));

  createGUI();

  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

  selector = new Selector(splitter);

  mainWidget = new MainWidget(splitter);
  mainWidget->setSelector(selector);
  splitter->setCollapsible(splitter->indexOf(mainWidget), false);

  setCentralWidget(splitter);

  // save/restore window settings and size
  setAutoSaveSettings();

  // system tray icon
  sysTray = new KSystemTrayIcon(this);
  sysTray->setIcon(sysTray->loadIcon("kbackup"));
  sysTray->setVisible(docked->isChecked());

  connect(sysTray, SIGNAL(quitSelected()), this, SLOT(maybeQuit()));

  connect(Archiver::instance, SIGNAL(totalFilesChanged(int)), this, SLOT(changeSystrayTip()));
  connect(Archiver::instance, SIGNAL(logging(const QString &)), this, SLOT(loggingSlot(const QString &)));
  connect(Archiver::instance, SIGNAL(inProgress(bool)), this, SLOT(inProgress(bool)));

  startBackupAction = actionCollection()->addAction("startBackup", mainWidget, SLOT(startBackup()));
  startBackupAction->setIcon(KIcon("kbackup_start"));
  startBackupAction->setText(i18n("Start Backup"));
  sysTray->contextMenu()->addAction(startBackupAction);

  cancelBackupAction = actionCollection()->addAction("cancelBackup", Archiver::instance, SLOT(cancel()));
  cancelBackupAction->setText(i18n("Cancel Backup"));
  cancelBackupAction->setIcon(KIcon("kbackup_cancel"));
  cancelBackupAction->setEnabled(false);
  sysTray->contextMenu()->addAction(cancelBackupAction);

  changeSystrayTip();
}

//--------------------------------------------------------------------------------

void MainWindow::runBackup()
{
  autorun = true;
  QTimer::singleShot(0, mainWidget, SLOT(startBackup()));
}

//--------------------------------------------------------------------------------

bool MainWindow::stopAllowed()
{
  if ( Archiver::instance->isInProgress() )
  {
    if ( KMessageBox::warningYesNo(this,
            i18n("There is a backup in progress. Do you want to abort it?")) == KMessageBox::No )
      return false;

    Archiver::instance->cancel();
  }

  return true;
}

//--------------------------------------------------------------------------------

void MainWindow::maybeQuit()
{
  if ( stopAllowed() )
    kapp->quit();
}

//--------------------------------------------------------------------------------

bool MainWindow::queryClose()
{
  if ( kapp->sessionSaving() || !sysTray->isVisible() )
    return stopAllowed();

  hide();
  return false;
}

//--------------------------------------------------------------------------------

bool MainWindow::queryExit()
{
  return stopAllowed();
}

//--------------------------------------------------------------------------------

void MainWindow::recentProfileSelected(const KUrl &url)
{
  loadProfile(url.path());
}

//--------------------------------------------------------------------------------

void MainWindow::loadProfile()
{
  QString fileName = KFileDialog::getOpenFileName(KUrl("kfiledialog:///profile"),
                                                  "*.kbp|" + i18n("KBackup Profile (*.kbp)"));

  if ( fileName.isEmpty() ) return;

  loadProfile(fileName);
}

//--------------------------------------------------------------------------------

void MainWindow::loadProfile(const QString &fileName, bool adaptTreeWidth)
{
  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

  QStringList includes, excludes;
  QString error;

  if ( ! Archiver::instance->loadProfile(fileName, includes, excludes, error) )
  {
    QApplication::restoreOverrideCursor();

    KMessageBox::error(this,
                i18n("Could not open profile '%1' for reading: %2")
                     .arg(fileName)
                     .arg(error),
                i18n("Open failed"));
    return;
  }

  setLoadedProfile(fileName);

  KUrl url;
  url.setPath(fileName);
  recentFiles->addUrl(url);
  recentFiles->saveEntries(KGlobal::config()->group(""));
  KGlobal::config()->group("").sync();

  // now fill the Selector tree with those settings
  selector->setBackupList(includes, excludes);

  mainWidget->getTargetLineEdit()->setText(Archiver::instance->getTarget().pathOrUrl());

  if ( adaptTreeWidth )
    selector->adjustColumn(0);

  QApplication::restoreOverrideCursor();
}

//--------------------------------------------------------------------------------

void MainWindow::saveProfileAs()
{
  QString fileName = KFileDialog::getSaveFileName(KUrl("kfiledialog:///profile"),
                                                  "*.kbp|" + i18n("KBackup Profile (*.kbp)"));

  if ( fileName.isEmpty() ) return;

  saveProfile(fileName);
}

//--------------------------------------------------------------------------------

void MainWindow::saveProfile(QString fileName)
{
  if ( fileName.isEmpty() )
    fileName = loadedProfile;

  if ( fileName.isEmpty() )
  {
    fileName = KFileDialog::getSaveFileName(KUrl("kfiledialog:///profile"),
                                            "*.kbp|" + i18n("KBackup Profile (*.kbp)"));

    if ( fileName.isEmpty() ) return;
  }

  QFile file(fileName);

  if ( file.exists() && (fileName != loadedProfile) )
  {
    if ( KMessageBox::warningYesNo(this,
                i18n("The profile '%1' does already exist.\n"
                     "Do you want to overwrite it?")
                     .arg(fileName),
                i18n("Profile exists")) == KMessageBox::No )
      return;
  }

  QStringList includes, excludes;
  selector->getBackupList(includes, excludes);

  if ( ! file.open(QIODevice::WriteOnly) )
  {
    KMessageBox::error(this,
                i18n("Could not open profile '%1' for writing: %2")
                     .arg(fileName)
                     .arg(file.errorString()),
                i18n("Open failed"));
    return;
  }

  QTextStream stream(&file);

  stream << "M " << mainWidget->getTargetLineEdit()->text() << endl;
  stream << "P " << Archiver::instance->getFilePrefix() << endl;
  stream << "S " << Archiver::instance->getMaxSliceMBs() << endl;
  stream << "R " << Archiver::instance->getKeptBackups() << endl;
  stream << "C " << static_cast<int>(Archiver::instance->getMediaNeedsChange()) << endl;
  stream << "Z " << static_cast<int>(Archiver::instance->getCompressFiles()) << endl;

  for (QStringList::const_iterator it = includes.begin(); it != includes.end(); ++it)
    stream << "I " << *it << endl;

  for (QStringList::const_iterator it = excludes.begin(); it != excludes.end(); ++it)
    stream << "E " << *it << endl;

  file.close();

  setLoadedProfile(fileName);
}

//--------------------------------------------------------------------------------

void MainWindow::profileSettings()
{
  SettingsDialog dialog(this);

  dialog.ui.prefix->setText(Archiver::instance->getFilePrefix());
  dialog.setMaxMB(Archiver::instance->getMaxSliceMBs());
  dialog.ui.numBackups->setValue(Archiver::instance->getKeptBackups());
  dialog.ui.mediaNeedsChange->setChecked(Archiver::instance->getMediaNeedsChange());
  dialog.ui.compressFiles->setChecked(Archiver::instance->getCompressFiles());

  if ( dialog.exec() == QDialog::Accepted )
  {
    Archiver::instance->setFilePrefix(dialog.ui.prefix->text().trimmed());
    Archiver::instance->setMaxSliceMBs(dialog.ui.maxSliceSize->value());
    Archiver::instance->setKeptBackups(dialog.ui.numBackups->value());
    Archiver::instance->setMediaNeedsChange(dialog.ui.mediaNeedsChange->isChecked());
    Archiver::instance->setCompressFiles(dialog.ui.compressFiles->isChecked());
  }
}

//--------------------------------------------------------------------------------

void MainWindow::newProfile()
{
  Archiver::instance->setFilePrefix("");  // back to default
  Archiver::instance->setMaxSliceMBs(Archiver::UNLIMITED);
  Archiver::instance->setMediaNeedsChange(true);
  Archiver::instance->setTarget(KUrl());
  Archiver::instance->setKeptBackups(Archiver::UNLIMITED);

  // clear selection
  QStringList includes, excludes;
  selector->setBackupList(includes, excludes);

  mainWidget->getTargetLineEdit()->setText("");

  setLoadedProfile("");
}

//--------------------------------------------------------------------------------

void MainWindow::loggingSlot(const QString &message)
{
  lastLog = message;
  changeSystrayTip();
}

//--------------------------------------------------------------------------------

void MainWindow::changeSystrayTip()
{
  QString text = KGlobal::mainComponent().aboutData()->programName() + " - " +
                 i18n("Files: %1 Size: %2 MB\n%3")
                    .arg(Archiver::instance->getTotalFiles())
                    .arg(QString::number(Archiver::instance->getTotalBytes() / 1024.0 / 1024.0, 'f', 2))
                    .arg(KStringHandler::csqueeze(lastLog, 60));

  sysTray->setToolTip(text);
}

//--------------------------------------------------------------------------------

void MainWindow::inProgress(bool runs)
{
  if ( runs )
  {
#if KDE_IS_VERSION(4,2,0)
    QMovie *movie = KIconLoader::global()->loadMovie("kbackup_runs", KIconLoader::Panel);
    if ( movie )
      sysTray->setMovie(movie);
#endif

    startBackupAction->setEnabled(false);
    cancelBackupAction->setEnabled(true);
  }
  else
  {
    sysTray->setIcon(sysTray->loadIcon("kbackup"));
    startBackupAction->setEnabled(true);
    cancelBackupAction->setEnabled(false);

    if ( autorun )
      kapp->quit();
  }
}

//--------------------------------------------------------------------------------

void MainWindow::dockInSysTray()
{
  KGlobal::config()->group("").writeEntry("dockInSysTray", docked->isChecked());
  KGlobal::config()->group("").sync();

  sysTray->setVisible(docked->isChecked());
}

//--------------------------------------------------------------------------------

void MainWindow::enableAllMessages()
{
  KMessageBox::enableAllMessages();
}

//--------------------------------------------------------------------------------

void MainWindow::setLoadedProfile(const QString &name)
{
  loadedProfile = name;
  setCaption(name);
}

//--------------------------------------------------------------------------------
