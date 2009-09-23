//**************************************************************************
//   (c) 2006 - 2009 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>

#include <QFile>
#include <QString>

#include <MainWindow.hxx>
#include <Archiver.hxx>

#include <iostream>

//--------------------------------------------------------------------------------

int main(int argc, char **argv)
{
  KAboutData about("kbackup", "", ki18n("KBackup"),
                   "0.6.3", ki18n("An easy to use backup program"), KAboutData::License_GPL_V2,
                   ki18n("(c) 2006 - 2009 Martin Koller"),  // copyright
                   KLocalizedString(),  // added text
                   "http://www.kde-apps.org/content/show.php?content=44998",  // homepage
                   "kollix@aon.at");  // bugs to

  about.addAuthor(ki18n("Martin Koller"), ki18n("Developer"), "kollix@aon.at");

  KCmdLineOptions options;
  options.add("+[profile]", ki18n("Start with given profile"));
  options.add("script <file>", ki18n("Script to run after finishing one archive slice"));
  options.add("auto <profile>", ki18n("Automatically run the backup with the given profile"
                                      " and terminate when done."));
  options.add("autobg <profile>", ki18n("Automatically run the backup with the given profile"
                                        " in the background (without showing a window)"
                                        " and terminate when done."));
  options.add("verbose", ki18n("In autobg mode be verbose and print every single filename during backup"));

  KCmdLineArgs::addCmdLineOptions(options);
  KCmdLineArgs::init(argc, argv, &about);

  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

  bool interactive = !args->isSet("autobg");

  KApplication app(interactive);

  MainWindow *mainWin = 0;

  if ( interactive )
  {
    mainWin = new MainWindow;
    mainWin->show();
  }
  else
    new Archiver(0);

  QString file = args->getOption("script");
  if ( file.length() )
    Archiver::sliceScript = file;

  if ( interactive )
  {
    QString profile;

    if ( args->count() > 0 )
      profile = args->arg(0);

    QString file = args->getOption("auto");
    if ( file.length() )
      profile = file;

    if ( profile.length() )
      mainWin->loadProfile(profile, true);

    if ( args->isSet("auto") )
      mainWin->runBackup();

    return app.exec();
  }
  else
  {
    QStringList includes, excludes;
    QString error, fileName = args->getOption("autobg");

    Archiver::instance->setVerbose(args->isSet("verbose"));

    if ( !Archiver::instance->loadProfile(fileName, includes, excludes, error) )
    {
      std::cerr << i18n("Could not open profile '%1' for reading: %2")
                        .arg(fileName)
                        .arg(error).toUtf8().constData() << std::endl;
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
