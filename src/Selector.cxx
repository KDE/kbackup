//**************************************************************************
//   Copyright 2006 - 2018 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#include <Selector.hxx>

#include <kio_version.h>
#include <kio/global.h>
#include <kiconloader.h>
#include <kiconeffect.h>
#include <KLocalizedString>
#include <kpropertiesdialog.h>
#include <KFileItem>
#include <KMimeTypeTrader>
#include <KRun>
#include <KActionCollection>
#include <KMessageBox>

#include <QDir>
#include <QPixmap>
#include <QDateTime>
#include <QCollator>
#include <QHeaderView>
#include <QMenu>
#include <QPointer>

#include <iostream>
using namespace std;

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

class Model : public QStandardItemModel
{
  public:
    Model(Selector *parent) : QStandardItemModel(parent), tree(parent)
    {
      const char *lc_collate = ::getenv("LC_COLLATE");
      if ( lc_collate )
        collator.setLocale(QLocale(QLatin1String(lc_collate)));

      collator.setNumericMode(true);
    }

    virtual bool hasChildren(const QModelIndex &index = QModelIndex()) const
    {
      QStandardItem *item = itemFromIndex(index);

      if ( !item )
        return true;  // invisible root

      return !(item->flags() & Qt::ItemNeverHasChildren);
    }

    Selector *tree;
    QCollator collator;
};

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

class ListItem : public QStandardItem
{
  public:
    ListItem(QStandardItem *parent, const QString &text, bool dir)
      : isDir_(dir), partly(false)
    {
      parent->appendRow(this);
      parent->setChild(row(), 1, new QStandardItem);
      parent->setChild(row(), 2, new QStandardItem);

      setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);

      if ( !isDir_ )
        setFlags(flags() | Qt::ItemNeverHasChildren);

      setCheckState(Qt::Unchecked);
      setText(0, text, key(text));
    }

    virtual int type() const { return QStandardItem::UserType; }

    bool isOn() const { return checkState() == Qt::Checked; }
    void setOn(bool on) { setCheckState(on ? Qt::Checked : Qt::Unchecked); }

    void setText(int column, const QString &txt, const QVariant &sortKey)
    {
      QStandardItem *item;

      if ( parent() )
        item = parent()->child(row(), column);
      else
        item = model()->item(row(), column);

      item->setText(txt);
      item->setData(sortKey, Qt::UserRole);
    }

    virtual void setData(const QVariant &value, int role = Qt::UserRole + 1)
    {
      if ( role == Qt::CheckStateRole )
      {
        if ( value.toInt() != checkState() )
        {
          QStandardItem::setData(value, role);
          stateChanged();
          return;
        }
      }

      QStandardItem::setData(value, role);
    }

    // check if all siblings have the same state as the parent or are partly marked
    // If not, then the parent must have the partly flag set, otherwise the parents
    // partly flag can be cleared
    void recursSiblingsUp()
    {
      if ( !parent() ) return;

      bool allSame = true, state = static_cast<ListItem*>(parent())->isOn();

      for (int i = 0; i < parent()->rowCount(); i++)
      {
        QStandardItem *item = parent()->child(i);

        if ( (static_cast<ListItem*>(item)->isOn() != state) || static_cast<ListItem*>(item)->partly )
        {
          allSame = false;
          break;
        }
      }

      // only continue upwards if the parents partly status changes
      if ( static_cast<ListItem*>(parent())->partly != !allSame )
      {
        static_cast<ListItem*>(parent())->partly = !allSame;

        if ( !allSame )
          static_cast<ListItem*>(parent())->setForeground(Qt::blue);
        else
        {
          QWidget *w = static_cast<Model *>(model())->tree;
          static_cast<ListItem*>(parent())->setForeground(w->palette().color(w->foregroundRole()));
        }

        static_cast<ListItem*>(parent())->recursSiblingsUp();
      }
    }

    void stateChanged()
    {
      recursActivate(isOn());
      recursSiblingsUp();
    }

    // set all children recursively below this to on
    void recursActivate(bool on)
    {
      partly = false;  // all children will get the same state

      QWidget *w = static_cast<Model *>(model())->tree;
      setForeground(w->palette().color(w->foregroundRole()));

      setOn(on);

      for (int i = 0; i < rowCount(); i++)
        static_cast<ListItem*>(child(i))->recursActivate(on);
    }

    bool isDir() const { return isDir_; }

    int key(const QString &text) const
    {
      bool hidden = text[0] == QChar('.');

      // sort directories _always_ first, and hidden before shown
      if ( isDir_ )
        return hidden ? 0 : 1;
      else  // file
        return hidden ? 2 : 3;
    }

    virtual bool operator<(const QStandardItem &other_) const
    {
      QTreeView *w = static_cast<Model *>(model())->tree;
      Qt::SortOrder order = w->header()->sortIndicatorOrder();

      const ListItem &other = static_cast<const ListItem &>(other_);

      int myKey = data(Qt::UserRole).toInt();
      int otherKey = other.data(Qt::UserRole).toInt();

      if ( myKey != otherKey )
        return (order == Qt::AscendingOrder) ? (myKey < otherKey) : (myKey > otherKey);
      else
      {
        // don't use localeAwareCompare. QLocale does not use LC_COLLATE QTBUG-29397
        return static_cast<Model *>(model())->collator.compare(text(), other.text()) < 0;
      }
    }

    void setSize(KIO::filesize_t size)
    {
      setText(1, KIO::convertSize(size), size);
    }

    void setLastModified(const QDateTime &time)
    {
      setText(2, QLocale().toString(time, QLocale::ShortFormat), time);
    }

    void setShowHiddenFiles(bool show)
    {
      QTreeView *w = static_cast<Model *>(model())->tree;

      QStandardItem *parentItem = parent() ? parent() : model()->invisibleRootItem();

      w->setRowHidden(row(), parentItem->index(), show ? false : text()[0] == QLatin1Char('.'));

      for (int i = 0; i < rowCount(); i++)
        static_cast<ListItem*>(child(i))->setShowHiddenFiles(show);
    }

  private:
    bool isDir_;
    bool partly;  // is this an item which is not fully (but partly - some of the children) selected

};

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

Selector::Selector(QWidget *parent, KActionCollection *actionCollection)
  : QTreeView(parent), showHiddenFiles(true)
{
  itemModel = new Model(this);

  setModel(itemModel);

  itemModel->setSortRole(Qt::UserRole);
  itemModel->setHorizontalHeaderLabels(QStringList() << i18n("Name") << i18n("Size") << i18n("Last Modified"));

  setRootIsDecorated(true);

  // start with / as root node
  ListItem *item = new ListItem(itemModel->invisibleRootItem(), "/", true);
  QFileInfo info("/");
  item->setSize(info.size());
  item->setLastModified(info.lastModified());
  item->setIcon(SmallIcon("folder"));
  setExpanded(item->index(), true);

  fillTree(item, "/", false);

  connect(this, &Selector::expanded, this, &Selector::expandedSlot);

  // for convenience, open the tree at the HOME directory
  const char *home = ::getenv("HOME");
  if ( home )
  {
    QStandardItem *item = findItemByPath(QFile::decodeName(home));
    if ( item )
    {
      setExpanded(item->index(), true);
      scrollTo(item->index());
    }
  }

  minSize = QSize(columnWidth(0) + columnWidth(1), -1);
  resizeColumnToContents(0);
  resizeColumnToContents(1);
  resizeColumnToContents(2);

  sortByColumn(0, Qt::AscendingOrder);

  // context menu
  menu = new QMenu(this);
  QAction *action;

  action = KStandardAction::open(this, SLOT(open()), actionCollection);
  menu->addAction(action);

  connect(this, &Selector::doubleClicked, this, &Selector::doubleClickedSlot);

  openWithSubMenu = new QMenu(i18n("Open With"), this);
  menu->addMenu(openWithSubMenu);
  connect(openWithSubMenu, &QMenu::aboutToShow, this, &Selector::populateOpenMenu);
  connect(openWithSubMenu, &QMenu::triggered, this, &Selector::openWith);

  // just since KF 5.25
  //deleteFileAction = KStandardAction::deleteFile(this, SLOT(deleteFile()), actionCollection);
  deleteFileAction = actionCollection->addAction("deleteFile", this, SLOT(deleteFile()));
  deleteFileAction->setText(i18n("Delete File"));
  deleteFileAction->setIcon(QIcon::fromTheme("edit-delete"));
  deleteFileAction->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Delete));
  menu->addAction(deleteFileAction);

  action = actionCollection->addAction("properties", this, SLOT(properties()));
  action->setText(i18n("Properties..."));
  menu->addAction(action);
}

//--------------------------------------------------------------------------------

QSize Selector::minimumSizeHint() const
{
  return minSize;
}

//--------------------------------------------------------------------------------

void Selector::fillTree(ListItem *parent, const QString &path, bool on)
{
  setSortingEnabled(false);

  const QDir::Filters filter = QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot;

  QDir dir(path, QString(), QDir::NoSort, filter);
  const QFileInfoList list = dir.entryInfoList();

  ListItem *item;

  for (int i = 0; i < list.count(); i++)
  {
    if ( parent )
      item = new ListItem(parent, list[i].fileName(), list[i].isDir());
    else
      item = new ListItem(itemModel->invisibleRootItem(), list[i].fileName(), list[i].isDir());

    item->setOn(on);
    item->setSize(list[i].size());
    item->setLastModified(list[i].lastModified());
    item->setShowHiddenFiles(showHiddenFiles);

    if ( item->isDir() )
    {
      QDir dir(list[i].absoluteFilePath(), QString(), QDir::NoSort, filter);

      // symlinked dirs can not be expanded as they are stored as single files in the archive
      if ( (dir.count() > 0) && !list[i].isSymLink() )
        ; // can have children
      else
        item->setFlags(item->flags() | Qt::ItemNeverHasChildren);

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

      item->setIcon(list[i].isSymLink() ?
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

      item->setIcon(list[i].isSymLink() ?
                           (list[i].isHidden() ? documentLinkIconHidden : documentLinkIcon)
                         : (list[i].isHidden() ? documentIconHidden : documentIcon));
    }
  }
  setSortingEnabled(true);
}

//--------------------------------------------------------------------------------

QString Selector::getPath(QStandardItem *item) const
{
  if ( !item )
    return QString();
  else if ( !item->parent() )
    return item->text();  // root
  else if ( item->parent() == itemModel->item(0) )
    return "/" + item->text();
  else
    return getPath(item->parent()) + "/" + item->text();
}

//--------------------------------------------------------------------------------

void Selector::expandedSlot(const QModelIndex &index)
{
  QStandardItem *item = itemModel->itemFromIndex(index);

  if ( item->rowCount() ) return;  // already done

  fillTree(static_cast<ListItem *>(item), getPath(item), static_cast<ListItem *>(item)->isOn());
}

//--------------------------------------------------------------------------------

void Selector::getBackupList(QStringList &includes, QStringList &excludes) const
{
  for (int i = 0; i < itemModel->rowCount(); i++)
    getBackupLists(itemModel->item(i, 0), includes, excludes);

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

void Selector::getBackupLists(QStandardItem *start, QStringList &includes, QStringList &excludes, bool add) const
{
  if ( static_cast<ListItem*>(start)->isOn() )
  {
    if ( add )
      includes.append(getPath(start));  // include it

    if ( static_cast<ListItem*>(start)->isDir() )
    {
      // get excludes from this dir
      for (int i = 0; i < start->rowCount(); i++)
      {
        QStandardItem *item = start->child(i);

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
      for (int i = 0; i < start->rowCount(); i++)
        getBackupLists(start->child(i), includes, excludes);
    }
}

//--------------------------------------------------------------------------------

void Selector::setBackupList(const QStringList &includes, const QStringList &excludes)
{
  // clear all current settings
  for (int i = 0; i < itemModel->rowCount(); i++)
    static_cast<ListItem*>(itemModel->item(i, 0))->recursActivate(false);

  for (QStringList::const_iterator it = includes.begin(); (it != includes.end()); ++it)
  {
    QStandardItem *item = findItemByPath(*it);
    if ( item )
      static_cast<ListItem*>(item)->recursActivate(true);
  }

  for (QStringList::const_iterator it = excludes.begin(); (it != excludes.end()); ++it)
  {
    QStandardItem *item = findItemByPath(*it);
    if ( item )
      static_cast<ListItem*>(item)->setOn(false);
  }
}

//--------------------------------------------------------------------------------

QStandardItem *Selector::findItemByPath(const QString &path)
{
  QStringList items = path.split('/', QString::SkipEmptyParts);
  QStandardItem *item = itemModel->invisibleRootItem()->child(0);

  for (int i = 0; i < items.count(); i++)
  {
    item = findItem(item, items[i]);

    if ( !item )
      return nullptr;
    else
    {
      if ( (i != (items.count() - 1)) &&
           static_cast<ListItem*>(item)->isDir() && (item->rowCount() == 0) )
        expandedSlot(item->index());
    }
  }

  return item;
}

//--------------------------------------------------------------------------------

QStandardItem *Selector::findItem(QStandardItem *start, const QString &toFind) const
{
  for (int i = 0; i < (start ? start->rowCount() : itemModel->rowCount()); i++)
  {
    QStandardItem *item = start ? start->child(i) : itemModel->item(i, 0);

    if ( item->text() == toFind )
      return item;
  }

  return nullptr;
}

//--------------------------------------------------------------------------------

ListItem *Selector::getSelectedItem() const
{
  QModelIndex index = selectionModel()->currentIndex();
  if ( !index.isValid() )
    return nullptr;

  QStandardItem *item = itemModel->itemFromIndex(itemModel->index(index.row(), 0, index.parent()));

  if ( !item || (item->type() != QStandardItem::UserType) )  // just be safe
    return nullptr;

  return static_cast<ListItem *>(item);
}

//--------------------------------------------------------------------------------

void Selector::contextMenuEvent(QContextMenuEvent *)
{
  ListItem *item = getSelectedItem();

  if ( !item )
    return;

  bool canDelete = true;

  if ( item->isDir() )
  {
    // only if it's empty
    canDelete = QDir(getPath(item), QString(), QDir::NoSort,
                     QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot).count() == 0;
  }

  deleteFileAction->setEnabled(canDelete);

  menu->exec(QCursor::pos());
}

//--------------------------------------------------------------------------------

void Selector::deleteFile()
{
  ListItem *item = getSelectedItem();

  if ( !item )
    return;

  QUrl sourceUrl = QUrl::fromLocalFile(getPath(item));

  if ( KMessageBox::questionYesNo(this,
          i18n("Do you really want to delete '%1'?", sourceUrl.path()),
          i18n("Delete"),
          KStandardGuiItem::yes(), KStandardGuiItem::no(),
          "dontAskAgainDelete") == KMessageBox::Yes )
  {
    QStandardItem *parent = nullptr;

    if ( item->isDir() )
    {
      QDir dir(sourceUrl.path());

      if ( !dir.removeRecursively() )
        KMessageBox::error(this, i18n("Could not delete directory '%1'.", sourceUrl.path()));
      else
        parent = item->parent();
    }
    else
    {
      QFile theFile(sourceUrl.path());

      if ( !theFile.remove(sourceUrl.path()) )
        KMessageBox::error(this, i18n("Could not delete file '%1'.\nReason: %2", sourceUrl.path(), theFile.errorString()));
      else
        parent = item->parent();
    }

    if ( parent )
    {
      parent->removeRow(item->row());

      if ( parent->type() == QStandardItem::UserType )
        static_cast<ListItem *>(parent)->stateChanged();  // make sure removed item is taken into account
    }
  }
}

//--------------------------------------------------------------------------------

void Selector::properties()
{
  ListItem *item = getSelectedItem();

  if ( !item )
    return;

  QUrl sourceUrl = QUrl::fromLocalFile(getPath(item));

  QPointer<KPropertiesDialog> dialog = new KPropertiesDialog(sourceUrl, this);
  connect(dialog.data(), &KPropertiesDialog::applied, this,
          [item, dialog]()
          {
            // make sure a renamed file is shown with the new name in the tree
            item->setText(0, dialog->item().name(), item->key(dialog->item().name()));
          });

  dialog->exec();
  delete dialog;
}

//--------------------------------------------------------------------------------

void Selector::populateOpenMenu()
{
  ListItem *item = getSelectedItem();

  if ( !item )
    return;

  QUrl sourceUrl = QUrl::fromLocalFile(getPath(item));

  qDeleteAll(openWithSubMenu->actions());
  serviceForName.clear();

  KFileItem fileItem(sourceUrl);
  QString mimeType(fileItem.determineMimeType().name());

  KService::List services = KMimeTypeTrader::self()->query(mimeType);

  foreach (const KService::Ptr &service, services)
  {
    QString text = service->name().replace('&', "&&");
    QAction* action = openWithSubMenu->addAction(text);
    action->setIcon(QIcon::fromTheme(service->icon()));
    action->setData(service->name());

    serviceForName[service->name()] = service;
  }

  openWithSubMenu->addSeparator();
  openWithSubMenu->addAction(i18n("Other Application..."));

  QAction* action = openWithSubMenu->addAction(i18n("File Manager"));
  action->setIcon(QIcon::fromTheme("folder"));
  action->setData("-");
}

//--------------------------------------------------------------------------------

void Selector::doubleClickedSlot()
{
  ListItem *item = getSelectedItem();

  if ( !item || item->isDir() )
    return;

  open();
}

//--------------------------------------------------------------------------------

void Selector::open()
{
  ListItem *item = getSelectedItem();

  if ( !item )
    return;

  QUrl sourceUrl = QUrl::fromLocalFile(getPath(item));

  KRun *run = new KRun(sourceUrl, window());  // auto-deletes itself
  run->setRunExecutables(false);
}

//--------------------------------------------------------------------------------

void Selector::openWith(QAction *action)
{
  ListItem *item = getSelectedItem();

  if ( !item )
    return;

  QUrl sourceUrl = QUrl::fromLocalFile(getPath(item));

  QString name = action->data().toString();

  if ( name.isEmpty() ) // Other Application...
  {
    KRun::displayOpenWithDialog(QList<QUrl>() << sourceUrl, this);
    return;
  }

  if ( name == "-" )  // File Manager
  {
#if (KIO_VERSION >= QT_VERSION_CHECK(5, 31, 0))
    KRun::runUrl(sourceUrl.adjusted(QUrl::RemoveFilename), "inode/directory", this, KRun::RunFlags());
#else
    KRun::runUrl(sourceUrl.adjusted(QUrl::RemoveFilename), "inode/directory", this);
#endif

    return;
  }

  KService::Ptr service = serviceForName[name];
#if (KIO_VERSION >= QT_VERSION_CHECK(5, 24, 0))
  KRun::runApplication(*service, QList<QUrl>() << sourceUrl, this);
#else
  KRun::run(*service, QList<QUrl>() << sourceUrl, this);
#endif
}

//--------------------------------------------------------------------------------

void Selector::setShowHiddenFiles(bool show)
{
  showHiddenFiles = show;

  for (int i = 0; i < itemModel->invisibleRootItem()->rowCount(); i++)
    static_cast<ListItem *>(itemModel->item(i, 0))->setShowHiddenFiles(show);
}

//--------------------------------------------------------------------------------
