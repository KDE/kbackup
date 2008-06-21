/***************************************************************************
 *   (c) 2006, Martin Koller, m.koller@surfeu.at                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, version 2 of the License                *
 *                                                                         *
 ***************************************************************************/

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>

#include <qfile.h>

#include <MainWindow.hxx>
#include <Archiver.hxx>

#include <iostream>
using namespace std;

//--------------------------------------------------------------------------------

int main(int argc, char **argv)
{
  KAboutData about("kbackup", I18N_NOOP("KBackup"),
                   "0.6", I18N_NOOP("An easy to use backup program"), KAboutData::License_GPL_V2,
                   "(c) 2006, 2007, 2008 Martin Koller",  // copyright
                   0,  // added text
                   "http://www.kde-apps.org/content/show.php?content=44998",  // homepage
                   "m.koller@surfeu.at");  // bugs to

  about.addAuthor("Martin Koller",
                  I18N_NOOP("Developer"), "m.koller@surfeu.at");

  static const KCmdLineOptions options[] =
  {
     { "+[profile]", I18N_NOOP("Start with given profile"), 0 },
     { "script <file>", I18N_NOOP("Script to run after finishing one archive slice"), 0 },
     { "auto <profile>", I18N_NOOP("Automatically run the backup with the given profile"
                                   " and terminate when done."), 0 },
     { "autobg <profile>", I18N_NOOP("Automatically run the backup with the given profile"
                                     " in the background (without showing a window)"
                                     " and terminate when done."), 0 },
     KCmdLineLastOption // End of options.
  };

  KCmdLineArgs::addCmdLineOptions(options);
  KCmdLineArgs::init(argc, argv, &about);

  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

  bool interactive = !args->isSet("autobg");

  KApplication app(interactive, interactive);

  MainWindow *mainWin = 0;

  if ( interactive )
  {
    mainWin = new MainWindow;
    mainWin->show();
  }
  else
    new Archiver(0);

  QCString file = args->getOption("script");
  if ( file.length() )
    Archiver::sliceScript = QFile::decodeName(file);

  if ( interactive )
  {
    QString profile;

    if ( args->count() > 0 )
      profile = QFile::decodeName(args->arg(0));

    QCString file = args->getOption("auto");
    if ( file.length() )
      profile = QFile::decodeName(file);

    if ( profile.length() )
      mainWin->loadProfile(profile, true);

    if ( args->isSet("auto") )
      mainWin->runBackup();

    return app.exec();
  }
  else
  {
    QStringList includes, excludes;
    QString error, fileName = QFile::decodeName(args->getOption("autobg"));

    if ( !Archiver::instance->loadProfile(fileName, includes, excludes, error) )
    {
      cerr << i18n("Could not open profile '%1' for reading: %2")
              .arg(fileName)
              .arg(kapp->translate("QFile", error)) << endl;
      return -1;
    }
    else
    {
      if ( Archiver::instance->createArchive(includes, excludes) )
        return 0;
      else
        return -1;
    }
  }

  return 0;
}
