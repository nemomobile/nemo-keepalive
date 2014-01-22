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

#ifndef HEARTBEAT_H_
# define HEARTBEAT_H_

# include <QObject>
# include <QSocketNotifier>
# include <QTimer>

extern "C" {
# include <iphbd/libiphb.h>
}

class Heartbeat : public QObject
{
  Q_OBJECT

private:
  // block copy constructor
  Heartbeat(const Heartbeat &that);

  int              m_min_delay;
  int              m_max_delay;

  bool             m_started;
  bool             m_waiting;

  iphb_t           m_iphb_handle;
  QSocketNotifier *m_wakeup_notifier;
  QTimer          *m_connect_timer;

private:
  bool tryConnect(void);
  void connect(void);

private slots:
  void retryConnect(void);
  void wakeup(int fd);
  void wait(void);

public:
  explicit Heartbeat(QObject *parent = 0);
  virtual ~Heartbeat(void);

  void setInterval(int global_slot);
  void setInterval(int mindelay, int maxdelay);

  void start(void);
  void start(int global_slot);
  void start(int mindelay, int maxdelay);

  void stop(void);

  void disconnect(void);

signals:
  void timeout(void);

};
#endif /* HEARTBEAT_H_ */
