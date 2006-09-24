#ifndef _MAIN_WINDOW_H_
#define _MAIN_WINDOW_H_

/***************************************************************************
 *   (c) 2006, Martin Koller, m.koller@surfeu.at                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, version 2 of the License                *
 *                                                                         *
 ***************************************************************************/

#include <kmainwindow.h>
class Selector;
class MainWidget;
class KSystemTray;
class KToggleAction;
class KAction;

class MainWindow : public KMainWindow
{
  Q_OBJECT

  public:
    MainWindow();
    void loadProfile(const QString &fileName, bool adaptTreeWidth = false);

  protected:
    virtual bool queryClose();

  private slots:
    void loadProfile();
    void saveProfile();
    void profileSettings();
    void newProfile();
    void loggingSlot(const QString &message);
    void changeSystrayTip();
    void inProgress(bool);
    void dockInSysTray();
    void maybeQuit();

  private:
    bool stopAllowed();

  private:
    Selector *selector;
    MainWidget *mainWidget;
    KSystemTray *sysTray;
    QString lastLog;
    KToggleAction *docked;
    KAction *startBackup;
    KAction *cancelBackup;
};

#endif
