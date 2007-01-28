#ifndef _SELECTOR_H_
#define _SELECTOR_H_

/***************************************************************************
 *   (c) 2006, Martin Koller, m.koller@surfeu.at                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, version 2 of the License                *
 *                                                                         *
 ***************************************************************************/

// the selection widget lets the user select which files/dirs to back up

#include <qlistview.h>

class Selector : public QListView
{
  Q_OBJECT

  public:
    Selector(QWidget *parent = 0);

    void getBackupList(QStringList &includes, QStringList &excludes) const;
    void setBackupList(const QStringList &includes, const QStringList &excludes);

    virtual QSize minimumSizeHint() const;

  private:
    void fillTree(QListViewItem *parent, const QString &path, bool on);
    QString getPath(QListViewItem *item) const;
    void getBackupLists(QListViewItem *start, QStringList &includes, QStringList &excludes, bool add = true) const;

    QListViewItem *findItemByPath(const QString &path);
    QListViewItem *findItem(QListViewItem *start, const QString &toFind) const;

  private slots:
    void expandedSlot(QListViewItem *);

  private:
    QSize minSize;
};

#endif
