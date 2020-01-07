//***************************************************************************
//   Copyright 2006 - 2017 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//***************************************************************************

#ifndef _MAIN_WINDOW_H_
#define _MAIN_WINDOW_H_

#include <kxmlguiwindow.h>

class Selector;
class MainWidget;
class KStatusNotifierItem;
class KToggleAction;
class QAction;
class KRecentFilesAction;
class QUrl;
class QSplitter;

class MainWindow : public KXmlGuiWindow
{
  Q_OBJECT

  public:
    MainWindow();
    void loadProfile(const QString &fileName, bool adaptTreeWidth = false);

    // start backup and quit application after it's finished
    void runBackup();

  protected:
    bool queryClose() override;

  private Q_SLOTS:
    void loadProfile();
    void saveProfileAs();
    void saveProfile(QString fileName = QString());
    void profileSettings();
    void newProfile();
    void loggingSlot(const QString &message);
    void changeSystrayTip();
    void inProgress(bool);
    void dockInSysTray(bool checked);
    void showHiddenFiles(bool checked);
    void maybeQuit();
    void recentProfileSelected(const QUrl &url);
    void enableAllMessages();

  private:
    bool stopAllowed();
    void setLoadedProfile(const QString &name);

  private:
    QSplitter *splitter = nullptr;
    Selector *selector = nullptr;
    MainWidget *mainWidget = nullptr;
    KStatusNotifierItem *sysTray = nullptr;
    QString lastLog;
    QAction *startBackupAction = nullptr;
    QAction *cancelBackupAction = nullptr;
    QAction *quitAction = nullptr;
    KRecentFilesAction *recentFiles = nullptr;
    bool autorun = false;
    QString loadedProfile;
};

#endif
