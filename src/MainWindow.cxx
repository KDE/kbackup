//**************************************************************************
//   Copyright 2006 - 2017 Martin Koller, kollix@aon.at
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
#include <qtimer.h>
#include <qcursor.h>
#include <qcheckbox.h>
#include <QTextStream>
#include <QMenu>
#include <QAction>
#include <QUrl>
#include <QApplication>
#include <QFileDialog>
#include <QDebug>
#include <QHeaderView>

#include <KXMLGUIFactory>
#include <kstandardaction.h>
#include <kactioncollection.h>
#include <ktoggleaction.h>
#include <krecentfilesaction.h>
#include <kmessagebox.h>
#include <kio/global.h>
#include <kstringhandler.h>
#include <KStatusNotifierItem>
#include <KSharedConfig>
#include <KConfigGroup>
#include <kshortcutsdialog.h>

//#include <iostream>
//using namespace std;
//--------------------------------------------------------------------------------

MainWindow::MainWindow()
  : sysTray(0), autorun(false)
{
  new Archiver(this);

  quitAction = KStandardAction::quit(this, SLOT(maybeQuit()), actionCollection());
  KStandardAction::keyBindings(guiFactory(), SLOT(configureShortcuts()), actionCollection());

  QAction *action;

  action = actionCollection()->addAction("newProfile", this, SLOT(newProfile()));
  action->setText(i18n("New Profile"));
  action->setIcon(QIcon::fromTheme("document-new"));

  action = actionCollection()->addAction("loadProfile", this, SLOT(loadProfile()));
  action->setText(i18n("Load Profile..."));
  action->setIcon(QIcon::fromTheme("document-open"));

  action = actionCollection()->addAction("saveProfile", this, SLOT(saveProfile()));
  action->setText(i18n("Save Profile"));
  action->setIcon(QIcon::fromTheme("document-save"));

  action = actionCollection()->addAction("saveProfileAs", this, SLOT(saveProfileAs()));
  action->setText(i18n("Save Profile As..."));
  action->setIcon(QIcon::fromTheme("document-save-as"));

  action = actionCollection()->addAction("profileSettings", this, SLOT(profileSettings()));
  action->setText(i18n("Profile Settings..."));

  action = actionCollection()->addAction("enableAllMessages", this, SLOT(enableAllMessages()));
  action->setText(i18n("Enable All Messages"));

  KToggleAction *docked = new KToggleAction(i18n("Dock in System Tray"), this);
  actionCollection()->addAction("dockInSysTray", docked);
  connect(docked, &QAction::toggled, this, &MainWindow::dockInSysTray);

  KToggleAction *showHidden = new KToggleAction(i18n("Show Hidden Files"), this);
  actionCollection()->addAction("showHiddenFiles", showHidden);
  connect(showHidden, &QAction::toggled, this, &MainWindow::showHiddenFiles);

  recentFiles = KStandardAction::openRecent(this, SLOT(recentProfileSelected(const QUrl &)), actionCollection());
  recentFiles->setObjectName("recentProfiles");
  recentFiles->loadEntries(KSharedConfig::openConfig()->group(""));

  createGUI();

  splitter = new QSplitter(Qt::Horizontal, this);

  selector = new Selector(splitter, actionCollection());

  mainWidget = new MainWidget(splitter);
  mainWidget->setSelector(selector);
  splitter->setCollapsible(splitter->indexOf(mainWidget), false);

  setCentralWidget(splitter);

  splitter->restoreState(KSharedConfig::openConfig()->group("geometry").readEntry<QByteArray>("splitter", QByteArray()));
  selector->header()->restoreState(KSharedConfig::openConfig()->group("geometry").readEntry<QByteArray>("tree", QByteArray()));

  // save/restore window settings and size
  setAutoSaveSettings();

  connect(Archiver::instance, SIGNAL(totalFilesChanged(int)), this, SLOT(changeSystrayTip()));
  connect(Archiver::instance, SIGNAL(logging(const QString &)), this, SLOT(loggingSlot(const QString &)));
  connect(Archiver::instance, SIGNAL(inProgress(bool)), this, SLOT(inProgress(bool)));

  startBackupAction = actionCollection()->addAction("startBackup", mainWidget, SLOT(startBackup()));
  startBackupAction->setIcon(QIcon::fromTheme("kbackup_start"));
  startBackupAction->setText(i18n("Start Backup"));

  cancelBackupAction = actionCollection()->addAction("cancelBackup", Archiver::instance, SLOT(cancel()));
  cancelBackupAction->setText(i18n("Cancel Backup"));
  cancelBackupAction->setIcon(QIcon::fromTheme("kbackup_cancel"));
  cancelBackupAction->setEnabled(false);

  showHidden->setChecked(KSharedConfig::openConfig()->group("settings").readEntry<bool>("showHiddenFiles", false));
  showHiddenFiles(showHidden->isChecked());
  docked->setChecked(KSharedConfig::openConfig()->group("settings").readEntry<bool>("dockInSysTray", false));
  dockInSysTray(docked->isChecked());

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

  KSharedConfig::openConfig()->group("geometry").writeEntry("splitter", splitter->saveState());
  KSharedConfig::openConfig()->group("geometry").writeEntry("tree", selector->header()->saveState());

  return true;
}

//--------------------------------------------------------------------------------

void MainWindow::maybeQuit()
{
  if ( stopAllowed() )
    qApp->quit();
}

//--------------------------------------------------------------------------------

bool MainWindow::queryClose()
{
  if ( qApp->isSavingSession() || !sysTray )
    return stopAllowed();

  hide();
  return false;
}

//--------------------------------------------------------------------------------

void MainWindow::recentProfileSelected(const QUrl &url)
{
  loadProfile(url.path());
}

//--------------------------------------------------------------------------------

void MainWindow::loadProfile()
{
  QString fileName = QFileDialog::getOpenFileName(this, i18n("Select Profile"), QString(),
                                                  i18n("KBackup Profile (*.kbp)"));

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
                i18n("Could not open profile '%1' for reading: %2",
                     fileName,
                     error),
                i18n("Open failed"));
    return;
  }

  setLoadedProfile(fileName);

  // now fill the Selector tree with those settings
  selector->setBackupList(includes, excludes);

  mainWidget->getTargetLineEdit()->setText(Archiver::instance->getTarget().toDisplayString(QUrl::PreferLocalFile));

  if ( adaptTreeWidth )
    selector->resizeColumnToContents(0);

  QApplication::restoreOverrideCursor();
}

//--------------------------------------------------------------------------------

void MainWindow::saveProfileAs()
{
  QString fileName = QFileDialog::getSaveFileName(this, i18n("Select Profile"), QString(),
                                                  i18n("KBackup Profile (*.kbp)"));

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
    fileName = QFileDialog::getSaveFileName(this, i18n("Select Profile"), QString(),
                                            i18n("KBackup Profile (*.kbp)"));

    if ( fileName.isEmpty() ) return;
  }

  QFile file(fileName);

  if ( file.exists() && (fileName != loadedProfile) )
  {
    if ( KMessageBox::warningYesNo(this,
                i18n("The profile '%1' does already exist.\n"
                     "Do you want to overwrite it?",
                     fileName),
                i18n("Profile exists")) == KMessageBox::No )
      return;
  }

  QStringList includes, excludes;
  selector->getBackupList(includes, excludes);
  QString error;

  Archiver::instance->setTarget(QUrl(mainWidget->getTargetLineEdit()->text()));

  if ( ! Archiver::instance->saveProfile(fileName, includes, excludes, error) )
  {
    KMessageBox::error(this,
                i18n("Could not open profile '%1' for writing: %2",
                     fileName,
                     error),
                i18n("Open failed"));
    return;
  }

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
  dialog.ui.fullBackupInterval->setValue(Archiver::instance->getFullBackupInterval());
  dialog.ui.filter->setText(Archiver::instance->getFilter());
  dialog.ui.dirFilter->setPlainText(Archiver::instance->getDirFilter());

  if ( dialog.exec() == QDialog::Accepted )
  {
    Archiver::instance->setFilePrefix(dialog.ui.prefix->text().trimmed());
    Archiver::instance->setMaxSliceMBs(dialog.ui.maxSliceSize->value());
    Archiver::instance->setKeptBackups(dialog.ui.numBackups->value());
    Archiver::instance->setMediaNeedsChange(dialog.ui.mediaNeedsChange->isChecked());
    Archiver::instance->setCompressFiles(dialog.ui.compressFiles->isChecked());
    Archiver::instance->setFullBackupInterval(dialog.ui.fullBackupInterval->value());
    Archiver::instance->setFilter(dialog.ui.filter->text());
    Archiver::instance->setDirFilter(dialog.ui.dirFilter->toPlainText());
  }
}

//--------------------------------------------------------------------------------

void MainWindow::newProfile()
{
  Archiver::instance->setFilePrefix("");  // back to default
  Archiver::instance->setMaxSliceMBs(Archiver::UNLIMITED);
  Archiver::instance->setMediaNeedsChange(true);
  Archiver::instance->setTarget(QUrl());
  Archiver::instance->setKeptBackups(Archiver::UNLIMITED);
  Archiver::instance->setFullBackupInterval(1);
  Archiver::instance->setFilter("");
  Archiver::instance->setDirFilter("");

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
  if ( !sysTray )
    return;

  QString text = qApp->applicationDisplayName() + " - " +
                 i18n("Files: %1 Size: %2 MB\n%3",
                    Archiver::instance->getTotalFiles(),
                    QString::number(Archiver::instance->getTotalBytes() / 1024.0 / 1024.0, 'f', 2),
                    KStringHandler::csqueeze(lastLog, 60));

  sysTray->setToolTip(QLatin1String("kbackup"), QLatin1String("kbackup"), text);
}

//--------------------------------------------------------------------------------

void MainWindow::inProgress(bool runs)
{
  if ( runs )
  {
    if ( sysTray )
    {
      /*
      QMovie *movie = KIconLoader::global()->loadMovie("kbackup_runs", KIconLoader::Panel);
      if ( movie )
      {
        sysTray->setMovie(movie);
        movie->start();
      }
      */
      sysTray->setIconByName("kbackup_runs");
      sysTray->setStatus(KStatusNotifierItem::Active);
    }

    startBackupAction->setEnabled(false);
    cancelBackupAction->setEnabled(true);
  }
  else
  {
    if ( sysTray )
    {
      /*
      if ( sysTray->movie() )
        const_cast<QMovie*>(sysTray->movie())->stop();  // why does it return a const pointer ? :-(

      sysTray->setIcon(sysTray->loadIcon("kbackup"));
      */
      sysTray->setIconByName("kbackup");
      sysTray->setStatus(KStatusNotifierItem::Passive);
    }

    startBackupAction->setEnabled(true);
    cancelBackupAction->setEnabled(false);

    if ( autorun )
      qApp->quit();
  }
}

//--------------------------------------------------------------------------------

void MainWindow::dockInSysTray(bool checked)
{
  KSharedConfig::openConfig()->group("settings").writeEntry("dockInSysTray", checked);
  KSharedConfig::openConfig()->group("settings").sync();

  if ( checked )
  {
    // system tray icon
    delete sysTray;
    sysTray = new KStatusNotifierItem(this);
    sysTray->setStandardActionsEnabled(false);

    sysTray->contextMenu()->addAction(startBackupAction);
    sysTray->contextMenu()->addAction(cancelBackupAction);
    sysTray->contextMenu()->addAction(quitAction);

    if ( Archiver::instance->isInProgress() )
    {
      sysTray->setStatus(KStatusNotifierItem::Active);
      sysTray->setIconByName("kbackup_runs");
    }
    else
    {
      sysTray->setStatus(KStatusNotifierItem::Passive);
      sysTray->setIconByName("kbackup");
    }
  }
  else
  {
    delete sysTray;
    sysTray = 0;
  }
}

//--------------------------------------------------------------------------------

void MainWindow::showHiddenFiles(bool checked)
{
  KSharedConfig::openConfig()->group("settings").writeEntry("showHiddenFiles", checked);
  KSharedConfig::openConfig()->group("settings").sync();

  selector->setShowHiddenFiles(checked);
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
  Archiver::instance->setLoadedProfile(name);
  setCaption(name);

  if ( !name.isEmpty() )
  {
    QUrl url;
    url.setPath(name);
    recentFiles->addUrl(url);
    recentFiles->saveEntries(KSharedConfig::openConfig()->group(""));
    KSharedConfig::openConfig()->group("").sync();
  }
}

//--------------------------------------------------------------------------------
