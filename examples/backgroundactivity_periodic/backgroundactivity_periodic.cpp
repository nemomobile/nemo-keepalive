/* ------------------------------------------------------------------------- *
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Simo Piiroinen <simo.piiroinen@jollamobile.com>
 * License: BSD
 * ------------------------------------------------------------------------- */

#include "../../lib/backgroundactivity.h"
#include "../common/debugf.h"

#include <QCoreApplication>
#include <QTimer>

class TestActivity : public QObject
{
  Q_OBJECT

  BackgroundActivity *activity;
  QCoreApplication *application;
  int run_count;

public:
  TestActivity(QCoreApplication *app): QObject(app)
  {
    application = app;
    run_count = 2;
    activity = new BackgroundActivity(this);
    connect(activity, SIGNAL(running()), this, SLOT(startRun()));
    connect(activity, SIGNAL(stopped()), application, SLOT(quit()));

#if 0
    activity->setWakeupFrequency(BackgroundActivity::ThirtySeconds);
    activity->wait();
#else
    activity->wait(BackgroundActivity::ThirtySeconds);
#endif
  }

  virtual ~TestActivity(void)
  {
    delete activity;
  }

public slots:

  void startRun(void)
  {
    HERE
    QTimer::singleShot(10 * 1000, this, SLOT(finishRun()));
  }

  void finishRun(void)
  {
    HERE
    if( --run_count > 0 )
      activity->wait();
    else
      activity->stop();
  }
};

int main(int argc, char **argv)
{
  HERE
  QCoreApplication app(argc, argv);
  TestActivity act(&app);
  return app.exec();
}

#include "backgroundactivity_periodic.moc"
