//**************************************************************************
//   (c) 2006, 2007 Martin Koller, m.koller@surfeu.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#include <Selector.hxx>

#include <kio/global.h>
#include <kglobal.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kiconeffect.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <kde_file.h>

#include <qdir.h>

#include <iostream>
using namespace std;
//--------------------------------------------------------------------------------

class ListItem : public QCheckListItem
{
  public:
    ListItem(QListView *parent, const QString &text, bool dir)
      : QCheckListItem(parent, text, QCheckListItem::CheckBox), isDir_(dir), partly(false)
    {
    }

    ListItem(QListViewItem *parent, const QString &text, bool dir)
      : QCheckListItem(parent, text, QCheckListItem::CheckBox), isDir_(dir), partly(false)
    {
    }

    // check if all siblings have the same state as the parent or are partly marked
    // If not, then the parent must have the partly flag set, otherwise the parents
    // partly flag can be cleared
    void recursSiblingsUp()
    {
      if ( !parent() ) return;

      bool allSame = true, state = static_cast<ListItem*>(parent())->isOn();
      for (QListViewItem *item = parent()->firstChild(); item; item = item->nextSibling())
        if ( (static_cast<ListItem*>(item)->isOn() != state) || static_cast<ListItem*>(item)->partly )
        {
          allSame = false;
          break;
        }

      // only continue upwards if the parents partly status changes
      if ( static_cast<ListItem*>(parent())->partly != !allSame )
      {
        static_cast<ListItem*>(parent())->partly = !allSame;
        parent()->repaint();
        static_cast<ListItem*>(parent())->recursSiblingsUp();
      }
    }

    virtual void stateChange(bool b)
    {
      QCheckListItem::stateChange(b);

      recursActivate(isOn());
      recursSiblingsUp();
    }

    // set all children recursively below this to on
    void recursActivate(bool on)
    {
      partly = false;  // all children will get the same state
      setOn(on);

      for (QListViewItem *item = firstChild(); item; item = item->nextSibling())
        static_cast<ListItem*>(item)->recursActivate(on);
    }

    bool isDir() const { return isDir_; }

    virtual void paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int align)
    {
      QColorGroup colorGroup(cg);

      if ( partly )
        colorGroup.setColor(QColorGroup::Text, Qt::blue);

      QCheckListItem::paintCell(p, colorGroup, column, width, align);
    }

    virtual QString key(int column, bool ascending) const
    {
      switch ( column )
      {
        case 0:
        {
          // sort directories _always_ first
          if ( ascending )
            return (isDir_ ? "0" : "1") + text();
          else
            return (isDir_ ? "1" : "0") + text();
        }
        case 1: return sizeSortStr;
        case 2: return timeSortStr;
      }
      return text();
    }

    void setSize(KIO::filesize_t size)
    {
      sizeSortStr = KIO::number(size).rightJustify(20);
      setText(1, KIO::convertSize(size));
    }

    void setLastModified(const QDateTime &time)
    {
      timeSortStr = time.toString(Qt::ISODate); // sortable
      setText(2, KGlobal::locale()->formatDateTime(time));
    }

  private:
    bool isDir_;
    bool partly;  // is this an item which is not fully (but partly - some of the children) selected

    // store for fast, correct sorting
    QString sizeSortStr;
    QString timeSortStr;
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

  // for convenience, open the tree at the HOME directory
  char *home = ::getenv("HOME");
  if ( home )
  {
    QListViewItem *item = findItemByPath(QFile::decodeName(home));
    if ( item )
    {
      item->setOpen(true);
      ensureItemVisible(item);
    }
  }
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

    KDE_struct_stat status;
    memset(&status, 0, sizeof(status));

    // QFileInfo has no large file support (only files up to 2GB)
    KDE_stat(QFile::encodeName(info->absFilePath()), &status);
    item->setSize(status.st_size);
    item->setLastModified(info->lastModified());

    if ( item->isDir() )
    {
      QDir dir(info->absFilePath(), QString::null, QDir::Name | QDir::IgnoreCase, QDir::All | QDir::Hidden);
      if ( (dir.count() - 2) > 0)  // skip "." and ".."
        item->setExpandable(true);

      static QPixmap folderIcon;
      static QPixmap folderLinkIcon;

      if ( folderIcon.isNull() )  // only get the icons once
      {
        folderIcon = SmallIcon("folder");

        // create a "link" icon and make sure the "folder" icon has
        // the same size as the "link" icon as otherwise the overlay operation
        // would not be done
        QImage overlay(SmallIcon("link").convertToImage());
        QImage src(folderIcon.convertToImage().scale(overlay.size()));
        KIconEffect::overlay(src, overlay);
        folderLinkIcon = src;
      }

      item->setPixmap(0, info->isSymLink() ? folderLinkIcon : folderIcon);
    }
    else
    {
      static QPixmap documentIcon;
      static QPixmap documentLinkIcon;

      if ( documentIcon.isNull() )  // only get the icons once
      {
        documentIcon = SmallIcon("document");

        // create a "link" icon and make sure the "document" icon has
        // the same size as the "link" icon as otherwise the overlay operation
        // would not be done (kdeclassic has 18x18 but link has 16x16; crystalsvg is ok)
        QImage overlay(SmallIcon("link").convertToImage());
        QImage src(documentIcon.convertToImage().scale(overlay.size()));
        KIconEffect::overlay(src, overlay);
        documentLinkIcon = src;
      }

      item->setPixmap(0, info->isSymLink() ? documentLinkIcon : documentIcon);
    }
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

void Selector::setBackupList(const QStringList &includes, const QStringList &excludes)
{
  int sortCol = sortColumn();
  setSorting(-1);  // otherwise the performance is very bad as firstChild() always sorts

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

  setSorting(sortCol);
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
