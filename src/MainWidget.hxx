#include <MainWidgetBase.h>
#include "kio/global.h"

class Selector;
class Archiver;

class MainWidget : public MainWidgetBase
{
  Q_OBJECT

  public:
    MainWidget(QWidget *parent);

    void setSelector(Selector * s);

  public slots:
    void setTargetURL(const QString &url);
    void startBackup();

  private:
    Selector *selector;
    Archiver *archiver;
    QTime elapsed;

  private slots:
    void getMediaSize();
    void updateElapsed();
    void updateTotalBytes();
    //void getDiskFree();
    void setFileProgress(int percent);
    void setCapacity(KIO::filesize_t bytes);
};
