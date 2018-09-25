//**************************************************************************
//   Copyright 2006 - 2017 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#ifndef _SELECTOR_H_
#define _SELECTOR_H_

// the selection widget lets the user select which files/dirs to back up

#include <QTreeView>
#include <QStandardItemModel>

#include <KService>

class KActionCollection;
class ListItem;
class QMenu;

class Selector : public QTreeView
{
  Q_OBJECT

  public:
    explicit Selector(QWidget *parent, KActionCollection *actionCollection);

    void getBackupList(QStringList &includes, QStringList &excludes) const;
    void setBackupList(const QStringList &includes, const QStringList &excludes);
    void setShowHiddenFiles(bool show);

    QSize minimumSizeHint() const override;

  protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

  private:
    void fillTree(ListItem *parent, const QString &path, bool on);
    QString getPath(QStandardItem *item) const;
    void getBackupLists(QStandardItem *start, QStringList &includes, QStringList &excludes, bool add = true) const;

    QStandardItem *findItemByPath(const QString &path);
    QStandardItem *findItem(QStandardItem *start, const QString &toFind) const;

    ListItem *getSelectedItem() const;

  private Q_SLOTS:
    void expandedSlot(const QModelIndex &index);
    void populateOpenMenu();
    void doubleClickedSlot();
    void open();
    void openWith(QAction *action);
    void deleteFile();
    void properties();

  private:
    QSize minSize;
    QStandardItemModel *itemModel;
    QMenu *menu, *openWithSubMenu;
    QAction *deleteFileAction;
    QMap<QString, KService::Ptr> serviceForName;
    bool showHiddenFiles;
};

#endif
