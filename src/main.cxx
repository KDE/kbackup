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


int main(int argc, char **argv)
{
  KAboutData about("kbackup", I18N_NOOP("KBackup"),
                   "0.5.1", I18N_NOOP("An easy to use backup program"), KAboutData::License_GPL_V2,
                   "(c) 2006, Martin Koller",  // copyright
                   0,  // added text
                   "http://www.kde-apps.org/content/show.php?content=44998",  // homepage
                   "m.koller@surfeu.at");  // bugs to

  about.addAuthor("Martin Koller",
                  I18N_NOOP("Developer"), "m.koller@surfeu.at");

  static const KCmdLineOptions options[] =
  {
     { "+[profile]", I18N_NOOP("Start with given profile"), 0 },
     { "script <file>", I18N_NOOP("Script to run after finishing one archive slice"), 0 },
     KCmdLineLastOption // End of options.
  };

  KCmdLineArgs::addCmdLineOptions(options);
  KCmdLineArgs::init(argc, argv, &about);

  KApplication app;

  MainWindow *mainWin = new MainWindow;
  mainWin->show();

  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

  if ( args->count() > 0 )
    mainWin->loadProfile(QFile::decodeName(args->arg(0)), true);

  QCString file = args->getOption("script");
  if ( file.length() )
    Archiver::sliceScript = QFile::decodeName(file);

  return app.exec();
}
