/***************************************************************************
 *   (c) 2006, Martin Koller, m.koller@surfeu.at                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, version 2 of the License                *
 *                                                                         *
 ***************************************************************************/

#include <MainWindow.hxx>
#include <Selector.hxx>
#include <Archiver.hxx>
#include <MainWidget.h>
#include <SettingsDialog.h>

#include <qsplitter.h>
#include <qspinbox.h>
#include <qtooltip.h>
#include <qmovie.h>

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

//--------------------------------------------------------------------------------

MainWindow::MainWindow()
  : KMainWindow(0, 0, 0)
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

  createGUI();

  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

  selector = new Selector(splitter);

  mainWidget = new MainWidget(splitter);
  mainWidget->setSelector(selector);
  splitter->setCollapsible(mainWidget, false);

  setCentralWidget(splitter);

  // system tray icon
  sysTray = new KSystemTray(this);
  sysTray->setPixmap(sysTray->loadIcon("kbackup"));
  sysTray->setShown(docked->isChecked());

  connect(sysTray, SIGNAL(quitSelected()), this, SLOT(maybeQuit()));

  connect(Archiver::instance, SIGNAL(totalFilesChanged(int)), this, SLOT(changeSystrayTip()));
  connect(Archiver::instance, SIGNAL(logging(const QString &)), this, SLOT(loggingSlot(const QString &)));
  connect(Archiver::instance, SIGNAL(inProgress(bool)), this, SLOT(inProgress(bool)));

  startBackup = new KAction(i18n("Start Backup"), "kbackup_start", 0, mainWidget,
                            SLOT(startBackup()), actionCollection(), "startBackup");
  startBackup->plug(sysTray->contextMenu(), 1);

  cancelBackup = new KAction(i18n("Cancel Backup"), "kbackup_cancel", 0, Archiver::instance,
                             SLOT(cancel()), actionCollection(), "cancelBackup");
  cancelBackup->plug(sysTray->contextMenu(), 2);
  cancelBackup->setEnabled(false);

  changeSystrayTip();
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

void MainWindow::loadProfile()
{
  QString fileName = KFileDialog::getOpenFileName(QString::null, "*.kbp|" + i18n("KBackup Profile (*.kbp)"));

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

  QStringList includes, excludes;
  char type;
  QTextStream stream(&file);

  Archiver::filePrefix = "";  // back to default (in case old profile read which does not include prefix)

  while ( ! stream.atEnd() )
  {
    stream >> type;
    stream.skipWhiteSpace();

    if ( type == 'M' )
    {
      mainWidget->setTargetURL(stream.readLine());  // include white space
    }
    else if ( type == 'P' )
    {
      Archiver::filePrefix = stream.readLine();  // include white space
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

  if ( adaptTreeWidth )
    selector->adjustColumn(0);
}

//--------------------------------------------------------------------------------

void MainWindow::saveProfile()
{
  QString fileName = KFileDialog::getSaveFileName(QString::null, "*.kbp|" + i18n("KBackup Profile (*.kbp)"));

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
  stream << "P " << Archiver::filePrefix << endl;

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

  dialog.prefix->setText(Archiver::filePrefix);

  if ( dialog.exec() == QDialog::Accepted )
    Archiver::filePrefix = dialog.prefix->text().stripWhiteSpace();
}

//--------------------------------------------------------------------------------

void MainWindow::newProfile()
{
  Archiver::filePrefix = "";  // back to default
  mainWidget->setTargetURL("");

  // clear selection
  QStringList includes, excludes;
  selector->setBackupList(includes, excludes);
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
                 tr("Files: %1 Size: %2 MB\n%3")
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
    startBackup->setEnabled(false);
    cancelBackup->setEnabled(true);
  }
  else
  {
    sysTray->setPixmap(sysTray->loadIcon("kbackup"));
    startBackup->setEnabled(true);
    cancelBackup->setEnabled(false);
  }
}

//--------------------------------------------------------------------------------

void MainWindow::dockInSysTray()
{
  KGlobal::instance()->config()->writeEntry("dockInSysTray", docked->isChecked());

  sysTray->setShown(docked->isChecked());
}

//--------------------------------------------------------------------------------
