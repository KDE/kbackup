//**************************************************************************
//   (c) 2006 - 2010 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#include <Archiver.hxx>

#include <ktar.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <ktemporaryfile.h>
#include <kfilterbase.h>
#include <kfilterdev.h>
#include <kstandarddirs.h>
#include <kio/job.h>
#include <kio/jobuidelegate.h>
#include <kprocess.h>
#include <kde_file.h>
#include <kdirselectdialog.h>
#include <kmountpoint.h>

#include <qapplication.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qcursor.h>
#include <QTextStream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>

// For INT64_MAX:
// The ISO C99 standard specifies that in C++ implementations these
// macros (stdint.h,inttypes.h) should only be defined if explicitly requested.

// ISO C99: 7.18 Integer types
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif
#include <stdint.h>


#include <iostream>

//--------------------------------------------------------------------------------

QString Archiver::sliceScript;
Archiver *Archiver::instance;

const KIO::filesize_t MAX_SLICE = INT64_MAX; // 64bit max value

//--------------------------------------------------------------------------------

Archiver::Archiver(QWidget *parent)
  : QObject(parent),
    archive(0), totalBytes(0), totalFiles(0), filteredFiles(0), sliceNum(0), mediaNeedsChange(false),
    fullBackupInterval(1), incrementalBackup(false), forceFullBackup(false),
    sliceCapacity(MAX_SLICE), interactive(parent != 0),
    cancelled(false), runs(false), skippedFiles(false), verbose(false), jobResult(0)
{
  instance = this;

  maxSliceMBs    = Archiver::UNLIMITED;
  numKeptBackups = Archiver::UNLIMITED;

  setCompressFiles(false);

  if ( !interactive )
  {
    connect(this, SIGNAL(logging(const QString &)), this, SLOT(loggingSlot(const QString &)));
    connect(this, SIGNAL(warning(const QString &)), this, SLOT(warningSlot(const QString &)));
  }
}

//--------------------------------------------------------------------------------

void Archiver::setCompressFiles(bool b)
{
  if ( b )
  {
    ext = ".bz2";
    if ( ! KFilterBase::findFilterByMimeType("application/x-bzip2") )
      ext = ".gz";
  }
  else
  {
    ext = "";
  }
}

//--------------------------------------------------------------------------------

void Archiver::setTarget(const KUrl &target)
{
  targetURL = target;
  calculateCapacity();
}

//--------------------------------------------------------------------------------

void Archiver::setMaxSliceMBs(int mbs)
{
  maxSliceMBs = mbs;
  calculateCapacity();
}

//--------------------------------------------------------------------------------

void Archiver::setKeptBackups(int num)
{
  numKeptBackups = num;
}

//--------------------------------------------------------------------------------

void Archiver::setFilter(const QString &filter)
{
  filters.clear();
  QStringList list = filter.split(' ', QString::SkipEmptyParts);
  foreach (const QString &str, list)
    filters.append(QRegExp(str, Qt::CaseSensitive, QRegExp::Wildcard));
}

//--------------------------------------------------------------------------------

QString Archiver::getFilter() const
{
  QString filter;
  foreach (const QRegExp &reg, filters)
  {
    filter += reg.pattern();
    filter += ' ';
  }
  return filter;
}

//--------------------------------------------------------------------------------

void Archiver::setFullBackupInterval(int days)
{
  fullBackupInterval = days;

  if ( fullBackupInterval == 1 )
  {
    setIncrementalBackup(false);
    lastFullBackup = QDateTime();
    lastBackup = QDateTime();
  }
}

//--------------------------------------------------------------------------------

void Archiver::setForceFullBackup(bool force)
{
  forceFullBackup = force;
  emit backupTypeChanged(isIncrementalBackup());
}

//--------------------------------------------------------------------------------

void Archiver::setIncrementalBackup(bool inc)
{
  incrementalBackup = inc;
  emit backupTypeChanged(isIncrementalBackup());
}

//--------------------------------------------------------------------------------

void Archiver::setFilePrefix(const QString &prefix)
{
  filePrefix = prefix;
}

//--------------------------------------------------------------------------------

void Archiver::calculateCapacity()
{
  if ( targetURL.isEmpty() ) return;

  // calculate how large a slice can actually be
  // - limited by the target directory (when we store directly into a local dir)
  // - limited by the "tmp" dir when we create a tmp file for later upload via KIO
  // - limited by Qt (64bit int)
  // - limited by user defined maxSliceMBs

  KIO::filesize_t totalBytes = 0;

  if ( targetURL.isLocalFile() )
  {
    if ( ! getDiskFree(targetURL.path(), totalBytes, sliceCapacity) )
      return;
  }
  else
  {
    getDiskFree(KStandardDirs::locateLocal("tmp", ""), totalBytes, sliceCapacity);
    // as "tmp" is also used by others and by us when compressing a file,
    // don't eat it up completely. Reserve 10%
    sliceCapacity = sliceCapacity * 9 / 10;
  }

  // limit to what Qt can handle
  sliceCapacity = qMin(sliceCapacity, MAX_SLICE);

  if ( maxSliceMBs != UNLIMITED )
  {
    KIO::filesize_t max = static_cast<KIO::filesize_t>(maxSliceMBs) * 1024 * 1024;
    sliceCapacity = qMin(sliceCapacity, max);
  }

  sliceBytes = 0;

  // if the disk is full (capacity == 0), don't tell the user "unlimited"
  // sliceCapacity == 0 has a special meaning as "unlimited"; see MainWidget.cxx
  if ( sliceCapacity == 0 ) sliceCapacity = 1;
  emit targetCapacity(sliceCapacity);
}

//--------------------------------------------------------------------------------

bool Archiver::loadProfile(const QString &fileName, QStringList &includes, QStringList &excludes, QString &error)
{
  QFile file(fileName);
  if ( ! file.open(QIODevice::ReadOnly) )
  {
    error = file.errorString();
    return false;
  }

  loadedProfile = fileName;

  QString target;
  QChar type, blank;
  QTextStream stream(&file);

  // back to default (in case old profile read which does not include these)
  setFilePrefix("");
  setMaxSliceMBs(Archiver::UNLIMITED);
  setFullBackupInterval(1);  // default as in previous versions
  filters.clear();

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
      setFilePrefix(prefix);
    }
    else if ( type == 'R' )
    {
      int max;
      stream >> max;
      setKeptBackups(max);
    }
    else if ( type == 'F' )
    {
      int days;
      stream >> days;
      setFullBackupInterval(days);
    }
    else if ( type == 'B' )  // last dateTime for backup
    {
      QString dateTime;
      stream >> dateTime;
      lastBackup = QDateTime::fromString(dateTime, Qt::ISODate);
    }
    else if ( type == 'L' )  // last dateTime for full backup
    {
      QString dateTime;
      stream >> dateTime;
      lastFullBackup = QDateTime::fromString(dateTime, Qt::ISODate);
    }
    else if ( type == 'S' )
    {
      int max;
      stream >> max;
      setMaxSliceMBs(max);
    }
    else if ( type == 'C' )
    {
      int change;
      stream >> change;
      setMediaNeedsChange(change);
    }
    else if ( type == 'X' )
    {
      setFilter(stream.readLine());  // include white space
    }
    else if ( type == 'Z' )
    {
      int compress;
      stream >> compress;
      setCompressFiles(compress);
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

  setTarget(KUrl(target));

  setIncrementalBackup(
    (fullBackupInterval > 1) && lastFullBackup.isValid() &&
    (lastFullBackup.daysTo(QDateTime::currentDateTime()) < fullBackupInterval));

  return true;
}

//--------------------------------------------------------------------------------

bool Archiver::saveProfile(const QString &fileName, const QStringList &includes, const QStringList &excludes, QString &error)
{
  QFile file(fileName);

  if ( ! file.open(QIODevice::WriteOnly) )
  {
    error = file.errorString();
    return false;
  }

  QTextStream stream(&file);

  stream << "M " << targetURL.pathOrUrl() << endl;
  stream << "P " << getFilePrefix() << endl;
  stream << "S " << getMaxSliceMBs() << endl;
  stream << "R " << getKeptBackups() << endl;
  stream << "F " << getFullBackupInterval() << endl;

  if ( getLastFullBackup().isValid() )
    stream << "L " << getLastFullBackup().toString(Qt::ISODate) << endl;

  if ( getLastBackup().isValid() )
    stream << "B " << getLastBackup().toString(Qt::ISODate) << endl;

  stream << "C " << static_cast<int>(getMediaNeedsChange()) << endl;
  stream << "Z " << static_cast<int>(getCompressFiles()) << endl;

  if ( !filters.isEmpty() )
    stream << "X " << getFilter() << endl;

  for (QStringList::const_iterator it = includes.begin(); it != includes.end(); ++it)
    stream << "I " << *it << endl;

  for (QStringList::const_iterator it = excludes.begin(); it != excludes.end(); ++it)
    stream << "E " << *it << endl;

  file.close();
  return true;
}

//--------------------------------------------------------------------------------

bool Archiver::createArchive(const QStringList &includes, const QStringList &excludes)
{
  if ( includes.count() == 0 )
  {
    emit warning(i18n("Nothing selected for backup"));
    return false;
  }

  if ( !targetURL.isValid() )
  {
    emit warning(i18n("The target dir '%1' is not valid").arg(targetURL.pathOrUrl()));
    return false;
  }

  // non-interactive mode only allows local targets as KIO needs $DISPLAY
  if ( !interactive && !targetURL.isLocalFile() )
  {
    emit warning(i18n("The target dir '%1' must be a local file system dir and no remote URL")
                     .arg(targetURL.pathOrUrl()));
    return false;
  }

  // check if the target dir exists and optionally create it
  if ( targetURL.isLocalFile() )
  {
    QDir dir(targetURL.path());
    if ( !dir.exists() )
    {
      if ( !interactive ||
           (KMessageBox::warningYesNo(static_cast<QWidget*>(parent()),
              i18n("The target directory '%1' does not exist.\n\n"
                   "Shall I create it?").arg(dir.absolutePath())) == KMessageBox::Yes) )
      {
        if ( !dir.mkpath(".") )
        {
          emit warning(i18n("Could not create the target directory '%1'.\n"
                            "The operating system reports: %2")
                            .arg(dir.absolutePath())
                            .arg(strerror(errno)));
          return false;
        }
      }
      else
      {
        emit warning(i18n("The target dir does not exist"));
        return false;
      }
    }
  }

  excludeDirs.clear();
  excludeFiles.clear();

  // build map for directories and files to be excluded for fast lookup
  for (QStringList::const_iterator it = excludes.begin(); it != excludes.end(); ++it)
  {
    QFileInfo info(*it);

    if ( !info.isSymLink() && info.isDir() )
      excludeDirs.insert(*it, 0);
    else
      excludeFiles.insert(*it, 0);
  }

  baseName = "";
  sliceNum = 0;
  totalBytes = 0;
  totalFiles = 0;
  filteredFiles = 0;
  cancelled = false;
  skippedFiles = false;
  sliceList.clear();

  QDateTime startTime = QDateTime::currentDateTime();

  runs = true;
  emit inProgress(true);

  QTimer runTimer;
  if ( interactive )  // else we do not need to be interrupted during the backup
  {
    connect(&runTimer, SIGNAL(timeout()), this, SLOT(updateElapsed()));
    runTimer.start(1000);
  }
  elapsed.start();

  if ( ! getNextSlice() )
  {
    runs = false;
    emit inProgress(false);

    return false;
  }

  for (QStringList::const_iterator it = includes.begin(); !cancelled && (it != includes.end()); ++it)
  {
    QString entry = *it;

    if ( entry.endsWith("/") )
      entry.truncate(entry.length() - 1);

    QFileInfo info(entry);

    if ( !info.isSymLink() && info.isDir() )
    {
      QDir dir(info.absoluteFilePath());
      addDirFiles(dir);
    }
    else
      addFile(info.absoluteFilePath());
  }

  finishSlice();

  // reduce the number of old backups to the defined number
  if ( !cancelled && (numKeptBackups != UNLIMITED) )
  {
    emit logging(i18n("...reducing number of kept archives to max. %1").arg(numKeptBackups));

    if ( !targetURL.isLocalFile() )  // KIO needs $DISPLAY; non-interactive only allowed for local targets
    {
      QPointer<KIO::ListJob> listJob;
      listJob = KIO::listDir(targetURL, KIO::DefaultFlags, false);

      listJob->ui()->setWindow(static_cast<QWidget*>(parent()));

      connect(listJob, SIGNAL(entries(KIO::Job *, const KIO::UDSEntryList &)),
              this, SLOT(slotListResult(KIO::Job *, const KIO::UDSEntryList &)));

      while ( listJob )
        qApp->processEvents(QEventLoop::WaitForMoreEvents);
    }
    else  // non-intercative. create UDSEntryList on our own
    {
      QDir dir(targetURL.path());
      targetDirList.clear();
      foreach (QString fileName, dir.entryList())
      {
        KIO::UDSEntry entry;
        entry.insert(KIO::UDSEntry::UDS_NAME, fileName);
        targetDirList.append(entry);
      }
      jobResult = 0;
    }

    if ( jobResult == 0 )
    {
      qSort(targetDirList.begin(), targetDirList.end(), Archiver::UDSlessThan);
      QString prefix = filePrefix.isEmpty() ? QString::fromLatin1("backup_") : (filePrefix + "_");

      QString sliceName;
      int num = 0;

      foreach(KIO::UDSEntry entry, targetDirList)
      {
        QString entryName = entry.stringValue(KIO::UDSEntry::UDS_NAME);

        if ( entryName.startsWith(prefix) &&  // only matching current profile
             entryName.endsWith(".tar") )     // just to be sure
        {
          if ( (num < numKeptBackups) &&
               (sliceName.isEmpty() ||
                !entryName.startsWith(sliceName)) )     // whenever a new backup set (different time) is found
          {
            sliceName = entryName.left(prefix.length() + strlen("yyyy.MM.dd-hh.mm.ss_"));
            if ( !entryName.endsWith("_inc.tar") )  // do not count partial (differential) backup files
              num++;
            if ( num == numKeptBackups ) num++;  // from here on delete all others
          }

          if ( (num > numKeptBackups) &&   // delete all other files
               !entryName.startsWith(sliceName) )     // keep complete last matching archive set
          {
            KUrl url = targetURL;
            url.addPath(entryName);
            emit logging(i18n("...deleting %1").arg(entryName));

            // delete the file using KIO
            if ( !targetURL.isLocalFile() )  // KIO needs $DISPLAY; non-interactive only allowed for local targets
            {
              QPointer<KIO::SimpleJob> delJob;
              delJob = KIO::file_delete(url, KIO::DefaultFlags);

              delJob->ui()->setWindow(static_cast<QWidget*>(parent()));

              connect(delJob, SIGNAL(result(KJob *)), this, SLOT(slotResult(KJob *)));

              while ( delJob )
                qApp->processEvents(QEventLoop::WaitForMoreEvents);
            }
            else
            {
              QDir dir(targetURL.path());
              dir.remove(entryName);
            }
          }
        }
      }
    }
    else
    {
      emit warning(i18n("fetching directory listing of target failed. Can not reduce kept archives."));
    }
  }

  runs = false;
  emit inProgress(false);
  runTimer.stop();
  updateElapsed();  // to catch the last partly second

  if ( !cancelled )
  {
    lastBackup = startTime;
    if ( !isIncrementalBackup() )
    {
      lastFullBackup = lastBackup;
      setIncrementalBackup(fullBackupInterval > 1);  // after a full backup, the next will be incremental
    }

    if ( (fullBackupInterval > 1) && !loadedProfile.isEmpty() )
    {
      QString error;
      if ( !saveProfile(loadedProfile, includes, excludes, error) )
      {
        emit warning(i18n("Could not write backup timestamps into profile %1: %2")
                          .arg(loadedProfile)
                          .arg(error));
      }
    }

    emit logging(i18n("-- Filtered Files: %1").arg(filteredFiles));

    if ( skippedFiles )
      emit logging(i18n("!! Backup finished <b>but files were skipped</b> !!"));
    else
      emit logging(i18n("-- Backup successfully finished --"));

    if ( interactive )
    {
      int ret = KMessageBox::questionYesNoList(static_cast<QWidget*>(parent()),
                               skippedFiles ?
                                 i18n("The backup has finished but files were skipped.\n"
                                      "What do you want to do now?") :
                                 i18n("The backup has finished successfully.\n"
                                      "What do you want to do now?"),
                               sliceList,
                               QString::null,
                               KStandardGuiItem::cont(), KStandardGuiItem::quit(),
                               "showDoneInfo");

      if ( ret == KMessageBox::No ) // quit
        qApp->quit();
    }
    else
    {
      std::cerr << "-------" << std::endl;
      foreach (QString slice, sliceList)
        std::cerr << slice.toUtf8().constData() << std::endl;
      std::cerr << "-------" << std::endl;

      std::cerr << i18n("Totals: Files: %1, Size: %2, Duration: %3")
                   .arg(totalFiles)
                   .arg(KIO::convertSize(totalBytes))
                   .arg(KGlobal::locale()->formatTime(QTime().addMSecs(elapsed.elapsed()), true, true))
                   .toUtf8().constData() << std::endl;
    }

    return true;
  }
  else
  {
    emit logging(i18n("...Backup aborted!"));
    return false;
  }
}

//--------------------------------------------------------------------------------

void Archiver::cancel()
{
  if ( !runs ) return;

  if ( job )
  {
    job->kill();
    job = 0;
  }
  if ( !cancelled )
  {
    cancelled = true;

    if ( archive )
    {
      archive->close();  // else I can not remove the file - don't know why
      delete archive;
      archive = 0;
    }

    QFile(archiveName).remove(); // remove the unfinished tar file (which is now corrupted)
    emit warning(i18n("Backup cancelled"));
  }
}

//--------------------------------------------------------------------------------

void Archiver::finishSlice()
{
  if ( archive )
    archive->close();

  if ( ! cancelled )
  {
    runScript("slice_closed");

    if ( targetURL.isLocalFile() )
    {
      emit logging(i18n("...finished slice %1").arg(archiveName));
      sliceList << archiveName;  // store name for display at the end
    }
    else
    {
      KUrl source, target = targetURL;
      source.setPath(archiveName);

      while ( true )
      {
        // copy to have the archive for the script later down
        job = KIO::copy(source, target, KIO::DefaultFlags);

        job->ui()->setWindow(static_cast<QWidget*>(parent()));

        connect(job, SIGNAL(result(KJob *)), this, SLOT(slotResult(KJob *)));

        emit logging(i18n("...uploading archive %1 to %2").arg(source.fileName()).arg(target.pathOrUrl()));

        while ( job )
          qApp->processEvents(QEventLoop::WaitForMoreEvents);

        if ( jobResult == 0 )
        {
          target.addPath(source.fileName());
          sliceList << target.pathOrUrl();  // store name for display at the end
          break;
        }
        else
        {
          enum { ASK, CANCEL, RETRY } action = ASK;
          while ( action == ASK )
          {
            int ret = KMessageBox::warningYesNoCancel(static_cast<QWidget*>(parent()),
                        i18n("How shall we proceed with the upload?"), i18n("Upload Failed"),
                        KGuiItem(i18n("Retry")), KGuiItem(i18n("Change Target")));

            if ( ret == KMessageBox::Cancel )
            {
              action = CANCEL;
              break;
            }
            else if ( ret == KMessageBox::No )  // change target
            {
              target = KFileDialog::getExistingDirectoryUrl(KUrl("/"), static_cast<QWidget*>(parent()));
              if ( target.isEmpty() )
                action = ASK;
              else
                action = RETRY;
            }
            else
              action = RETRY;
          }

          if ( action == CANCEL )
            break;
        }
      }

      if ( jobResult != 0 )
        cancel();
    }
  }

  if ( ! cancelled )
    runScript("slice_finished");

  if ( !targetURL.isLocalFile() )
    QFile(archiveName).remove(); // remove the tmp file

  delete archive;
  archive = 0;
}

//--------------------------------------------------------------------------------

void Archiver::slotResult(KJob *theJob)
{
  if ( (jobResult = theJob->error()) )
  {
    theJob->uiDelegate()->showErrorMessage();

    emit warning(theJob->errorString());
  }
}

//--------------------------------------------------------------------------------

void Archiver::slotListResult(KIO::Job *theJob, const KIO::UDSEntryList &entries)
{
  if ( (jobResult = theJob->error()) )
  {
    theJob->uiDelegate()->showErrorMessage();

    emit warning(theJob->errorString());
  }

  targetDirList = entries;
}

//--------------------------------------------------------------------------------

void Archiver::runScript(const QString &mode)
{
  // do some extra action via external script (program)
  if ( sliceScript.length() )
  {
    QString mountPoint;
    if ( targetURL.isLocalFile() )
    {
      KMountPoint::Ptr ptr = KMountPoint::currentMountPoints().findByPath(targetURL.path());
      if ( ! ptr.isNull() )
        mountPoint = ptr->mountPoint();
    }

    KProcess proc;
    proc << sliceScript
         << mode
         << QFile::encodeName(archiveName)
         << QFile::encodeName(targetURL.pathOrUrl())
         << QFile::encodeName(mountPoint);

    connect(&proc, SIGNAL(readyReadStandardOutput()),
            this, SLOT(receivedOutput()));

    proc.setOutputChannelMode(KProcess::MergedChannels);

    if ( proc.execute() == -2 )
    {
      QString message = i18n("The script '%1' could not be started.").arg(sliceScript);
      if ( interactive )
        KMessageBox::error(static_cast<QWidget*>(parent()), message);
      else
        emit warning(message);
    }
  }
}

//--------------------------------------------------------------------------------

void Archiver::receivedOutput()
{
  KProcess *proc = static_cast<KProcess*>(sender());

  QByteArray buffer = proc->readAllStandardOutput();

  QString msg(buffer);
  if ( msg.endsWith("\n") )
    msg.truncate(msg.length() - 1);

  emit warning(msg);
}

//--------------------------------------------------------------------------------

bool Archiver::getNextSlice()
{
  sliceNum++;

  if ( archive )
  {
    emit sliceProgress(100);

    finishSlice();
    if ( cancelled ) return false;

    if ( interactive && mediaNeedsChange &&
         KMessageBox::warningContinueCancel(static_cast<QWidget*>(parent()),
                             i18n("The medium is full. Please insert medium Nr. %1").arg(sliceNum)) ==
          KMessageBox::Cancel )
    {
      cancel();
      return false;
    }
  }

  emit newSlice(sliceNum);

  if ( baseName.isEmpty() )
  {
    QString prefix = filePrefix.isEmpty() ? QString::fromLatin1("backup") : filePrefix;

    if ( targetURL.isLocalFile() )
      baseName = targetURL.path() + "/" + prefix + QDateTime::currentDateTime().toString("_yyyy.MM.dd-hh.mm.ss");
    else
      baseName = KStandardDirs::locateLocal("tmp", prefix + QDateTime::currentDateTime().toString("_yyyy.MM.dd-hh.mm.ss"));
  }

  archiveName = baseName + QString("_%1").arg(sliceNum);
  if ( isIncrementalBackup() )
    archiveName += "_inc.tar";  // mark the file as being not a full backup
  else
    archiveName += ".tar";

  runScript("slice_init");

  calculateCapacity();

  // don't create a bz2 compressed file as we compress each file on its own
  archive = new KTar(archiveName, "application/x-tar");

  while ( (sliceCapacity < 1024) || !archive->open(QIODevice::WriteOnly) )  // disk full ?
  {
    if ( !interactive )
      emit warning(i18n("The file '%1' can not be opened for writing.").arg(archiveName));

    if ( !interactive ||
         (KMessageBox::warningYesNo(static_cast<QWidget*>(parent()),
           i18n("The file '%1' can not be opened for writing.\n\n"
                "Do you want to retry?").arg(archiveName)) == KMessageBox::No) )
    {
      delete archive;
      archive = 0;

      cancel();
      return false;
    }
    calculateCapacity();  // try again; maybe the user freed up some space
  }

  return true;
}

//--------------------------------------------------------------------------------

void Archiver::addDirFiles(QDir &dir)
{
  if ( excludeDirs.contains(dir.absolutePath()) )
    return;

  // add the dir itself
  KDE_struct_stat status;
  memset(&status, 0, sizeof(status));
  if ( KDE_stat(QFile::encodeName(dir.absolutePath()), &status) == -1 )
  {
    emit warning(i18n("Could not get information of directory: %1\n"
                      "The operating system reports: %2")
                 .arg(dir.absolutePath())
                 .arg(strerror(errno)));
    return;
  }
  QFileInfo dirInfo(dir.absolutePath());

  if ( ! dirInfo.isReadable() )
  {
    emit warning(i18n("Directory '%1' is not readable. Skipping.").arg(dir.absolutePath()));
    skippedFiles = true;
    return;
  }

  totalFiles++;
  emit totalFilesChanged(totalFiles);
  if ( interactive || verbose )
    emit logging(dir.absolutePath());

  qApp->processEvents(QEventLoop::AllEvents, 5);
  if ( cancelled ) return;

  if ( ! archive->writeDir(QString(".") + dir.absolutePath(), dirInfo.owner(), dirInfo.group(),
                           status.st_mode, status.st_atime, status.st_mtime, status.st_ctime) )
  {
    emit warning(i18n("Could not write directory '%1' to archive.\n"
                      "Maybe the medium is full.").arg(dir.absolutePath()));
    return;
  }

  dir.setFilter(QDir::All | QDir::Hidden);

  const QFileInfoList list = dir.entryInfoList();

  for (int i = 0; !cancelled && (i < list.count()); i++)
  {
    if ( (list[i].fileName() == ".") || (list[i].fileName() == "..") ) continue;

    if ( !list[i].isSymLink() && list[i].isDir() )
    {
      QDir dir(list[i].absoluteFilePath());
      addDirFiles(dir);
    }
    else
      addFile(list[i].absoluteFilePath());
  }
}

//--------------------------------------------------------------------------------

bool Archiver::fileIsFiltered(const QString &fileName) const
{
  foreach (const QRegExp &exp, filters)
    if ( exp.exactMatch(fileName) )
      return true;

  return false;
}

//--------------------------------------------------------------------------------

void Archiver::addFile(const QFileInfo &info)
{
  if ( (isIncrementalBackup() && (info.lastModified() < lastBackup)) ||
       fileIsFiltered(info.fileName()) )
  {
    filteredFiles++;
    return;
  }

  if ( excludeFiles.contains(info.absoluteFilePath()) )
    return;

  // avoid including my own archive file
  // (QFileInfo to have correct path comparison even in case archiveName contains // etc.)
  // startsWith() is needed as KDE4 KTar does not create directly the .tar file but until it's closed
  // the file is named "...tarXXXX.new"
  if ( info.absoluteFilePath().startsWith(QFileInfo(archiveName).absoluteFilePath()) )
    return;

  if ( cancelled ) return;

  if ( ! info.isReadable() )
  {
    emit warning(i18n("File '%1' is not readable. Skipping.").arg(info.absoluteFilePath()));
    skippedFiles = true;
    return;
  }

  // emit before we do the compression, so that the receiver can already show
  // with which file we work

  // show filename + size
  if ( interactive || verbose )
    emit logging(info.absoluteFilePath() + QString(" (%1)").arg(KIO::convertSize(info.size())));

  qApp->processEvents(QEventLoop::AllEvents, 5);
  if ( cancelled ) return;

  if ( info.isSymLink() )
  {
    archive->addLocalFile(info.absoluteFilePath(), QString(".") + info.absoluteFilePath());
    totalFiles++;
    emit totalFilesChanged(totalFiles);
    return;
  }

  if ( !getCompressFiles() )
  {
    if ( ! addLocalFile(info) )  // this also increases totalBytes
    {
      cancel();  //  we must cancel as the tar-file is now corrupt (file was only partly written)
      return;
    }
  }
  else  // add the file compressed
  {
    // as we can't know which size the file will have after compression,
    // we create a compressed file and put this into the archive
    KTemporaryFile tmpFile;

    if ( ! compressFile(info.absoluteFilePath(), tmpFile) || cancelled )
      return;

    // here we have the compressed file in tmpFile

    tmpFile.open();  // size() only works if open

    if ( (sliceBytes + tmpFile.size()) > sliceCapacity )
      if ( ! getNextSlice() ) return;

    // to be able to create the exact same metadata (permission, date, owner) we need
    // to fill the file into the archive with the following:
    {
      KDE_struct_stat status;
      memset(&status, 0, sizeof(status));

      if ( KDE_stat(QFile::encodeName(info.absoluteFilePath()), &status) == -1 )
      {
        emit warning(i18n("Could not get information of file: %1\n"
                          "The operating system reports: %2")
                     .arg(info.absoluteFilePath())
                     .arg(strerror(errno)));

        return;
      }

      if ( ! archive->prepareWriting(QString(".") + info.absoluteFilePath() + ext,
                                     info.owner(), info.group(), tmpFile.size(),
                                     status.st_mode, status.st_atime, status.st_mtime, status.st_ctime) )
      {
        emit warning(i18n("Could not write to archive. Maybe the medium is full."));
        cancel();
        return;
      }

      const int BUFFER_SIZE = 8*1024;
      static char buffer[BUFFER_SIZE];
      qint64 len;
      int count = 0;
      while ( ! tmpFile.atEnd() )
      {
        len = tmpFile.read(buffer, BUFFER_SIZE);

        if ( len < 0 )  // error in reading
        {
          emit warning(i18n("Could not read from file '%1'\n"
                            "The operating system reports: %2")
                       .arg(info.absoluteFilePath())
                       .arg(tmpFile.errorString()));
          cancel();
          return;
        }

        if ( ! archive->writeData(buffer, len) )
        {
          emit warning(i18n("Could not write to archive. Maybe the medium is full."));
          cancel();
          return;
        }

        count = (count + 1) % 50;
        if ( count == 0 )
        {
          qApp->processEvents(QEventLoop::AllEvents, 5);
          if ( cancelled ) return;
        }
      }
      if ( ! archive->finishWriting(tmpFile.size()) )
      {
        emit warning(i18n("Could not write to archive. Maybe the medium is full."));
        cancel();
        return;
      }
    }

    // get filesize
    sliceBytes = archive->device()->pos();  // account for tar overhead
    totalBytes += tmpFile.size();

    emit sliceProgress(static_cast<int>(sliceBytes * 100 / sliceCapacity));
  }

  totalFiles++;
  emit totalFilesChanged(totalFiles);
  emit totalBytesChanged(totalBytes);

  qApp->processEvents(QEventLoop::AllEvents, 5);
}

//--------------------------------------------------------------------------------

bool Archiver::addLocalFile(const QFileInfo &info)
{
  KDE_struct_stat sourceStat;
  memset(&sourceStat, 0, sizeof(sourceStat));

  if ( KDE_stat(QFile::encodeName(info.absoluteFilePath()), &sourceStat) == -1 )
  {
    emit warning(i18n("Could not get information of file: %1\n"
                      "The operating system reports: %2")
                 .arg(info.absoluteFilePath())
                 .arg(strerror(errno)));

    return false;
  }

  QFile sourceFile(info.absoluteFilePath());
  if ( ! sourceFile.open(QIODevice::ReadOnly) )
  {
    emit warning(i18n("Could not open file '%1' for reading.").arg(info.absoluteFilePath()));
    return false;
  }

  if ( (sliceBytes + sourceStat.st_size) > sliceCapacity )
    if ( ! getNextSlice() ) return false;

  if ( ! archive->prepareWriting(QString(".") + info.absoluteFilePath(),
                                 info.owner(), info.group(), sourceStat.st_size,
                                 sourceStat.st_mode, sourceStat.st_atime, sourceStat.st_mtime, sourceStat.st_ctime) )
  {
    emit warning(i18n("Could not write to archive. Maybe the medium is full."));
    return false;
  }

  const int BUFFER_SIZE = 8*1024;
  static char buffer[BUFFER_SIZE];
  qint64 len;
  int count = 0, progress;
  QTime timer;
  timer.start();
  bool msgShown = false;
  KIO::filesize_t fileSize = sourceStat.st_size;
  KIO::filesize_t written = 0;

  while ( fileSize && !sourceFile.atEnd() && !cancelled )
  {
    len = sourceFile.read(buffer, BUFFER_SIZE);

    if ( len < 0 )  // error in reading
    {
      emit warning(i18n("Could not read from file '%1'\n"
                        "The operating system reports: %2")
                   .arg(info.absoluteFilePath())
                   .arg(sourceFile.errorString()));
      return false;
    }

    if ( ! archive->writeData(buffer, len) )
    {
      emit warning(i18n("Could not write to archive. Maybe the medium is full."));
      return false;
    }

    totalBytes += len;
    written += len;

    progress = static_cast<int>(written * 100 / fileSize);

    // stay responsive
    count = (count + 1) % 50;
    if ( count == 0 )
    {
      if ( msgShown )
        emit fileProgress(progress);

      emit totalBytesChanged(totalBytes);
      qApp->processEvents(QEventLoop::AllEvents, 5);
    }

    if ( !msgShown && (timer.elapsed() > 3000) && (progress < 50) )
    {
      emit fileProgress(progress);
      if ( interactive || verbose )
        emit logging(i18n("...archiving file %1").arg(info.absoluteFilePath()));

      if ( interactive )
        QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
      qApp->processEvents(QEventLoop::AllEvents, 5);
      msgShown = true;
    }
  }
  emit fileProgress(100);
  sourceFile.close();

  if ( !cancelled )
  {
    // get filesize
    sliceBytes = archive->device()->pos();  // account for tar overhead

    emit sliceProgress(static_cast<int>(sliceBytes * 100 / sliceCapacity));
  }

  if ( msgShown && interactive )
    QApplication::restoreOverrideCursor();

  if ( !cancelled && !archive->finishWriting(sourceStat.st_size) )
  {
    emit warning(i18n("Could not write to archive. Maybe the medium is full."));
    return false;
  }

  return !cancelled;
}

//--------------------------------------------------------------------------------

bool Archiver::compressFile(const QString &origName, QFile &comprFile)
{
  QFile origFile(origName);
  if ( ! origFile.open(QIODevice::ReadOnly) )
  {
    emit warning(i18n("Could not read file: %1\n"
                      "The operating system reports: %2")
                 .arg(origName)
                 .arg(origFile.errorString()));

    return false;
  }
  else
  {
    QIODevice *filter = KFilterDev::device(&comprFile,
                          ext == ".bz2" ? "application/x-bzip2" : "application/x-gzip",
                          false);  // don't delete comprFile

    if ( ! filter->open(QIODevice::WriteOnly) )
    {
      emit warning(i18n("Could not create temporary file for compressing: %1\n"
                        "The operating system reports: %2")
                   .arg(origName)
                   .arg(filter->errorString()));
      return false;
    }

    const int BUFFER_SIZE = 8*1024;
    static char buffer[BUFFER_SIZE];
    qint64 len;
    int count = 0, progress;
    QTime timer;
    timer.start();
    bool msgShown = false;

    KIO::filesize_t fileSize = origFile.size();
    KIO::filesize_t written = 0;

    while ( fileSize && !origFile.atEnd() && !cancelled )
    {
      len = origFile.read(buffer, BUFFER_SIZE);
      qint64 wrote = filter->write(buffer, len);

      if ( len != wrote )
      {
        emit warning(i18n("Could not write to temporary file"));
        delete filter;
        return false;
      }

      written += len;

      progress = static_cast<int>(written * 100 / fileSize);

      // keep the ui responsive
      count = (count + 1) % 50;
      if ( count == 0 )
      {
        if ( msgShown )
          emit fileProgress(progress);

        qApp->processEvents(QEventLoop::AllEvents, 5);
      }

      if ( !msgShown && (timer.elapsed() > 3000) && (progress < 50) )
      {
        emit fileProgress(progress);
        emit logging(i18n("...compressing file %1").arg(origName));
        if ( interactive )
          QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
        qApp->processEvents(QEventLoop::AllEvents, 5);
        msgShown = true;
      }
    }
    emit fileProgress(100);
    origFile.close();
    delete filter;

    if ( msgShown && interactive )
      QApplication::restoreOverrideCursor();
  }

  return true;
}

//--------------------------------------------------------------------------------

bool Archiver::getDiskFree(const QString &path, KIO::filesize_t &capacityB, KIO::filesize_t &freeB)
{
  struct statvfs vfs;
  memset(&vfs, 0, sizeof(vfs));

  if ( ::statvfs(QFile::encodeName(path), &vfs) == -1 )
    return false;

  capacityB = static_cast<KIO::filesize_t>(vfs.f_blocks) * static_cast<KIO::filesize_t>(vfs.f_frsize);
  freeB = static_cast<KIO::filesize_t>(vfs.f_bavail) * static_cast<KIO::filesize_t>(vfs.f_frsize);

  return true;
}

//--------------------------------------------------------------------------------

void Archiver::loggingSlot(const QString &message)
{
  std::cerr << message.toUtf8().constData() << std::endl;
}

//--------------------------------------------------------------------------------

void Archiver::warningSlot(const QString &message)
{
  std::cerr << i18n("WARNING:").toUtf8().constData() << message.toUtf8().constData() << std::endl;
}

//--------------------------------------------------------------------------------

void Archiver::updateElapsed()
{
  emit elapsedChanged(QTime().addMSecs(elapsed.elapsed()));
}

//--------------------------------------------------------------------------------
// sort by name of entries in descending order (younger names are first)

bool Archiver::UDSlessThan(KIO::UDSEntry &left, KIO::UDSEntry &right)
{
  return left.stringValue(KIO::UDSEntry::UDS_NAME) > right.stringValue(KIO::UDSEntry::UDS_NAME);
}

//--------------------------------------------------------------------------------
