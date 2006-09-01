/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/


void MainWidget::startBackup()
{
  log->clear();
  warnings->clear();
  elapsed.start();
  cancelButton->setEnabled(true);
  startButton->setEnabled(false);
  
  QTimer timer;
  connect(&timer, SIGNAL(timeout()), this, SLOT(updateElapsed()));
  timer.start(1000);
  
  QStringList includes, excludes;
  selector->getBackupList(includes, excludes);
  
  archiver->createArchive(targetDir->text(), includes, excludes);
  
  timer.stop();
  updateElapsed();
  cancelButton->setEnabled(false);
  startButton->setEnabled(true);
}


void MainWidget::setSelector( Selector *s )
{
  setCapacity(0);  // to hide slice progress bar ...
  setFileProgress(100);  // to hide file progress bar

  selector = s;
  
  archiver = new Archiver(this);
  connect(cancelButton, SIGNAL(clicked()), archiver, SLOT(cancel()));
  
  connect(archiver, SIGNAL(logging(const QString &)), log, SLOT(append(const QString &)));
  connect(archiver, SIGNAL(warning(const QString &)), warnings, SLOT(append(const QString &)));
  
  connect(archiver, SIGNAL(targetCapacity(KIO::filesize_t)), this, SLOT(setCapacity(KIO::filesize_t)));
  
  connect(archiver, SIGNAL(totalFilesChanged(int)), totalFiles, SLOT(setNum(int)));
  connect(archiver, SIGNAL(totalBytesChanged(KIO::filesize_t)), this, SLOT(updateTotalBytes()));
  
  connect(archiver, SIGNAL(sliceProgress(int)), progressSlice, SLOT(setProgress(int)));
  connect(archiver, SIGNAL(newSlice(int)), sliceNum, SLOT(setNum(int)));

  connect(archiver, SIGNAL(fileProgress(int)), this, SLOT(setFileProgress(int)));
}


void MainWidget::getMediaSize()
{
  KURL url = KDirSelectDialog::selectDirectory("/", false, this);
  
  if ( url.isEmpty() ) return;  // cancelled
  
  targetDir->setText(url.pathOrURL());
  
  getDiskFree();
}


void MainWidget::updateElapsed()
{
  elapsedTime->setText(KGlobal::locale()->formatTime(QTime().addMSecs(elapsed.elapsed()), true, true));

}

void MainWidget::setTargetURL( QString url )
{
  targetDir->setText(url);
  getDiskFree();
}


void MainWidget::updateTotalBytes()
{
  // don't use KIO::convertSize() as this would not show good progress
  // after reaching 1 GB; always show MBs
  totalSize->setText(
    QString::number(archiver->getTotalBytes() / 1024.0 / 1024.0, 'f', 2));
}


void MainWidget::getDiskFree()
{
  KURL url = KURL::fromPathOrURL(targetDir->text());
  
  if ( url.isLocalFile() )
  {
    KIO::filesize_t capacityBytes, freeBytes;
    
    if ( Archiver::getDiskFree(url.path(), capacityBytes, freeBytes) )
    {
      setCapacity(capacityBytes);
      progressSlice->setProgress(static_cast<int>((capacityBytes - freeBytes) * 100 / capacityBytes));
    }
  }
  else
    setCapacity(0);
}


void MainWidget::setFileProgress( int percent )
{
  if ( percent == 100 )
  {
    fileProgressLabel->hide();
    fileProgress->hide();
  }
  else
  {
    fileProgressLabel->show();
    fileProgress->show();
    fileProgress->setProgress(percent);
  }
}


void MainWidget::setCapacity( KIO::filesize_t bytes )
{
  if ( bytes == 0 )
    capacity->setText(i18n("unlimited"));
  else
    capacity->setText(KIO::convertSize(bytes));
  
  sliceLabel->setShown(bytes);
  sliceNum->setShown(bytes);
  progressSlice->setShown(bytes);
}
