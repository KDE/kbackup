//**************************************************************************
//   (c) 2006 - 2009 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#ifndef _ARCHIVER_H_
#define _ARCHIVER_H_

// the class which does the archiving

#include <qobject.h>
#include <qmap.h>
#include <qpointer.h>
#include <QTimer>
#include <QTime>
#include <QStringList>
#include <kurl.h>
#include <kio/copyjob.h>

class KTar;
class KProcess;
class QDir;
class QFileInfo;
class QFile;


class Archiver : public QObject
{
  Q_OBJECT

  public:
    Archiver(QWidget *parent);

    static Archiver *instance;

    // always call after you have already set maxSliceMBs, as the sliceCapacity
    // might be limited with it
    void setTarget(const KUrl &target);
    const KUrl &getTarget() const { return targetURL; }

    enum { UNLIMITED = 0 };
    void setMaxSliceMBs(int mbs);
    int getMaxSliceMBs() const { return maxSliceMBs; }

    void setFilePrefix(const QString &prefix);
    const QString &getFilePrefix() const { return filePrefix; }

    void setMediaNeedsChange(bool b) { mediaNeedsChange = b; }
    bool getMediaNeedsChange() const { return mediaNeedsChange; }

    void setCompressFiles(bool b);
    bool getCompressFiles() const { return !ext.isEmpty(); }

    // number of backups to keep before older ones will be deleted (UNLIMITED or 1..n)
    void setKeptBackups(int num);
    int getKeptBackups() const { return numKeptBackups; }

    // print every single file/dir in non-interactive mode
    void setVerbose(bool b) { verbose = b; }

    // loads the profile into the Archiver and returns includes/excludes lists
    // return true if loaded, false on file open error
    bool loadProfile(const QString &fileName, QStringList &includes, QStringList &excludes, QString &error);

    // return true if the backup completed successfully, else false
    bool createArchive(const QStringList &includes, const QStringList &excludes);

    KIO::filesize_t getTotalBytes() const { return totalBytes; }
    int getTotalFiles() const { return totalFiles; }

    bool isInProgress() const { return runs; }

    static bool getDiskFree(const QString &path, KIO::filesize_t &capacityBytes, KIO::filesize_t &freeBytes);

    // TODO: put probably in some global settings object
    static QString sliceScript;

  public slots:
    void cancel();  // cancel a running creation

  signals:
    void inProgress(bool runs);
    void logging(const QString &);
    void warning(const QString &);
    void targetCapacity(KIO::filesize_t bytes);
    void sliceProgress(int percent);
    void fileProgress(int percent);
    void newSlice(int);
    void totalFilesChanged(int);
    void totalBytesChanged(KIO::filesize_t);
    void elapsedChanged(const QTime &);

  private slots:
    void slotResult(KJob *);
    void slotListResult(KIO::Job *, const KIO::UDSEntryList &);
    void receivedStderr(KProcess *proc, char *buffer, int buflen);
    void loggingSlot(const QString &message); // for non-interactive output
    void warningSlot(const QString &message); // for non-interactive output
    void updateElapsed();

  private:
    void calculateCapacity();  // also emits signals
    void addDirFiles(QDir &dir);
    void addFile(const QFileInfo &info);
    bool addLocalFile(const QFileInfo &info);
    bool compressFile(const QString &origName, QFile &comprFile);

    void finishSlice();
    bool getNextSlice();

    void runScript(const QString &mode);

    static bool UDSlessThan(KIO::UDSEntry &left, KIO::UDSEntry &right);

  private:
    QMap<QString, char> excludeFiles;
    QMap<QString, char> excludeDirs;

    QString archiveName;
    QString filePrefix;  // default = "backup"
    QStringList sliceList;

    KTar *archive;
    KIO::filesize_t totalBytes;
    int totalFiles;
    QTime elapsed;

    KUrl targetURL;
    QString baseName;
    int sliceNum;
    int maxSliceMBs;
    bool mediaNeedsChange;
    bool compressFiles;

    int numKeptBackups;
    KIO::UDSEntryList targetDirList;

    KIO::filesize_t sliceBytes;
    KIO::filesize_t sliceCapacity;

    QString ext;

    bool interactive;
    bool cancelled;
    bool runs;
    bool skippedFiles;  // did we skip files during backup ?
    bool verbose;

    QPointer<KIO::CopyJob> job;
    int jobResult;
};

#endif
