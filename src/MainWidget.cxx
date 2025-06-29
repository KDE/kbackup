//**************************************************************************
//   Copyright 2006 - 2017 Martin Koller, martin@kollix.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#include <MainWidget.hxx>
#include <Archiver.hxx>
#include <Selector.hxx>

#include <KUrlCompletion>
#include <KLineEdit>

#include <QPushButton>
#include <QFileDialog>

//krazy:excludeall=normalize
//--------------------------------------------------------------------------------

MainWidget::MainWidget(QWidget *parent)
  : QWidget(parent), selector(nullptr)
{
  ui.setupUi(this);

  ui.startButton->setIcon(QIcon::fromTheme(QStringLiteral("kbackup_start")));
  ui.cancelButton->setIcon(QIcon::fromTheme(QStringLiteral("kbackup_cancel")));
  ui.folder->setIcon(QIcon::fromTheme(QStringLiteral("folder")));

  connect(ui.startButton,  &QAbstractButton::clicked, this, &MainWidget::startBackup);
  connect(ui.cancelButton, &QAbstractButton::clicked, Archiver::instance, &Archiver::cancel);

  connect(ui.forceFullBackup, &QAbstractButton::clicked, Archiver::instance, &Archiver::setForceFullBackup);

  connect(Archiver::instance, &Archiver::logging, ui.log, &QTextEdit::append);
  connect(Archiver::instance, &Archiver::warning, ui.warnings, &QTextEdit::append);

  connect(Archiver::instance, &Archiver::targetCapacity, this, &MainWidget::setCapacity);

  connect(Archiver::instance, SIGNAL(totalFilesChanged(int)), ui.totalFiles, SLOT(setNum(int)));
  connect(Archiver::instance, &Archiver::totalBytesChanged, this, &MainWidget::updateTotalBytes);

  connect(Archiver::instance, &Archiver::sliceProgress, ui.progressSlice, &QProgressBar::setValue);
  connect(Archiver::instance, SIGNAL(newSlice(int)), ui.sliceNum, SLOT(setNum(int)));

  connect(Archiver::instance, &Archiver::fileProgress, this, &MainWidget::setFileProgress);
  connect(Archiver::instance, &Archiver::elapsedChanged, this, &MainWidget::updateElapsed);

  connect(Archiver::instance, &Archiver::backupTypeChanged, this, &MainWidget::setIsIncrementalBackup);

  connect(ui.folder, &QAbstractButton::clicked, this, &MainWidget::getMediaSize);
  connect(ui.targetDir, &KLineEdit::returnKeyPressed, this, &MainWidget::setTargetURL);

  KUrlCompletion *kc = new KUrlCompletion(KUrlCompletion::DirCompletion);
  ui.targetDir->setCompletionObject(kc);
  ui.targetDir->setAutoDeleteCompletionObject(true);

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
  Archiver::instance->setTarget(QUrl::fromUserInput(ui.targetDir->text()));
}

//--------------------------------------------------------------------------------

void MainWidget::updateElapsed(const QTime &elapsed)
{
  ui.elapsedTime->setText(elapsed.toString(QStringLiteral("HH:mm:ss")));
}

//--------------------------------------------------------------------------------

void MainWidget::setTargetURL(const QString &url)
{
  ui.targetDir->setText(url);
  Archiver::instance->setTarget(QUrl::fromUserInput(ui.targetDir->text()));
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
      txt += QStringLiteral(" (*)");
    ui.capacity->setText(txt);
  }
}

//--------------------------------------------------------------------------------

#include "moc_MainWidget.cpp"
