//**************************************************************************
//   Copyright 2006 - 2018 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#include <signal.h>

#include <memory>

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
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

  QTimer::singleShot(0, Archiver::instance, &Archiver::cancel);
  QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
}

//--------------------------------------------------------------------------------

int main(int argc, char **argv)
{
  std::unique_ptr<QCoreApplication> app(new QCoreApplication(argc, argv));

  KLocalizedString::setApplicationDomain(QByteArrayLiteral("kbackup"));

  KAboutData about(QStringLiteral("kbackup"), i18n("KBackup"),
                   QStringLiteral(KBACKUP_VERSION), i18n("An easy to use backup program"), KAboutLicense::GPL_V2,
                   i18n("(c) 2006 - 2018 Martin Koller"),  // copyright
                   QString(),  // added text
                   QStringLiteral("https://www.linux-apps.com/content/show.php?content=44998"));  // homepage

  about.addAuthor(i18n("Martin Koller"), i18n("Developer"), QStringLiteral("kollix@aon.at"));

  about.setDesktopFileName(QStringLiteral("org.kde.kbackup"));

  KAboutData::setApplicationData(about);

  QCommandLineParser cmdLine;

  cmdLine.addPositionalArgument(QStringLiteral("profile"), i18n("Start with given profile."), QStringLiteral("[profile]"));

  cmdLine.addOption(QCommandLineOption(QStringLiteral("script"), i18n("Script to run after finishing one archive slice."), QStringLiteral("file")));

  cmdLine.addOption(QCommandLineOption(QStringLiteral("auto"), i18n("Automatically run the backup with the given profile "
                                                    "and terminate when done."), QStringLiteral("profile")));

  cmdLine.addOption(QCommandLineOption(QStringLiteral("autobg"), i18n("Automatically run the backup with the given profile "
                                                      "in the background (without showing a window) "
                                                      "and terminate when done."), QStringLiteral("profile")));


  cmdLine.addOption(QCommandLineOption(QStringLiteral("verbose"), i18n("In autobg mode be verbose and print every "
                                                       "single filename during backup.")));

  cmdLine.addOption(QCommandLineOption(QStringLiteral("forceFull"), i18n("In auto/autobg mode force the backup to be a full backup "
                                                         "instead of acting on the profile settings.")));

  about.setupCommandLine(&cmdLine);
  cmdLine.process(*app);
  about.processCommandLine(&cmdLine);

  bool interactive = !cmdLine.isSet(QStringLiteral("autobg"));

  if ( interactive )
  {
    delete app.release();  // must make explicitly. Only reset() leads to error
    // kf5.kcoreaddons.kaboutdata: Could not initialize the equivalent properties of Q*Application: no instance (yet) existing.
    app.reset(new QApplication(argc, argv));
    QApplication *qapp = qobject_cast<QApplication *>(app.get());
    qapp->setWindowIcon(QIcon::fromTheme(QStringLiteral("kbackup")));

    KAboutData::setApplicationData(about);
  }

  QPointer<MainWindow> mainWin;

  if ( interactive )
  {
    mainWin = new MainWindow;
    mainWin->show();
  }
  else
    new Archiver(nullptr);

  signal(SIGTERM, sigHandler);
  signal(SIGINT, sigHandler);

  QString file = cmdLine.value(QStringLiteral("script"));
  if ( file.length() )
    Archiver::sliceScript = file;

  if ( interactive )
  {
    QString profile;

    QStringList args = cmdLine.positionalArguments();

    if ( !args.isEmpty() )
      profile = args[0];

    const QString file = cmdLine.value(QStringLiteral("auto"));
    if ( !file.isEmpty() )
      profile = file;

    if ( !profile.isEmpty() )
      mainWin->loadProfile(profile, true);

    if ( cmdLine.isSet(QStringLiteral("forceFull")) )
      Archiver::instance->setForceFullBackup();

    if ( cmdLine.isSet(QStringLiteral("auto")) )
      mainWin->runBackup();

    int ret = app->exec();

    delete mainWin;
    return ret;
  }
  else
  {
    QStringList includes, excludes;
    QString error, fileName = cmdLine.value(QStringLiteral("autobg"));

    Archiver::instance->setVerbose(cmdLine.isSet(QStringLiteral("verbose")));

    if ( !Archiver::instance->loadProfile(fileName, includes, excludes, error) )
    {
      std::cerr << qPrintable(i18n("Could not open profile '%1' for reading: %2", fileName, error)) << std::endl;
      return -1;
    }
    else
    {
      if ( cmdLine.isSet(QStringLiteral("forceFull")) )
        Archiver::instance->setForceFullBackup();

      if ( Archiver::instance->createArchive(includes, excludes) )
        return 0;
      else
        return -1;
    }
  }

  return 0;
}
