//**************************************************************************
//   (c) 2006 - 2009 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#ifndef _SELECTOR_H_
#define _SELECTOR_H_

// the selection widget lets the user select which files/dirs to back up

#include <q3listview.h>

class Selector : public Q3ListView
{
  Q_OBJECT

  public:
    Selector(QWidget *parent = 0);

    void getBackupList(QStringList &includes, QStringList &excludes) const;
    void setBackupList(const QStringList &includes, const QStringList &excludes);

    virtual QSize minimumSizeHint() const;

  private:
    void fillTree(Q3ListViewItem *parent, const QString &path, bool on);
    QString getPath(Q3ListViewItem *item) const;
    void getBackupLists(Q3ListViewItem *start, QStringList &includes, QStringList &excludes, bool add = true) const;

    Q3ListViewItem *findItemByPath(const QString &path);
    Q3ListViewItem *findItem(Q3ListViewItem *start, const QString &toFind) const;

  private slots:
    void expandedSlot(Q3ListViewItem *);

  private:
    QSize minSize;
};

#endif
