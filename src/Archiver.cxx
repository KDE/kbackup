//**************************************************************************
//   (c) 2006, 2007 Martin Koller, m.koller@surfeu.at
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
#include <ktempfile.h>
#include <kfilterbase.h>
#include <kfilterdev.h>
#include <kstandarddirs.h>
#include <kio/job.h>
#include <kprocess.h>
#include <kde_file.h>

#include <qapplication.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qcursor.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>

//#include <iostream>
//using namespace std;
//--------------------------------------------------------------------------------

QString Archiver::sliceScript;
Archiver *Archiver::instance;

//--------------------------------------------------------------------------------

Archiver::Archiver(QWidget *parent)
  : QObject(parent),
    archive(0), totalBytes(0), totalFiles(0), sliceNum(0), mediaNeedsChange(true),
    sliceCapacity(0), cancelled(false), runs(false), jobResult(0)
{
  instance = this;

  maxSliceMBs = Archiver::UNLIMITED;

  setCompressFiles(true);
}

//--------------------------------------------------------------------------------

void Archiver::setCompressFiles(bool b)
{
  if ( b )
  {
    ext = ".bz2";
    filterBase = KFilterBase::findFilterByMimeType("application/x-bzip2");
    if ( !filterBase )
    {
      filterBase = KFilterBase::findFilterByMimeType("application/x-gzip");
      ext = ".gz";
    }
  }
  else
  {
    ext = "";
    filterBase = 0;
  }
}

//--------------------------------------------------------------------------------

void Archiver::setTarget(const KURL &target)
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

void Archiver::setFilePrefix(const QString &prefix)
{
  filePrefix = prefix;
}

//--------------------------------------------------------------------------------

void Archiver::calculateCapacity()
{
  if ( targetURL.isLocalFile() || (maxSliceMBs != UNLIMITED) )
  {
    KIO::filesize_t freeBytes = 0;

    if ( targetURL.isLocalFile() )
      getDiskFree(targetURL.path(), sliceCapacity, freeBytes);

    if ( maxSliceMBs != UNLIMITED )
    {
      KIO::filesize_t max = static_cast<KIO::filesize_t>(maxSliceMBs) * 1024 * 1024;

      if ( targetURL.isLocalFile() )
        sliceCapacity = QMIN(freeBytes, max);  // still limit to the size we have available
      else
        sliceCapacity = max;  // remote target

      freeBytes = sliceCapacity;
    }

    startSize = sliceBytes = sliceCapacity - freeBytes;  // how much is already used on that medium

    if ( sliceCapacity == 0 )
      emit sliceProgress(100);
    else
      emit sliceProgress(static_cast<int>(sliceBytes * 100 / sliceCapacity));

    emit targetCapacity(sliceCapacity);
  }
  else
  {
    sliceCapacity = UNLIMITED;
    startSize = sliceBytes = 0;
    emit targetCapacity(0);
  }
}

//--------------------------------------------------------------------------------

void Archiver::createArchive(const QStringList &includes, const QStringList &excludes)
{
  if ( includes.count() == 0 )
  {
    emit warning(i18n("Nothing selected for backup"));
    return;
  }

  if ( ! targetURL.isValid() )
  {
    emit warning(i18n("The target dir is not valid"));
    return;
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
  cancelled = false;

  if ( ! getNextSlice() ) return;

  runs = true;
  emit inProgress(true);

  for (QStringList::const_iterator it = includes.begin(); !cancelled && (it != includes.end()); ++it)
  {
    QString entry = *it;

    if ( entry.endsWith("/") )
      entry.truncate(entry.length() - 1);

    QFileInfo info(entry);

    if ( !info.isSymLink() && info.isDir() )
    {
      QDir dir(info.absFilePath());
      addDirFiles(dir);
    }
    else
      addFile(info.absFilePath());
  }

  finishSlice();

  runs = false;
  emit inProgress(false);

  if ( !cancelled )
  {
    emit logging(i18n("-- Backup successfully finished --"));
    KMessageBox::information(static_cast<QWidget*>(parent()),
                             i18n("The backup has finished successfully."), QString::null, "showDoneInfo");
  }
  else
    emit logging(i18n("...Backup aborted!"));
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
    QFile(archiveName).remove(); // remove the unfinished tar file (which is now corrupted)
    emit warning(i18n("Backup cancelled"));
  }
}

//--------------------------------------------------------------------------------

void Archiver::finishSlice()
{
  if ( archive )
    archive->close();

  if ( sliceCapacity == UNLIMITED )
  {
    if ( filterBase )  // user wants compression
    {
      // now let's compress the file
      if ( ! cancelled )
        compressFile(archiveName, archiveName + ext);

      QFile(archiveName).remove();  // remove uncompressed file

      archiveName += ext; // this is now the final name
    }
  }

  if ( ! cancelled )
  {
    runScript("slice_closed");

    if ( targetURL.isLocalFile() )
      emit logging(i18n("...finished slice %1").arg(archiveName));
  }

  if ( !cancelled && !targetURL.isLocalFile() )
  {
    KURL source;
    source.setPath(archiveName);

    while ( true )
    {
      job = KIO::copy(source, targetURL);  // copy to have the archive for the script later down
      job->setWindow(static_cast<QWidget*>(parent()));

      connect(job, SIGNAL(result(KIO::Job *)), this, SLOT(slotResult(KIO::Job *)));

      emit logging(i18n("...uploading archive %1 to %2").arg(source.fileName()).arg(targetURL.prettyURL()));

      while ( job )
        qApp->processEvents();

      if ( jobResult == 0 )
        break;
      else
      {
        if ( KMessageBox::warningYesNo(static_cast<QWidget*>(parent()),
               i18n("Do you want to retry the upload?")) == KMessageBox::No )
        {
          break;
        }
      }
    }

    if ( jobResult != 0 )
      cancel();
  }

  if ( ! cancelled )
    runScript("slice_finished");

  if ( !targetURL.isLocalFile() )
    QFile(archiveName).remove(); // remove the tmp file

  delete archive;
  archive = 0;
}

//--------------------------------------------------------------------------------

void Archiver::slotResult(KIO::Job *theJob)
{
  if ( (jobResult = theJob->error()) )
    theJob->showErrorDialog(static_cast<QWidget*>(parent()));

  // NOTE: I really wanted to show the errorText on my own, but I could not get
  // rid of the automatic popup window of KIO (even with setAuto*HandlingEnabled(false))
}

//--------------------------------------------------------------------------------

void Archiver::runScript(const QString &mode)
{
  // do some extra action via external script (program)
  if ( sliceScript.length() )
  {
    QString mountPoint;
    if ( targetURL.isLocalFile() )
      mountPoint = KIO::findPathMountPoint(targetURL.path());

    KProcess *proc = new KProcess;
    *proc << sliceScript
          << mode
          << QFile::encodeName(archiveName)
          << QFile::encodeName(KURL_pathOrURL(targetURL))
          << QFile::encodeName(mountPoint);

    connect(proc, SIGNAL(receivedStdout(KProcess *, char *, int )),
            this, SLOT(receivedStderr(KProcess *, char *, int )));

    connect(proc, SIGNAL(receivedStderr(KProcess *, char *, int )),
            this, SLOT(receivedStderr(KProcess *, char *, int )));

    if ( ! proc->start(KProcess::NotifyOnExit, KProcess::AllOutput) )
    {
      KMessageBox::error(static_cast<QWidget*>(parent()),
                        i18n("The script '%1' could not be started.").arg(sliceScript));
    }
    else
    {
      while ( proc->isRunning() )
        qApp->processEvents();
    }

    delete proc;
  }
}

//--------------------------------------------------------------------------------

void Archiver::receivedStderr(KProcess *, char *buffer, int )
{
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

    if ( mediaNeedsChange &&
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
      baseName = ::locateLocal("tmp", prefix + QDateTime::currentDateTime().toString("_yyyy.MM.dd-hh.mm.ss"));
  }

  archiveName = baseName + QString("_%1.tar").arg(sliceNum);

  runScript("slice_init");

  calculateCapacity();

  // don't create a bz2 compressed file as we compress each file on its own
  // Even if we have sliceCapacity == UNLIMITED, as we will compress that later
  archive = new KTar(archiveName, "application/x-tar");

  while ( ! archive->open(IO_WriteOnly) )
  {
    if ( KMessageBox::warningYesNo(static_cast<QWidget*>(parent()),
           i18n("The file '%1' can not be opened for writing.\n\n"
                "Do you want to retry?").arg(archiveName)) == KMessageBox::No )
    {
      delete archive;
      archive = 0;

      cancel();
      return false;
    }
  }

  return true;
}

//--------------------------------------------------------------------------------

void Archiver::addDirFiles(QDir &dir)
{
  if ( excludeDirs.contains(dir.absPath()) )
    return;

  // add the dir itself
  KDE_struct_stat status;
  memset(&status, 0, sizeof(status));
  if ( KDE_stat(QFile::encodeName(dir.absPath()), &status) == -1 )
  {
    emit warning(i18n("Could not get information of directory: %1\n"
                      "The operating system reports: %2")
                 .arg(dir.absPath())
                 .arg(strerror(errno)));
    return;
  }
  QFileInfo dirInfo(dir.absPath());

  if ( ! dirInfo.isReadable() )
  {
    emit warning(i18n("Directory '%1' is not readable. Skipping.").arg(dir.absPath()));
    return;
  }

  totalFiles++;
  emit totalFilesChanged(totalFiles);
  emit logging(dir.absPath());
  qApp->processEvents(5);

  if ( ! archive->writeDir(QString(".") + dir.absPath(), dirInfo.owner(), dirInfo.group(),
                           status.st_mode, status.st_atime, status.st_mtime, status.st_ctime) )
  {
    emit warning(i18n("Could not write directory '%1' to archive.\n"
                      "Maybe the medium is full.").arg(dir.absPath()));
    return;
  }

  dir.setFilter(QDir::All | QDir::Hidden);

  const QFileInfoList *list = dir.entryInfoList();
  if ( !list ) return;

  QFileInfoListIterator it(*list);
  QFileInfo *info;

  for (; !cancelled && ((info = it.current()) != 0); ++it)
  {
    if ( (info->fileName() == ".") || (info->fileName() == "..") ) continue;

    if ( !info->isSymLink() && info->isDir() )
    {
      QDir dir(info->absFilePath());
      addDirFiles(dir);
    }
    else
      addFile(info->absFilePath());
  }
}

//--------------------------------------------------------------------------------

void Archiver::addFile(const QFileInfo &info)
{
  if ( excludeFiles.contains(info.absFilePath()) )
    return;

  if ( cancelled ) return;

  if ( ! info.isReadable() )
  {
    emit warning(i18n("File '%1' is not readable. Skipping.").arg(info.absFilePath()));
    return;
  }

  // emit before we do the compression, so that the receiver can already show
  // with which file we work

  emit logging(info.absFilePath());
  qApp->processEvents(5);

  if ( info.isSymLink() )
  {
    archive->addLocalFile(info.absFilePath(), QString(".") + info.absFilePath());
    totalFiles++;
    emit totalFilesChanged(totalFiles);
    return;
  }

  if ( (sliceCapacity == UNLIMITED) || !getCompressFiles() )
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
    KTempFile tmpFile;
    tmpFile.close();  // we only need the name

    if ( ! compressFile(info.absFilePath(), tmpFile.name()) || cancelled )
    {
      tmpFile.unlink();
      return;
    }

    // here we have the compressed file in tmpFile

    // get stat (size) from the now compressed file
    KDE_struct_stat compressedStatus;
    memset(&compressedStatus, 0, sizeof(compressedStatus));

    // QFileInfo has no large file support (only files up to 2GB)
    KDE_stat(QFile::encodeName(tmpFile.name()), &compressedStatus);

    if ( sliceCapacity && ((sliceBytes + compressedStatus.st_size) > sliceCapacity) )
      if ( ! getNextSlice() ) return;

    // to be able to create the exact same metadata (permission, date, owner) we need
    // to fill the file into the archive with the following:
    {
      KDE_struct_stat status;
      memset(&status, 0, sizeof(status));

      if ( KDE_stat(QFile::encodeName(info.absFilePath()), &status) == -1 )
      {
        emit warning(i18n("Could not get information of file: %1\n"
                          "The operating system reports: %2")
                     .arg(info.absFilePath())
                     .arg(strerror(errno)));

        tmpFile.unlink();
        return;
      }

      if ( ! archive->prepareWriting(QString(".") + info.absFilePath() + ext,
                                     info.owner(), info.group(), compressedStatus.st_size,
                                     status.st_mode, status.st_atime, status.st_mtime, status.st_ctime) )
      {
        emit warning(i18n("Could not write to archive. Maybe the medium is full."));
        tmpFile.unlink();
        cancel();
        return;
      }

      QFile compressedFile(tmpFile.name());
      compressedFile.open(IO_ReadOnly);
      static QByteArray buffer(8*1024);
      Q_LONG len;
      int count = 0;
      while ( ! compressedFile.atEnd() )
      {
        len = compressedFile.readBlock(buffer.data(), buffer.size());
        if ( ! archive->writeData(buffer.data(), len) )
        {
          emit warning(i18n("Could not write to archive. Maybe the medium is full."));
          tmpFile.unlink();
          cancel();
          return;
        }

        count = (count + 1) % 50;
        if ( count == 0 )
          qApp->processEvents(5);
      }
      compressedFile.close();
      if ( ! archive->doneWriting(compressedStatus.st_size) )
      {
        emit warning(i18n("Could not write to archive. Maybe the medium is full."));
        tmpFile.unlink();
        cancel();
        return;
      }
    }

    // get filesize
    KDE_struct_stat archiveStat;
    memset(&archiveStat, 0, sizeof(archiveStat));
    KDE_stat(QFile::encodeName(archiveName), &archiveStat);

    sliceBytes = startSize + archiveStat.st_size;  // account for tar overhead
    totalBytes += compressedStatus.st_size;

    tmpFile.unlink();

    if ( sliceCapacity )
      emit sliceProgress(static_cast<int>(sliceBytes * 100 / sliceCapacity));
    else
      emit sliceProgress(100);
  }

  totalFiles++;
  emit totalFilesChanged(totalFiles);
  emit totalBytesChanged(totalBytes);

  qApp->processEvents(5);
}

//--------------------------------------------------------------------------------

bool Archiver::addLocalFile(const QFileInfo &info)
{
  KDE_struct_stat sourceStat;
  memset(&sourceStat, 0, sizeof(sourceStat));

  if ( KDE_stat(QFile::encodeName(info.absFilePath()), &sourceStat) == -1 )
  {
    emit warning(i18n("Could not get information of file: %1\n"
                      "The operating system reports: %2")
                 .arg(info.absFilePath())
                 .arg(strerror(errno)));

    return false;
  }

  QFile sourceFile(info.absFilePath());
  if ( ! sourceFile.open(IO_ReadOnly) )
  {
    emit warning(i18n("Could not open file '%1' for reading.").arg(info.absFilePath()));
    return false;
  }

  if ( sliceCapacity && ((sliceBytes + sourceStat.st_size) > sliceCapacity) )
    if ( ! getNextSlice() ) return false;

  if ( ! archive->prepareWriting(QString(".") + info.absFilePath(),
                                 info.owner(), info.group(), sourceStat.st_size,
                                 sourceStat.st_mode, sourceStat.st_atime, sourceStat.st_mtime, sourceStat.st_ctime) )
  {
    emit warning(i18n("Could not write to archive. Maybe the medium is full."));
    return false;
  }

  static QByteArray buffer(8*1024);
  Q_LONG len;
  int count = 0, progress;
  QTime timer;
  timer.start();
  bool msgShown = false;
  KIO::filesize_t fileSize = sourceStat.st_size;
  KIO::filesize_t written = 0;

  while ( fileSize && !sourceFile.atEnd() && !cancelled )
  {
    len = sourceFile.readBlock(buffer.data(), buffer.size());
    if ( ! archive->writeData(buffer.data(), len) )
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
      qApp->processEvents(5);
    }

    if ( !msgShown && (timer.elapsed() > 3000) && (progress < 50) )
    {
      emit fileProgress(progress);
      emit logging(i18n("...archiving file %1").arg(info.absFilePath()));
      QApplication::setOverrideCursor(QCursor(QCursor::BusyCursor));
      qApp->processEvents(5);
      msgShown = true;
    }
  }
  emit fileProgress(100);
  sourceFile.close();

  // get filesize
  KDE_struct_stat archiveStat;
  memset(&archiveStat, 0, sizeof(archiveStat));
  KDE_stat(QFile::encodeName(archiveName), &archiveStat);

  sliceBytes = startSize + archiveStat.st_size;  // account for tar overhead
  if ( sliceCapacity )
    emit sliceProgress(static_cast<int>(sliceBytes * 100 / sliceCapacity));
  else
    emit sliceProgress(100);

  if ( msgShown )
    QApplication::restoreOverrideCursor();

  if ( !cancelled && !archive->doneWriting(sourceStat.st_size) )
  {
    emit warning(i18n("Could not write to archive. Maybe the medium is full."));
    return false;
  }

  return !cancelled;
}

//--------------------------------------------------------------------------------

bool Archiver::compressFile(const QString &origName, const QString &comprName)
{
  QFile origFile(origName);
  if ( ! origFile.open(IO_ReadOnly) )
  {
    emit warning(i18n("Could not read file: %1\n"
                      "The operating system reports: %2")
                 .arg(origName)
                 .arg(strerror(errno)));

    return false;
  }
  else
  {
    QFile tmpQFile(comprName);

    filterBase->setDevice(&tmpQFile);
    KFilterDev filter(filterBase, false);

    filter.open(IO_WriteOnly);
    static QByteArray buffer(8*1024);
    Q_LONG len;
    int count = 0, progress;
    QTime timer;
    timer.start();
    bool msgShown = false;

    // get filesize
    KDE_struct_stat origStat;
    memset(&origStat, 0, sizeof(origStat));
    KDE_stat(QFile::encodeName(origName), &origStat);

    KIO::filesize_t fileSize = origStat.st_size;
    KIO::filesize_t written = 0;

    while ( fileSize && !origFile.atEnd() && !cancelled )
    {
      len = origFile.readBlock(buffer.data(), buffer.size());
      filter.writeBlock(buffer.data(), len);

      written += len;

      progress = static_cast<int>(written * 100 / fileSize);

      // keep the ui responsive
      count = (count + 1) % 50;
      if ( count == 0 )
      {
        if ( msgShown )
          emit fileProgress(progress);

        qApp->processEvents(5);
      }

      if ( !msgShown && (timer.elapsed() > 3000) && (progress < 50) )
      {
        emit fileProgress(progress);
        emit logging(i18n("...compressing file %1").arg(origName));
        QApplication::setOverrideCursor(QCursor(QCursor::BusyCursor));
        qApp->processEvents(5);
        msgShown = true;
      }
    }
    emit fileProgress(100);
    origFile.close();
    filter.close();

    if ( cancelled )
      tmpQFile.remove();

    if ( msgShown )
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

QString KURL_pathOrURL(const KURL &kurl)
{
  if ( kurl.isLocalFile() && kurl.ref().isNull() && kurl.query().isNull() )
    return kurl.path();
  else
    return kurl.prettyURL();
}

//--------------------------------------------------------------------------------
