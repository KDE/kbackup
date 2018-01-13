//**************************************************************************
//   (c) 2006 - 2018 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#include <signal.h>

#include <QApplication>
#include <QScopedPointer>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFile>
#include <QString>
#include <QTimer>
#include <QPointer>

#include <KAboutData>
#include <KLocalizedString>

#include <MainWindow.hxx>
#include <Archiver.hxx>

#include <iostream>

//--------------------------------------------------------------------------------

void sigHandler(int sig)
{
  Q_UNUSED(sig)

  QTimer::singleShot(0, Archiver::instance, SLOT(cancel()));
  QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
}

//--------------------------------------------------------------------------------

int main(int argc, char **argv)
{
  QScopedPointer<QCoreApplication> app(new QCoreApplication(argc, argv));

  KLocalizedString::setApplicationDomain("kbackup");

  KAboutData about("kbackup", i18n("KBackup"),
                   "1.0.1", i18n("An easy to use backup program"), KAboutLicense::GPL_V2,
                   i18n("(c) 2006 - 2018 Martin Koller"),  // copyright
                   QString(),  // added text
                   "https://www.linux-apps.com/content/show.php?content=44998");  // homepage

  about.addAuthor(i18n("Martin Koller"), i18n("Developer"), "kollix@aon.at");

  KAboutData::setApplicationData(about);

  QCommandLineParser cmdLine;
  cmdLine.addVersionOption();
  cmdLine.addHelpOption();

  cmdLine.addPositionalArgument("profile", i18n("Start with given profile."), "[profile]");

  cmdLine.addOption(QCommandLineOption("script", i18n("Script to run after finishing one archive slice."), "file"));

  cmdLine.addOption(QCommandLineOption("auto", i18n("Automatically run the backup with the given profile "
                                                    "and terminate when done."), "profile"));

  cmdLine.addOption(QCommandLineOption("autobg", i18n("Automatically run the backup with the given profile "
                                                      "in the background (without showing a window) "
                                                      "and terminate when done."), "profile"));


  cmdLine.addOption(QCommandLineOption("verbose", i18n("In autobg mode be verbose and print every "
                                                       "single filename during backup.")));

  cmdLine.addOption(QCommandLineOption("forceFull", i18n("In auto/autobg mode force the backup to be a full backup "
                                                         "instead of acting on the profile settings.")));

  about.setupCommandLine(&cmdLine);
  cmdLine.process(*app);
  about.processCommandLine(&cmdLine);

  bool interactive = !cmdLine.isSet("autobg");

  if ( interactive )
  {
    delete app.take();  // must make explicitely. Only reset() leads to error
    // kf5.kcoreaddons.kaboutdata: Could not initialize the equivalent properties of Q*Application: no instance (yet) existing.
    app.reset(new QApplication(argc, argv));
    KAboutData::setApplicationData(about);
  }

  QPointer<MainWindow> mainWin;

  if ( interactive )
  {
    mainWin = new MainWindow;
    mainWin->show();
  }
  else
    new Archiver(0);

  signal(SIGTERM, sigHandler);
  signal(SIGINT, sigHandler);

  QString file = cmdLine.value("script");
  if ( file.length() )
    Archiver::sliceScript = file;

  if ( interactive )
  {
    QString profile;

    QStringList args = cmdLine.positionalArguments();

    if ( args.count() > 0 )
      profile = args[0];

    QString file = cmdLine.value("auto");
    if ( file.length() )
      profile = file;

    if ( profile.length() )
      mainWin->loadProfile(profile, true);

    if ( cmdLine.isSet("forceFull") )
      Archiver::instance->setForceFullBackup();

    if ( cmdLine.isSet("auto") )
      mainWin->runBackup();

    int ret = app->exec();

    delete mainWin;
    return ret;
  }
  else
  {
    QStringList includes, excludes;
    QString error, fileName = cmdLine.value("autobg");

    Archiver::instance->setVerbose(cmdLine.isSet("verbose"));

    if ( !Archiver::instance->loadProfile(fileName, includes, excludes, error) )
    {
      std::cerr << i18n("Could not open profile '%1' for reading: %2")
                        .arg(fileName)
                        .arg(error).toUtf8().constData() << std::endl;
      return -1;
    }
    else
    {
      if ( cmdLine.isSet("forceFull") )
        Archiver::instance->setForceFullBackup();

      if ( Archiver::instance->createArchive(includes, excludes) )
        return 0;
      else
        return -1;
    }
  }

  return 0;
}
