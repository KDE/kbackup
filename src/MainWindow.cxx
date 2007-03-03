/***************************************************************************
 *   (c) 2006, 2007 Martin Koller, m.koller@surfeu.at                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, version 2 of the License                *
 *                                                                         *
 ***************************************************************************/

#include <MainWindow.hxx>
#include <Selector.hxx>
#include <Archiver.hxx>
#include <MainWidget.hxx>
#include <SettingsDialog.h>

#include <qsplitter.h>
#include <qspinbox.h>
#include <qtooltip.h>
#include <qmovie.h>
#include <qtimer.h>
#include <qcursor.h>

#include <kapplication.h>
#include <kstdaction.h>
#include <kactionclasses.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <klineedit.h>
#include <kio/global.h>
#include <ksystemtray.h>
#include <kglobal.h>
#include <kaboutdata.h>
#include <kiconloader.h>
#include <kstringhandler.h>
#include <kpopupmenu.h>
#include <kurl.h>

//#include <iostream>
//using namespace std;
//--------------------------------------------------------------------------------

MainWindow::MainWindow()
  : KMainWindow(0, 0, 0), autorun(false)
{
  KStdAction::quit(this, SLOT(maybeQuit()), actionCollection());

  new KAction(i18n("New Profile"), "filenew", 0, this,
              SLOT(newProfile()), actionCollection(), "newProfile");

  new KAction(i18n("Load Profile"), "fileopen", 0, this,
              SLOT(loadProfile()), actionCollection(), "loadProfile");

  new KAction(i18n("Save Profile"), "filesave", 0, this,
              SLOT(saveProfile()), actionCollection(), "saveProfile");

  new KAction(i18n("Profile Settings"), "", 0, this,
              SLOT(profileSettings()), actionCollection(), "profileSettings");

  docked = new KToggleAction(i18n("Dock in System Tray"), KShortcut(), this,
                             SLOT(dockInSysTray()), actionCollection(), "dockInSysTray");

  docked->setChecked(KGlobal::instance()->config()->readBoolEntry("dockInSysTray", false));

  recentFiles = KStdAction::openRecent(this, SLOT(recentProfileSelected(const KURL &)),
                                       actionCollection(), "recentProfiles");
  recentFiles->loadEntries(KGlobal::instance()->config());

  createGUI();

  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

  selector = new Selector(splitter);

  mainWidget = new MainWidget(splitter);
  mainWidget->setSelector(selector);
  splitter->setCollapsible(mainWidget, false);

  setCentralWidget(splitter);

  // save/restore window settings and size
  setAutoSaveSettings();

  // system tray icon
  sysTray = new KSystemTray(this);
  sysTray->setPixmap(sysTray->loadIcon("kbackup"));
  sysTray->setShown(docked->isChecked());

  connect(sysTray, SIGNAL(quitSelected()), this, SLOT(maybeQuit()));

  connect(Archiver::instance, SIGNAL(totalFilesChanged(int)), this, SLOT(changeSystrayTip()));
  connect(Archiver::instance, SIGNAL(logging(const QString &)), this, SLOT(loggingSlot(const QString &)));
  connect(Archiver::instance, SIGNAL(inProgress(bool)), this, SLOT(inProgress(bool)));

  startBackupAction = new KAction(i18n("Start Backup"), "kbackup_start", 0, mainWidget,
                                  SLOT(startBackup()), actionCollection(), "startBackup");
  startBackupAction->plug(sysTray->contextMenu(), 1);

  cancelBackupAction = new KAction(i18n("Cancel Backup"), "kbackup_cancel", 0, Archiver::instance,
                                   SLOT(cancel()), actionCollection(), "cancelBackup");
  cancelBackupAction->plug(sysTray->contextMenu(), 2);
  cancelBackupAction->setEnabled(false);

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
  if ( kapp->sessionSaving() || !sysTray->isShown() )
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

void MainWindow::recentProfileSelected(const KURL &url)
{
  loadProfile(url.path());
}

//--------------------------------------------------------------------------------

void MainWindow::loadProfile()
{
  QString fileName = KFileDialog::getOpenFileName(":profile", "*.kbp|" + i18n("KBackup Profile (*.kbp)"));

  if ( fileName.isEmpty() ) return;

  loadProfile(fileName);
}

//--------------------------------------------------------------------------------

void MainWindow::loadProfile(const QString &fileName, bool adaptTreeWidth)
{
  QFile file(fileName);
  if ( ! file.open(IO_ReadOnly) )
  {
    KMessageBox::error(this,
                i18n("Could not open profile '%1' for reading: %2")
                     .arg(fileName)
                     .arg(kapp->translate("QFile", file.errorString())),
                i18n("Open failed"));
    return;
  }

  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

  KURL url;
  url.setPath(fileName);
  recentFiles->addURL(url);
  recentFiles->saveEntries(KGlobal::instance()->config());

  QStringList includes, excludes;
  QString target;
  QChar type, blank;
  QTextStream stream(&file);

  // back to default (in case old profile read which does not include these)
  Archiver::instance->setFilePrefix("");
  Archiver::instance->setMaxSliceMBs(Archiver::UNLIMITED);

  while ( ! stream.atEnd() )
  {
    stream.skipWhiteSpace();
    stream >> type;            // read a QChar without skipping whitespace
    stream >> blank;           // read a QChar without skipping whitespace

    if ( type == 'M' )
    {
      target = stream.readLine();  // include white space
    }
    else if ( type == 'P' )
    {
      QString prefix = stream.readLine();  // include white space
      Archiver::instance->setFilePrefix(prefix);
    }
    else if ( type == 'S' )
    {
      int max;
      stream >> max;
      Archiver::instance->setMaxSliceMBs(max);
    }
    else if ( type == 'I' )
    {
      includes.append(stream.readLine());
    }
    else if ( type == 'E' )
    {
      excludes.append(stream.readLine());
    }
  }

  file.close();

  // now fill the Selector tree with those settings
  selector->setBackupList(includes, excludes);

  mainWidget->targetDir->setText(target);
  Archiver::instance->setTarget(KURL::fromPathOrURL(target));

  if ( adaptTreeWidth )
    selector->adjustColumn(0);

  QApplication::restoreOverrideCursor();
}

//--------------------------------------------------------------------------------

void MainWindow::saveProfile()
{
  QString fileName = KFileDialog::getSaveFileName(":profile", "*.kbp|" + i18n("KBackup Profile (*.kbp)"));

  if ( fileName.isEmpty() ) return;

  QFile file(fileName);

  if ( file.exists() )
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

  if ( ! file.open(IO_WriteOnly) )
  {
    KMessageBox::error(this,
                i18n("Could not open profile '%1' for writing: %2")
                     .arg(fileName)
                     .arg(kapp->translate("QFile", file.errorString())),
                i18n("Open failed"));
    return;
  }

  QTextStream stream(&file);

  stream << "M " << mainWidget->targetDir->text() << endl;
  stream << "P " << Archiver::instance->getFilePrefix() << endl;
  stream << "S " << Archiver::instance->getMaxSliceMBs() << endl;

  for (QStringList::const_iterator it = includes.begin(); it != includes.end(); ++it)
    stream << "I " << *it << endl;

  for (QStringList::const_iterator it = excludes.begin(); it != excludes.end(); ++it)
    stream << "E " << *it << endl;

  file.close();
}

//--------------------------------------------------------------------------------

void MainWindow::profileSettings()
{
  SettingsDialog dialog(this);

  dialog.prefix->setText(Archiver::instance->getFilePrefix());
  dialog.setMaxMB(Archiver::instance->getMaxSliceMBs());

  if ( dialog.exec() == QDialog::Accepted )
  {
    Archiver::instance->setFilePrefix(dialog.prefix->text().stripWhiteSpace());
    Archiver::instance->setMaxSliceMBs(dialog.maxSliceSize->value());
  }
}

//--------------------------------------------------------------------------------

void MainWindow::newProfile()
{
  Archiver::instance->setFilePrefix("");  // back to default
  Archiver::instance->setMaxSliceMBs(Archiver::UNLIMITED);
  Archiver::instance->setTarget("");

  // clear selection
  QStringList includes, excludes;
  selector->setBackupList(includes, excludes);

  mainWidget->targetDir->setText("");
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
  QString text = KGlobal::instance()->aboutData()->programName() + " - " +
                 i18n("Files: %1 Size: %2 MB\n%3")
                    .arg(Archiver::instance->getTotalFiles())
                    .arg(QString::number(Archiver::instance->getTotalBytes() / 1024.0 / 1024.0, 'f', 2))
                    .arg(KStringHandler::csqueeze(lastLog, 60));

  QToolTip::add(sysTray, text);
}

//--------------------------------------------------------------------------------

void MainWindow::inProgress(bool runs)
{
  if ( runs )
  {
    sysTray->setMovie(KGlobal::instance()->iconLoader()->loadMovie("kbackup_runs", KIcon::Panel));
    startBackupAction->setEnabled(false);
    cancelBackupAction->setEnabled(true);
  }
  else
  {
    sysTray->setPixmap(sysTray->loadIcon("kbackup"));
    startBackupAction->setEnabled(true);
    cancelBackupAction->setEnabled(false);

    if ( autorun )
      kapp->quit();
  }
}

//--------------------------------------------------------------------------------

void MainWindow::dockInSysTray()
{
  KGlobal::instance()->config()->writeEntry("dockInSysTray", docked->isChecked());

  sysTray->setShown(docked->isChecked());
}

//--------------------------------------------------------------------------------
