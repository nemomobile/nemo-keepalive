/* ------------------------------------------------------------------------- *
 * Copyright (C) 2014 Jolla Ltd.
 * Contact: Simo Piiroinen <simo.piiroinen@jollamobile.com>
 * License: BSD
 * ------------------------------------------------------------------------- */

/* ========================================================================= *
 *
 * This application demonstrates background activity that starts now, and
 * the device should not suspend until it is finished
 *
 * ========================================================================= */

#include "../../lib/backgroundactivity.h"
#include "../common/debugf.h"

#include <QCoreApplication>
#include <QTimer>

class TestActivity : public QObject
{
  Q_OBJECT

  BackgroundActivity *activity;
  QCoreApplication *application;

public:
  TestActivity(QCoreApplication *app): QObject(app)
  {
    application = app;
    activity = new BackgroundActivity(this);
    connect(activity, SIGNAL(running()), this, SLOT(startRun()));
    connect(activity, SIGNAL(stopped()), application, SLOT(quit()));
    activity->setWakeupFrequency(BackgroundActivity::ThirtySeconds);
    activity->run();
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

#include "backgroundactivity_linger.moc"
