/***************************************************************************
 *   Copyright (C) 2010 by A.J. Admiraal                                   *
 *   code@admiraal.dds.nl                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#ifndef LXISERVER_SANDBOXSERVER_H
#define LXISERVER_SANDBOXSERVER_H

#include <QtCore>
#include "httpengine.h"

namespace LXiServer {

class SandboxServer : public HttpServerEngine
{
Q_OBJECT
public:
  explicit                      SandboxServer(QObject * = NULL);
  virtual                       ~SandboxServer();

  void                          initialize(const QString &name, const QString &mode);
  void                          close(void);

signals:
  void                          busy(void);
  void                          idle(void);

protected: // From HttpServerEngine
  virtual QIODevice           * openSocket(quintptr, int timeout);
  virtual void                  closeSocket(QIODevice *, bool canReuse, int timeout);

private:
  class Server;
  struct Private;
  Private               * const p;
};

} // End of namespace

#endif
