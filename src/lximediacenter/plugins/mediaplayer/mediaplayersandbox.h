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

#ifndef MEDIAPLAYERSANDBOX_H
#define MEDIAPLAYERSANDBOX_H

#include <LXiMediaCenter>
#include "mediadatabase.h"
#include "slideshownode.h"

namespace LXiMediaCenter {
namespace MediaPlayerBackend {

class MediaPlayerSandbox : public BackendSandbox
{
Q_OBJECT
public:
  explicit                      MediaPlayerSandbox(const QString &, QObject *parent = NULL);

  virtual void                  initialize(SSandboxServer *);
  virtual void                  close(void);

public: // From SandboxServer::Callback
  virtual SSandboxServer::SocketOp handleHttpRequest(const SSandboxServer::RequestHeader &, QIODevice *);

private slots:
  void                          cleanStreams(void);

public:
  static const char     * const path;

private:
  SSandboxServer              * server;
  QMutex                        mutex;
  QList<MediaStream *>          streams;
  QTimer                        cleanStreamsTimer;
};

class SandboxFileStream : public MediaTranscodeStream
{
Q_OBJECT
public:
  explicit                      SandboxFileStream(const QString &fileName);
  virtual                       ~SandboxFileStream();

  bool                          setup(const SHttpServer::RequestHeader &, QIODevice *);

public:
  SFileInputNode                file;
};

class SandboxPlaylistStream : public MediaTranscodeStream
{
Q_OBJECT
public:
  explicit                      SandboxPlaylistStream(const SMediaInfoList &files);

  bool                          setup(const SHttpServer::RequestHeader &, QIODevice *);

public slots:
  void                          opened(const QString &, quint16);
  void                          closed(const QString &, quint16);

private:
  QString                       currentFile;
  SPlaylistNode                 playlistNode;
};

class SandboxSlideShowStream : public MediaStream
{
Q_OBJECT
public:
  explicit                      SandboxSlideShowStream(const SMediaInfoList &files);

  bool                          setup(const SHttpServer::RequestHeader &, QIODevice *);

public:
  SlideShowNode                 slideShow;
};


} } // End of namespaces

#endif