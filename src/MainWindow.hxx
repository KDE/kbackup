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

class MainWindow : public KMainWindow
{
  Q_OBJECT

  public:
    MainWindow();
    void loadProfile(const QString &fileName, bool adaptTreeWidth = false);

  private slots:
    void loadProfile();
    void saveProfile();

  private:
    Selector *selector;
    MainWidget *mainWidget;
};

#endif
