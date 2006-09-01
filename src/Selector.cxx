/***************************************************************************
 *   (c) 2006, Martin Koller, m.koller@surfeu.at                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, version 2 of the License                *
 *                                                                         *
 ***************************************************************************/

#include <Selector.hxx>

#include <kio/global.h>
#include <kglobal.h>
#include <klocale.h>
#include <kiconloader.h>

#include <qdir.h>

//#include <iostream>
//using namespace std;
//--------------------------------------------------------------------------------

class ListItem : public QCheckListItem
{
  public:
    ListItem(QListView *parent, const QString &text, bool dir)
      : QCheckListItem(parent, text, QCheckListItem::CheckBox), isDir_(dir)
    {
    }

    ListItem(QListViewItem *parent, const QString &text, bool dir)
      : QCheckListItem(parent, text, QCheckListItem::CheckBox), isDir_(dir)
    {
    }

    virtual void stateChange(bool b)
    {
      QCheckListItem::stateChange(b);

      if ( ! isExpandable() ) return;

      recursActivate(isOn());
    }

    // set all children recursively below this to on
    void recursActivate(bool on)
    {
      setOn(on);

      for (QListViewItem *item = firstChild(); item; item = item->nextSibling())
        static_cast<ListItem*>(item)->recursActivate(on);
    }

    bool isDir() const { return isDir_; }

  private:
    bool isDir_;
};

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

Selector::Selector(QWidget *parent)
  : QListView(parent)
{
  addColumn(i18n("Name"));
  addColumn(i18n("Size"));
  addColumn(i18n("Last Modified"));

  setRootIsDecorated(true);
  setShowSortIndicator(true);

  fillTree(0, "/", false);

  adjustColumn(0);
  adjustColumn(1);
  adjustColumn(2);
  minSize = QSize(columnWidth(0) + columnWidth(1), -1);

  connect(this, SIGNAL(expanded(QListViewItem *)), this, SLOT(expandedSlot(QListViewItem*)));
}

//--------------------------------------------------------------------------------

QSize Selector::minimumSizeHint() const
{
  return minSize;
}

//--------------------------------------------------------------------------------

void Selector::fillTree(QListViewItem *parent, const QString &path, bool on)
{
  QDir dir(path, QString::null, QDir::Name | QDir::IgnoreCase, QDir::All | QDir::Hidden);
  const QFileInfoList *list = dir.entryInfoList();
  if ( !list ) return;

  ListItem *item;
  QFileInfoListIterator it(*list);
  QFileInfo *info;

  for (; (info = it.current()) != 0; ++it)
  {
    if ( (info->fileName() == ".") || (info->fileName() == "..") ) continue;

    if ( parent )
      item = new ListItem(parent, info->fileName(), info->isDir());
    else
      item = new ListItem(this, info->fileName(), info->isDir());

    item->setOn(on);

    item->setText(1, KIO::convertSize(info->size()));
    item->setText(2, KGlobal::locale()->formatDateTime(info->lastModified()));

    if ( item->isDir() )
    {
      QDir dir(info->absFilePath(), QString::null, QDir::Name | QDir::IgnoreCase, QDir::All | QDir::Hidden);
      if ( (dir.count() - 2) > 0)  // skip "." and ".."
        item->setExpandable(true);

      item->setPixmap(0, SmallIcon("folder"));
    }
    else
      item->setPixmap(0, SmallIcon("document"));
  }
}

//--------------------------------------------------------------------------------

QString Selector::getPath(QListViewItem *item) const
{
  if ( !item )
    return "";
  else
    return getPath(item->parent()) + "/" + item->text(0);
}

//--------------------------------------------------------------------------------

void Selector::expandedSlot(QListViewItem *item)
{
  if ( item->childCount() ) return;  // already done

  fillTree(item, getPath(item), static_cast<ListItem*>(item)->isOn());
}

//--------------------------------------------------------------------------------

void Selector::getBackupList(QStringList &includes, QStringList &excludes) const
{
  for (QListViewItem *item = firstChild(); item; item = item->nextSibling())
    getBackupLists(item, includes, excludes);

  /*
  cerr << "includes:" << includes.count() << endl;
  for (QStringList::const_iterator it = includes.begin(); (it != includes.end()); ++it)
    cerr << *it << endl;

  cerr << "excludes:" << excludes.count() << endl;
  for (QStringList::const_iterator it = excludes.begin(); (it != excludes.end()); ++it)
    cerr << *it << endl;

  cerr << endl;
  */
}

//--------------------------------------------------------------------------------

void Selector::getBackupLists(QListViewItem *start, QStringList &includes, QStringList &excludes, bool add) const
{
  if ( static_cast<ListItem*>(start)->isOn() )
  {
    if ( add )
      includes.append(getPath(start));  // include it

    if ( static_cast<ListItem*>(start)->isDir() )
    {
      // get excludes from this dir
      for (QListViewItem *item = start->firstChild(); item; item = item->nextSibling())
      {
        if ( !static_cast<ListItem*>(item)->isOn() )
          excludes.append(getPath(item));

        if ( static_cast<ListItem*>(item)->isDir() )
          getBackupLists(item, includes, excludes, false);
      }
    }
  }
  else
    if ( static_cast<ListItem*>(start)->isDir() )
    {
      for (QListViewItem *item = start->firstChild(); item; item = item->nextSibling())
        getBackupLists(item, includes, excludes);
    }
}

//--------------------------------------------------------------------------------

void Selector::setBackupList(QStringList &includes, QStringList &excludes)
{
  // clear all current settings
  for (QListViewItem *item = firstChild(); item; item = item->nextSibling())
    static_cast<ListItem*>(item)->recursActivate(false);

  for (QStringList::const_iterator it = includes.begin(); (it != includes.end()); ++it)
  {
    QListViewItem *item = findItemByPath(*it);
    if ( item )
      static_cast<ListItem*>(item)->recursActivate(true);
  }

  for (QStringList::const_iterator it = excludes.begin(); (it != excludes.end()); ++it)
  {
    QListViewItem *item = findItemByPath(*it);
    if ( item )
      static_cast<ListItem*>(item)->setOn(false);
  }
}

//--------------------------------------------------------------------------------

QListViewItem *Selector::findItemByPath(const QString &path)
{
  QStringList items = QStringList::split('/', path);
  QListViewItem *item = 0;

  for (QStringList::const_iterator it = items.begin(); it != items.end(); ++it)
  {
    item = findItem(item, *it);

    if ( !item )
      return 0;
    else
      if ( (it != items.fromLast()) &&
           static_cast<ListItem*>(item)->isDir() && (item->childCount() == 0) )
        expandedSlot(item);
  }

  return item;
}

//--------------------------------------------------------------------------------

QListViewItem *Selector::findItem(QListViewItem *start, const QString &toFind) const
{
  for (QListViewItem *item = start ? start->firstChild() : firstChild(); item; item = item->nextSibling())
    if ( item->text(0) == toFind )
      return item;

  return 0;
}

//--------------------------------------------------------------------------------
