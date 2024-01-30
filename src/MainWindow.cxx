//**************************************************************************
//   Copyright 2006 - 2022 Martin Koller, kollix@aon.at
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

#include <QSplitter>
#include <QSpinBox>
#include <QToolTip>
#include <QTimer>
#include <QCursor>
#include <QCheckBox>
#include <QMenu>
#include <QAction>
#include <QUrl>
#include <QApplication>
#include <QFileDialog>
#include <QHeaderView>

#include <KXMLGUIFactory>
#include <KStandardAction>
#include <KActionCollection>
#include <KToggleAction>
#include <KRecentFilesAction>
#include <KMessageBox>
#include <kio/global.h>
#include <KStringHandler>
#include <KStatusNotifierItem>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KShortcutsDialog>

//#include <iostream>
//using namespace std;
//--------------------------------------------------------------------------------

MainWindow::MainWindow()
{
  new Archiver(this);

  quitAction = KStandardAction::quit(this, SLOT(maybeQuit()), actionCollection());
  KStandardAction::keyBindings(guiFactory(), &KXMLGUIFactory::showConfigureShortcutsDialog, actionCollection());


  QAction *action;

  action = actionCollection()->addAction(QStringLiteral("newProfile"), this, SLOT(newProfile()));
  action->setText(i18n("New Profile"));
  action->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));

  action = actionCollection()->addAction(QStringLiteral("loadProfile"), this, SLOT(loadProfile()));
  action->setText(i18n("Load Profile..."));
  action->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));

  action = actionCollection()->addAction(QStringLiteral("saveProfile"), this, SLOT(saveProfile()));
  action->setText(i18n("Save Profile"));
  action->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));

  action = actionCollection()->addAction(QStringLiteral("saveProfileAs"), this, SLOT(saveProfileAs()));
  action->setText(i18n("Save Profile As..."));
  action->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));

  action = actionCollection()->addAction(QStringLiteral("profileSettings"), this, SLOT(profileSettings()));
  action->setText(i18n("Profile Settings..."));

  action = actionCollection()->addAction(QStringLiteral("enableAllMessages"), this, SLOT(enableAllMessages()));
  action->setText(i18n("Enable All Messages"));

  KToggleAction *docked = new KToggleAction(i18n("Dock in System Tray"), this);
  actionCollection()->addAction(QStringLiteral("dockInSysTray"), docked);
  connect(docked, &QAction::toggled, this, &MainWindow::dockInSysTray);

  KToggleAction *showHidden = new KToggleAction(i18n("Show Hidden Files"), this);
  actionCollection()->addAction(QStringLiteral("showHiddenFiles"), showHidden);
  connect(showHidden, &QAction::toggled, this, &MainWindow::showHiddenFiles);

  recentFiles = KStandardAction::openRecent(this, SLOT(recentProfileSelected(const QUrl &)), actionCollection());
  recentFiles->setObjectName(QStringLiteral("recentProfiles"));
  recentFiles->loadEntries(KSharedConfig::openConfig()->group(QString()));

  createGUI();

  splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setContentsMargins(0, 1, 0, 0);

  selector = new Selector(splitter, actionCollection());

  mainWidget = new MainWidget(splitter);
  mainWidget->setSelector(selector);
  splitter->setCollapsible(splitter->indexOf(mainWidget), false);

  setCentralWidget(splitter);

  splitter->restoreState(KSharedConfig::openConfig()->group(QStringLiteral("geometry")).readEntry<QByteArray>("splitter", QByteArray()));
  selector->header()->restoreState(KSharedConfig::openConfig()->group(QStringLiteral("geometry")).readEntry<QByteArray>("tree", QByteArray()));

  // save/restore window settings and size
  setAutoSaveSettings();

  connect(Archiver::instance, &Archiver::totalFilesChanged, this, &MainWindow::changeSystrayTip);
  connect(Archiver::instance, &Archiver::logging, this, &MainWindow::loggingSlot);
  connect(Archiver::instance, &Archiver::inProgress, this, &MainWindow::inProgress);

  startBackupAction = actionCollection()->addAction(QStringLiteral("startBackup"), mainWidget, SLOT(startBackup()));
  startBackupAction->setIcon(QIcon::fromTheme(QStringLiteral("kbackup_start")));
  startBackupAction->setText(i18n("Start Backup"));

  cancelBackupAction = actionCollection()->addAction(QStringLiteral("cancelBackup"), Archiver::instance, SLOT(cancel()));
  cancelBackupAction->setText(i18n("Cancel Backup"));
  cancelBackupAction->setIcon(QIcon::fromTheme(QStringLiteral("kbackup_cancel")));
  cancelBackupAction->setEnabled(false);

  showHidden->setChecked(KSharedConfig::openConfig()->group(QStringLiteral("settings")).readEntry<bool>("showHiddenFiles", false));
  showHiddenFiles(showHidden->isChecked());
  docked->setChecked(KSharedConfig::openConfig()->group(QStringLiteral("settings")).readEntry<bool>("dockInSysTray", false));
  dockInSysTray(docked->isChecked());

  // for convenience, open the tree at the HOME directory
  selector->openHomeDir();

  changeSystrayTip();
}

//--------------------------------------------------------------------------------

void MainWindow::runBackup()
{
  autorun = true;
  QTimer::singleShot(0, mainWidget, &MainWidget::startBackup);
}

//--------------------------------------------------------------------------------

bool MainWindow::stopAllowed()
{
  if ( Archiver::instance->isInProgress() )
  {
    if ( KMessageBox::warningTwoActions(this,
            i18n("There is a backup in progress. Do you want to abort it?"), i18n("Backup in Progress"), KGuiItem(i18n("Abort")), KStandardGuiItem::cancel()) == KMessageBox::SecondaryAction )
      return false;

    Archiver::instance->cancel();
  }

  KSharedConfig::openConfig()->group(QStringLiteral("geometry")).writeEntry("splitter", splitter->saveState());
  KSharedConfig::openConfig()->group(QStringLiteral("geometry")).writeEntry("tree", selector->header()->saveState());

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
    if ( KMessageBox::warningTwoActions(this,
                i18n("The profile '%1' does already exist.\n"
                     "Do you want to overwrite it?",
                     fileName),
                i18n("Profile exists"), KStandardGuiItem::save(), KStandardGuiItem::discard()) == KMessageBox::SecondaryAction )
      return;
  }

  QStringList includes, excludes;
  selector->getBackupList(includes, excludes);
  QString error;

  Archiver::instance->setTarget(QUrl(mainWidget->getTargetLineEdit()->text()));

  // when manually saving a profile, we need to restart the backup (incremental) cycle
  // since e.g. adding a new dir would otherwise not save its content on the next run
  Archiver::instance->resetBackupCycle();

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
  Archiver::instance->setFilePrefix(QString());  // back to default
  Archiver::instance->setMaxSliceMBs(Archiver::UNLIMITED);
  Archiver::instance->setMediaNeedsChange(true);
  Archiver::instance->setTarget(QUrl());
  Archiver::instance->setKeptBackups(Archiver::UNLIMITED);
  Archiver::instance->setFullBackupInterval(1);
  Archiver::instance->setFilter(QString());
  Archiver::instance->setDirFilter(QString());

  // clear selection
  QStringList includes, excludes;
  selector->setBackupList(includes, excludes);

  mainWidget->getTargetLineEdit()->setText(QString());

  setLoadedProfile(QString());
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

  QString text = qApp->applicationDisplayName() + QLatin1String(" - ") +
                 i18n("Files: %1 Size: %2 MB\n%3",
                    Archiver::instance->getTotalFiles(),
                    QString::number(Archiver::instance->getTotalBytes() / 1024.0 / 1024.0, 'f', 2),
                    KStringHandler::csqueeze(lastLog, 60));

  sysTray->setToolTip(QStringLiteral("kbackup"), QStringLiteral("kbackup"), text);
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
      sysTray->setIconByName(QStringLiteral("kbackup_runs"));
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
      sysTray->setIconByName(QStringLiteral("kbackup"));
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
  KSharedConfig::openConfig()->group(QStringLiteral("settings")).writeEntry("dockInSysTray", checked);
  KSharedConfig::openConfig()->group(QStringLiteral("settings")).sync();

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
      sysTray->setIconByName(QStringLiteral("kbackup_runs"));
    }
    else
    {
      sysTray->setStatus(KStatusNotifierItem::Passive);
      sysTray->setIconByName(QStringLiteral("kbackup"));
    }
  }
  else
  {
    delete sysTray;
    sysTray = nullptr;
  }
}

//--------------------------------------------------------------------------------

void MainWindow::showHiddenFiles(bool checked)
{
  KSharedConfig::openConfig()->group(QStringLiteral("settings")).writeEntry("showHiddenFiles", checked);
  KSharedConfig::openConfig()->group(QStringLiteral("settings")).sync();

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
    recentFiles->saveEntries(KSharedConfig::openConfig()->group(QString()));
    KSharedConfig::openConfig()->group(QString()).sync();
  }
}

//--------------------------------------------------------------------------------

#include "moc_MainWindow.cpp"
