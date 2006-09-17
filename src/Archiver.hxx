#ifndef _ARCHIVER_H_
#define _ARCHIVER_H_

/***************************************************************************
 *   (c) 2006, Martin Koller, m.koller@surfeu.at                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, version 2 of the License                *
 *                                                                         *
 ***************************************************************************/


// the class which does the archiving

#include <qobject.h>
#include <qmap.h>
#include <qguardedptr.h>
#include <kurl.h>
#include <kio/job.h>

class KTar;
class KFilterBase;
class KProcess;
class QDir;
class QFileInfo;
class QStringList;


class Archiver : public QObject
{
  Q_OBJECT

  public:
    Archiver(QWidget *parent);

    void createArchive(const KURL &target, const QStringList &includes, const QStringList &excludes);

    KIO::filesize_t getTotalBytes() const { return totalBytes; }

    static bool getDiskFree(const QString &path, KIO::filesize_t &capacityBytes, KIO::filesize_t &freeBytes);

    // TODO: put probably in some global settings object
    static QString sliceScript;
    static QString filePrefix;  // default = "backup"

  public slots:
    void cancel();  // cancel a running creation

  signals:
    void logging(const QString &);
    void warning(const QString &);
    void targetCapacity(KIO::filesize_t bytes);
    void sliceProgress(int percent);
    void fileProgress(int percent);
    void newSlice(int);
    void totalFilesChanged(int);
    void totalBytesChanged(KIO::filesize_t);

  private slots:
    void slotResult(KIO::Job *);
    void receivedStderr(KProcess *proc, char *buffer, int buflen);

  private:
    void addDirFiles(QDir &dir);
    void addFile(const QFileInfo &info);
    bool addLocalFile(const QFileInfo &info);
    bool compressFile(const QString &origName, const QString &comprName);

    void finishSlice();
    bool getNextSlice();

    void runScript(const QString &mode);

  private:
    enum { UNLIMITED = 0 };

    QMap<QString, char> excludeFiles;
    QMap<QString, char> excludeDirs;

    QString archiveName;

    KTar *archive;
    KIO::filesize_t totalBytes;
    uint totalFiles;

    KURL targetURL;
    QString baseName;
    int sliceNum;
    KIO::filesize_t startSize;
    KIO::filesize_t sliceBytes;
    KIO::filesize_t sliceCapacity;

    KFilterBase *filterBase;
    QString ext;

    bool cancelled;

    QGuardedPtr<KIO::CopyJob> job;
    int jobResult;
};

#endif
