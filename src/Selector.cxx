//**************************************************************************
//   (c) 2006 - 2009 Martin Koller, kollix@aon.at
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

#include <QDir>
#include <QPixmap>
#include <QDateTime>

#include <iostream>
using namespace std;
//--------------------------------------------------------------------------------

class ListItem : public Q3CheckListItem
{
  public:
    ListItem(Q3ListView *parent, const QString &text, bool dir)
      : Q3CheckListItem(parent, text, Q3CheckListItem::CheckBox), isDir_(dir), partly(false)
    {
    }

    ListItem(Q3ListViewItem *parent, const QString &text, bool dir)
      : Q3CheckListItem(parent, text, Q3CheckListItem::CheckBox), isDir_(dir), partly(false)
    {
    }

    // check if all siblings have the same state as the parent or are partly marked
    // If not, then the parent must have the partly flag set, otherwise the parents
    // partly flag can be cleared
    void recursSiblingsUp()
    {
      if ( !parent() ) return;

      bool allSame = true, state = static_cast<ListItem*>(parent())->isOn();
      for (Q3ListViewItem *item = parent()->firstChild(); item; item = item->nextSibling())
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
      Q3CheckListItem::stateChange(b);

      recursActivate(isOn());
      recursSiblingsUp();
    }

    // set all children recursively below this to on
    void recursActivate(bool on)
    {
      partly = false;  // all children will get the same state
      setOn(on);

      for (Q3ListViewItem *item = firstChild(); item; item = item->nextSibling())
        static_cast<ListItem*>(item)->recursActivate(on);
    }

    bool isDir() const { return isDir_; }

    virtual void paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int align)
    {
      QColorGroup colorGroup(cg);

      if ( partly )
        colorGroup.setColor(QColorGroup::Text, Qt::blue);

      Q3CheckListItem::paintCell(p, colorGroup, column, width, align);
    }

    virtual QString key(int column, bool ascending) const
    {
      switch ( column )
      {
        case 0:
        {
          bool hidden = text()[0] == QChar('.');

          // sort directories _always_ first, and hidden before shown
          if ( ascending )
          {
            if ( isDir_ )
              return (hidden ? "0" : "1") + text();
            else  // file
              return (hidden ? "2" : "3") + text();
          }
          else
          {
            if ( isDir_ )
              return (hidden ? "3" : "2") + text();
            else
              return (hidden ? "1" : "0") + text();
          }
        }
        case 1: return sizeSortStr;
        case 2: return timeSortStr;
      }
      return text();
    }

    void setSize(KIO::filesize_t size)
    {
      sizeSortStr = KIO::number(size).rightJustified(15, QLatin1Char('0'));
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
  : Q3ListView(parent)
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

  connect(this, SIGNAL(expanded(Q3ListViewItem *)), this, SLOT(expandedSlot(Q3ListViewItem*)));

  // for convenience, open the tree at the HOME directory
  const char *home = ::getenv("HOME");
  if ( home )
  {
    Q3ListViewItem *item = findItemByPath(QFile::decodeName(home));
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

void Selector::fillTree(Q3ListViewItem *parent, const QString &path, bool on)
{
  QDir dir(path, QString::null, QDir::Name | QDir::IgnoreCase, QDir::All | QDir::Hidden);
  const QFileInfoList list = dir.entryInfoList();

  ListItem *item;

  for (int i = 0; i < list.count(); i++)
  {
    if ( (list[i].fileName() == ".") || (list[i].fileName() == "..") ) continue;

    if ( parent )
      item = new ListItem(parent, list[i].fileName(), list[i].isDir());
    else
      item = new ListItem(this, list[i].fileName(), list[i].isDir());

    item->setOn(on);
    item->setSize(list[i].size());
    item->setLastModified(list[i].lastModified());

    if ( item->isDir() )
    {
      QDir dir(list[i].absoluteFilePath(), QString::null, QDir::Name | QDir::IgnoreCase, QDir::All | QDir::Hidden);

      // symlinked dirs can not be expanded as they are stored as single files in the archive
      if ( ((dir.count() - 2) > 0) && !list[i].isSymLink() ) // skip "." and ".."
        item->setExpandable(true);

      static QPixmap folderIcon;
      static QPixmap folderLinkIcon;
      static QPixmap folderIconHidden;
      static QPixmap folderLinkIconHidden;

      if ( folderIcon.isNull() )  // only get the icons once
      {
        KIconEffect effect;

        folderIcon = SmallIcon("folder");
        folderIconHidden = effect.apply(folderIcon, KIconEffect::DeSaturate, 0, QColor(), true);

        folderLinkIcon = SmallIcon("folder", 0, KIconLoader::DefaultState,
                                   QStringList("emblem-symbolic-link"));

        folderLinkIconHidden = effect.apply(folderLinkIcon, KIconEffect::DeSaturate, 0, QColor(), true);
      }

      item->setPixmap(0, list[i].isSymLink() ?
                           (list[i].isHidden() ? folderLinkIconHidden : folderLinkIcon)
                         : (list[i].isHidden() ? folderIconHidden : folderIcon));
    }
    else
    {
      static QPixmap documentIcon;
      static QPixmap documentLinkIcon;
      static QPixmap documentIconHidden;
      static QPixmap documentLinkIconHidden;

      if ( documentIcon.isNull() )  // only get the icons once
      {
        KIconEffect effect;

        documentIcon = SmallIcon("text-x-generic");
        documentIconHidden = effect.apply(documentIcon, KIconEffect::DeSaturate, 0, QColor(), true);

        documentLinkIcon = SmallIcon("text-x-generic", 0, KIconLoader::DefaultState,
                                     QStringList("emblem-symbolic-link"));

        documentLinkIconHidden = effect.apply(documentLinkIcon, KIconEffect::DeSaturate, 0, QColor(), true);
      }

      item->setPixmap(0, list[i].isSymLink() ?
                           (list[i].isHidden() ? documentLinkIconHidden : documentLinkIcon)
                         : (list[i].isHidden() ? documentIconHidden : documentIcon));
    }
  }
}

//--------------------------------------------------------------------------------

QString Selector::getPath(Q3ListViewItem *item) const
{
  if ( !item )
    return "";
  else
    return getPath(item->parent()) + "/" + item->text(0);
}

//--------------------------------------------------------------------------------

void Selector::expandedSlot(Q3ListViewItem *item)
{
  if ( item->childCount() ) return;  // already done

  fillTree(item, getPath(item), static_cast<ListItem*>(item)->isOn());
}

//--------------------------------------------------------------------------------

void Selector::getBackupList(QStringList &includes, QStringList &excludes) const
{
  for (Q3ListViewItem *item = firstChild(); item; item = item->nextSibling())
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

void Selector::getBackupLists(Q3ListViewItem *start, QStringList &includes, QStringList &excludes, bool add) const
{
  if ( static_cast<ListItem*>(start)->isOn() )
  {
    if ( add )
      includes.append(getPath(start));  // include it

    if ( static_cast<ListItem*>(start)->isDir() )
    {
      // get excludes from this dir
      for (Q3ListViewItem *item = start->firstChild(); item; item = item->nextSibling())
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
      for (Q3ListViewItem *item = start->firstChild(); item; item = item->nextSibling())
        getBackupLists(item, includes, excludes);
    }
}

//--------------------------------------------------------------------------------

void Selector::setBackupList(const QStringList &includes, const QStringList &excludes)
{
  int sortCol = sortColumn();
  setSorting(-1);  // otherwise the performance is very bad as firstChild() always sorts

  // clear all current settings
  for (Q3ListViewItem *item = firstChild(); item; item = item->nextSibling())
    static_cast<ListItem*>(item)->recursActivate(false);

  for (QStringList::const_iterator it = includes.begin(); (it != includes.end()); ++it)
  {
    Q3ListViewItem *item = findItemByPath(*it);
    if ( item )
      static_cast<ListItem*>(item)->recursActivate(true);
  }

  for (QStringList::const_iterator it = excludes.begin(); (it != excludes.end()); ++it)
  {
    Q3ListViewItem *item = findItemByPath(*it);
    if ( item )
      static_cast<ListItem*>(item)->setOn(false);
  }

  setSorting(sortCol);
}

//--------------------------------------------------------------------------------

Q3ListViewItem *Selector::findItemByPath(const QString &path)
{
  QStringList items = path.split('/', QString::SkipEmptyParts);
  Q3ListViewItem *item = 0;

  for (int i = 0; i < items.count(); i++)
  {
    item = findItem(item, items[i]);

    if ( !item )
      return 0;
    else
    {
      if ( (i != (items.count() - 1)) &&
           static_cast<ListItem*>(item)->isDir() && (item->childCount() == 0) )
        expandedSlot(item);
    }
  }

  return item;
}

//--------------------------------------------------------------------------------

Q3ListViewItem *Selector::findItem(Q3ListViewItem *start, const QString &toFind) const
{
  for (Q3ListViewItem *item = start ? start->firstChild() : firstChild(); item; item = item->nextSibling())
    if ( item->text(0) == toFind )
      return item;

  return 0;
}

//--------------------------------------------------------------------------------
