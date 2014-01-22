/* ------------------------------------------------------------------------- *
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Simo Piiroinen <simo.piiroinen@jollamobile.com>
 * License: BSD
 * ------------------------------------------------------------------------- */

#include <QCoreApplication>
#include <QTimer>
#include "../../lib/displayblanking.h"
#include "../common/debugf.h"

class MyApp : public QCoreApplication
{
  Q_OBJECT

  DisplayBlanking *displayBlanking;

public:
  MyApp(int &argc, char **argv) : QCoreApplication(argc, argv)
  {
    HERE
    displayBlanking = new DisplayBlanking(this);
    QObject::connect(displayBlanking, SIGNAL(preventBlankingChanged()),
                     this, SLOT(preventBlankingChanged()));

    QTimer::singleShot(5 * 1000, this, SLOT(startPeriod()));
  }

  virtual ~MyApp(void)
  {
    HERE
  }

public slots:

  void preventBlankingChanged(void)
  {
    bool val = displayBlanking->preventBlanking();
    debugf("prevent blanking -> %d\n", val );
    if( !val ) quit();
  }

  void startPeriod(void)
  {
    HERE
    displayBlanking->setPreventBlanking(true);
    QTimer::singleShot(5 * 1000, this, SLOT(endPeriod()));
  }

  void endPeriod(void)
  {
    HERE
    displayBlanking->setPreventBlanking(false);
  }
};

int main(int argc, char **argv)
{
  HERE
  MyApp app(argc, argv);
  return app.exec();
}

#include "displayblanking.moc"
