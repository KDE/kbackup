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

#include <qapplication.h>
#include <qsplitter.h>
#include <qspinbox.h>

#include <kstdaction.h>
#include <kaction.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <klineedit.h>
#include <kio/global.h>

//--------------------------------------------------------------------------------

MainWindow::MainWindow()
{
  KStdAction::quit(this, SLOT(close()), actionCollection());

  new KAction(i18n("New Profile"), "filenew", 0, this,
              SLOT(newProfile()), actionCollection(), "newProfile");

  new KAction(i18n("Load Profile"), "fileopen", 0, this,
              SLOT(loadProfile()), actionCollection(), "loadProfile");

  new KAction(i18n("Save Profile"), "filesave", 0, this,
              SLOT(saveProfile()), actionCollection(), "saveProfile");

  new KAction(i18n("Profile Settings"), "", 0, this,
              SLOT(profileSettings()), actionCollection(), "profileSettings");

  createGUI();

  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

  selector = new Selector(splitter);

  mainWidget = new MainWidget(splitter);
  mainWidget->setSelector(selector);
  splitter->setCollapsible(mainWidget, false);

  setCentralWidget(splitter);
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
                     .arg(qApp->translate("QFile", file.errorString())),
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
                     .arg(qApp->translate("QFile", file.errorString())),
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
