//**************************************************************************
//   Copyright 2006 - 2017 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#include <MainWidget.hxx>
#include <Archiver.hxx>
#include <Selector.hxx>

#include <kiconloader.h>
#include <kurlcompletion.h>

#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QFileDialog>

//--------------------------------------------------------------------------------

MainWidget::MainWidget(QWidget *parent)
  : QWidget(parent), selector(0)
{
  ui.setupUi(this);

  ui.startButton->setIcon(SmallIcon("kbackup_start", 22));
  ui.cancelButton->setIcon(SmallIcon("kbackup_cancel", 22));
  ui.folder->setIcon(SmallIcon("folder"));

  connect(ui.startButton,  SIGNAL(clicked()), this, SLOT(startBackup()));
  connect(ui.cancelButton, SIGNAL(clicked()), Archiver::instance, SLOT(cancel()));

  connect(ui.forceFullBackup, SIGNAL(clicked(bool)), Archiver::instance, SLOT(setForceFullBackup(bool)));

  connect(Archiver::instance, SIGNAL(logging(const QString &)), ui.log, SLOT(append(const QString &)));
  connect(Archiver::instance, SIGNAL(warning(const QString &)), ui.warnings, SLOT(append(const QString &)));

  connect(Archiver::instance, SIGNAL(targetCapacity(KIO::filesize_t)), this, SLOT(setCapacity(KIO::filesize_t)));

  connect(Archiver::instance, SIGNAL(totalFilesChanged(int)), ui.totalFiles, SLOT(setNum(int)));
  connect(Archiver::instance, SIGNAL(totalBytesChanged(KIO::filesize_t)), this, SLOT(updateTotalBytes()));

  connect(Archiver::instance, SIGNAL(sliceProgress(int)), ui.progressSlice, SLOT(setValue(int)));
  connect(Archiver::instance, SIGNAL(newSlice(int)), ui.sliceNum, SLOT(setNum(int)));

  connect(Archiver::instance, SIGNAL(fileProgress(int)), this, SLOT(setFileProgress(int)));
  connect(Archiver::instance, SIGNAL(elapsedChanged(const QTime &)), this, SLOT(updateElapsed(const QTime &)));

  connect(Archiver::instance, SIGNAL(backupTypeChanged(bool)), this, SLOT(setIsIncrementalBackup(bool)));

  connect(ui.folder, SIGNAL(clicked()), this, SLOT(getMediaSize()));
  connect(ui.targetDir, SIGNAL(returnPressed(const QString &)), this, SLOT(setTargetURL(const QString &)));

  KUrlCompletion *kc = new KUrlCompletion(KUrlCompletion::DirCompletion);
  ui.targetDir->setCompletionObject(kc);

  Archiver::instance->setForceFullBackup(ui.forceFullBackup->isChecked());
}

//--------------------------------------------------------------------------------

void MainWidget::setIsIncrementalBackup(bool incremental)
{
  if ( incremental )
    ui.backupType->setText(i18n("Incremental Backup"));
  else
    ui.backupType->setText(i18n("Full Backup"));
}

//--------------------------------------------------------------------------------

void MainWidget::startBackup()
{
  ui.log->clear();
  ui.warnings->clear();
  ui.cancelButton->setEnabled(true);
  ui.startButton->setEnabled(false);

  Archiver::instance->setTarget(QUrl::fromUserInput(ui.targetDir->text()));

  QStringList includes, excludes;
  selector->getBackupList(includes, excludes);

  Archiver::instance->createArchive(includes, excludes);

  ui.forceFullBackup->setChecked(false);
  Archiver::instance->setForceFullBackup(false);

  ui.cancelButton->setEnabled(false);
  ui.startButton->setEnabled(true);
}

//--------------------------------------------------------------------------------

void MainWidget::setSelector(Selector *s)
{
  setCapacity(0);
  setFileProgress(100);  // to hide file progress bar

  selector = s;
}

//--------------------------------------------------------------------------------

void MainWidget::getMediaSize()
{
  QUrl url = QFileDialog::getExistingDirectoryUrl(this);

  if ( url.isEmpty() ) return;  // cancelled

  ui.targetDir->setText(url.toLocalFile());
  Archiver::instance->setTarget(ui.targetDir->text());
}

//--------------------------------------------------------------------------------

void MainWidget::updateElapsed(const QTime &elapsed)
{
  ui.elapsedTime->setText(elapsed.toString("HH:mm:ss"));
}

//--------------------------------------------------------------------------------

void MainWidget::setTargetURL(const QString &url)
{
  ui.targetDir->setText(url);
  Archiver::instance->setTarget(ui.targetDir->text());
}

//--------------------------------------------------------------------------------

void MainWidget::updateTotalBytes()
{
  // don't use KIO::convertSize() as this would not show good progress
  // after reaching 1 GB; always show MBs
  ui.totalSize->setText(
    QString::number(Archiver::instance->getTotalBytes() / 1024.0 / 1024.0, 'f', 2));
}

//--------------------------------------------------------------------------------

void MainWidget::setFileProgress( int percent )
{
  if ( percent == 100 )
  {
    ui.fileProgressLabel->hide();
    ui.fileProgress->hide();
  }
  else
  {
    ui.fileProgressLabel->show();
    ui.fileProgress->show();
    ui.fileProgress->setValue(percent);
  }
}

//--------------------------------------------------------------------------------

void MainWidget::setCapacity(KIO::filesize_t bytes)
{
  if ( bytes == 0 )
    ui.capacity->setText(i18n("unlimited"));
  else
  {
    QString txt = KIO::convertSize(bytes);
    if ( Archiver::instance->getMaxSliceMBs() != Archiver::UNLIMITED )
      txt += " (*)";
    ui.capacity->setText(txt);
  }
}

//--------------------------------------------------------------------------------
