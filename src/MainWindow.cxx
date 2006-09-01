/***************************************************************************
 *   (c) 2006, Martin Koller, m.koller@surfeu.at                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, version 2 of the License                *
 *                                                                         *
 ***************************************************************************/

#include <MainWindow.hxx>
#include <MainWidget.h>
#include <Selector.hxx>

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

  new KAction(i18n("Load Profile"), "fileopen", 0, this,
              SLOT(loadProfile()), actionCollection(), "loadProfile");

  new KAction(i18n("Save Profile"), "filesave", 0, this,
              SLOT(saveProfile()), actionCollection(), "saveProfile");

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

  while ( ! stream.atEnd() )
  {
    stream >> type;
    if ( type == 'M' )
    {
      stream.skipWhiteSpace();
      mainWidget->setTargetURL(stream.readLine());  // include white space
    }
    else if ( type == 'I' )
    {
      stream.skipWhiteSpace();
      includes.append(stream.readLine());
    }
    else if ( type == 'E' )
    {
      stream.skipWhiteSpace();
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

  for (QStringList::const_iterator it = includes.begin(); it != includes.end(); ++it)
    stream << "I " << *it << endl;
  
  for (QStringList::const_iterator it = excludes.begin(); it != excludes.end(); ++it)
    stream << "E " << *it << endl;
  
  file.close();
}

//--------------------------------------------------------------------------------
