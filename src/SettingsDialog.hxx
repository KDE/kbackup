//**************************************************************************
//   Copyright 2006 - 2017 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#ifndef _SETTINGS_DIALOG_H_
#define _SETTINGS_DIALOG_H_

#include <QDialog>
#include <ui_SettingsDialog.h>

class SettingsDialog : public QDialog
{
  Q_OBJECT

  public:
    explicit SettingsDialog(QWidget *parent);

    void setMaxMB(int mb);

    Ui::SettingsDialog ui;

  private Q_SLOTS:
    void slotSizeSelected(int idx);
};

#endif
