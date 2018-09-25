//**************************************************************************
//   Copyright 2006 - 2017 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#ifndef _MAIN_WIDGET_H_
#define _MAIN_WIDGET_H_

#include <QWidget>
#include <QTime>
#include "kio/global.h"
#include <ui_MainWidgetBase.h>

class Selector;

class MainWidget : public QWidget
{
  Q_OBJECT

  public:
    explicit MainWidget(QWidget *parent);

    void setSelector(Selector * s);
    KLineEdit *getTargetLineEdit() const { return ui.targetDir; }

  public Q_SLOTS:
    void setTargetURL(const QString &url);
    void startBackup();
    void setIsIncrementalBackup(bool incremental);

  private:
    Selector *selector;
    Ui::MainWidgetBase ui;

  private Q_SLOTS:
    void getMediaSize();
    void updateElapsed(const QTime &);
    void updateTotalBytes();
    void setFileProgress(int percent);
    void setCapacity(KIO::filesize_t bytes);
};

#endif
