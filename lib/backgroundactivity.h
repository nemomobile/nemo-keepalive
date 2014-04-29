/****************************************************************************************
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: Simo Piiroinen <simo.piiroinen@jollamobile.com>
** All rights reserved.
**
** This file is part of nemo keepalive package.
**
** You may use this file under the terms of the GNU Lesser General
** Public License version 2.1 as published by the Free Software Foundation
** and appearing in the file license.lgpl included in the packaging
** of this file.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file license.lgpl included in the packaging
** of this file.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Lesser General Public License for more details.
**
****************************************************************************************/

#ifndef BACKGROUNDACTIVITY_H_
# define BACKGROUNDACTIVITY_H_

# include <QObject>
# include <QEvent>

class BackgroundActivityPrivate;

class BackgroundActivity: public QObject
{
  Q_OBJECT
  Q_ENUMS(State Frequency)

public:

  enum State
  {
    Stopped,
    Waiting,
    Running
  };

  enum Frequency
  {                                       // ORIGIN:
    Range             =            0, // Nemomobile
    ThirtySeconds     =           30, // Meego
    TwoAndHalfMinutes =  30 + 2 * 60, // Meego
    FiveMinutes       =       5 * 60, // Meego
    TenMinutes        =      10 * 60, // Meego
    FifteenMinutes    =      15 * 60, // Android
    ThirtyMinutes     =      30 * 60, // Meego & Android
    OneHour           =  1 * 60 * 60, // Meego & Android
    TwoHours          =  2 * 60 * 60, // Meego
    FourHours         =  4 * 60 * 60, // Nemomobile
    EightHours        =  8 * 60 * 60, // Nemomobile
    TenHours          = 10 * 60 * 60, // Meego
    TwelveHours       = 12 * 60 * 60, // Android
    TwentyFourHours   = 24 * 60 * 60, // Android

    MaximumFrequency  =   0x7fffffff, // due to 32-bit libiphb ranges
  };

  explicit BackgroundActivity(QObject *parent = 0);
  virtual ~BackgroundActivity(void);

  Frequency wakeupFrequency(void) const;
  void wakeupRange(int &, int &) const;

  bool isWaiting(void) const;
  bool isRunning(void) const;
  bool isStopped(void) const;
  BackgroundActivity::State state(void) const;

  void setWakeupFrequency(Frequency slot);
  void setWakeupRange(int min_delay, int max_delay);
  void setState(BackgroundActivity::State new_state);

  void wait(Frequency slot);
  void wait(int min_delay, int max_delay=-1);

  QString id() const;

public slots:
  void wait(void);
  void run(void);
  void stop(void);

signals:
  void waiting(void);
  void running(void);
  void stopped(void);
  void stateChanged(void);

  void wakeupFrequencyChanged(void);
  void wakeupRangeChanged(void);

private:
  /* Block the default copy-constructor */
  BackgroundActivity(const BackgroundActivity &that);
  BackgroundActivityPrivate *priv;
};

#endif // BACKGROUNDACTIVITY_H_
