/******************************************************************************
 *   Copyright (C) 2014  A.J. Admiraal                                        *
 *   code@admiraal.dds.nl                                                     *
 *                                                                            *
 *   This program is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License version 3 as        *
 *   published by the Free Software Foundation.                               *
 *                                                                            *
 *   This program is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *   GNU General Public License for more details.                             *
 *                                                                            *
 *   You should have received a copy of the GNU General Public License        *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 ******************************************************************************/

#ifndef __NETWORKBUFFERREADER_H
#define __NETWORKBUFFERREADER_H

#include <QtCore>
#include <LXiStream>
#include "ffmpegcommon.h"
#include "bufferreaderbase.h"

namespace LXiStream {
namespace FFMpegBackend {

class NetworkBufferReader : public SInterfaces::NetworkBufferReader,
                            public BufferReaderBase
{
Q_OBJECT
public:
  explicit                      NetworkBufferReader(const QString &, QObject *);
  virtual                       ~NetworkBufferReader();

public: // From SInterfaces::AbstractBufferReader
  inline virtual STime          duration(void) const                            { return BufferReaderBase::duration(); }
  inline virtual bool           setPosition(STime p)                            { return BufferReaderBase::setPosition(p); }
  inline virtual STime          position(void) const                            { return BufferReaderBase::position(); }
  inline virtual QList<Chapter> chapters(void) const                            { return BufferReaderBase::chapters(); }

  inline virtual int            numTitles(void) const                           { return 1; }
  inline virtual QList<AudioStreamInfo> audioStreams(int) const                 { return BufferReaderBase::audioStreams(); }
  inline virtual QList<VideoStreamInfo> videoStreams(int) const                 { return BufferReaderBase::videoStreams(); }
  inline virtual QList<DataStreamInfo> dataStreams(int) const                   { return BufferReaderBase::dataStreams(); }
  inline virtual void           selectStreams(int, const QVector<StreamId> &s)  { return BufferReaderBase::selectStreams(s); }

  virtual bool                  process(void);

public: // From SInterfaces::NetworkBufferReader
  virtual bool                  openProtocol(const QString &);

  virtual bool                  start(const QUrl &url, ProduceCallback *);
  virtual void                  stop(void);

  inline virtual bool           buffer(void)                                    { return BufferReaderBase::buffer(); }
  inline virtual STime          bufferDuration(void) const                      { return BufferReaderBase::bufferDuration(); }

private:
  QList<QUrl>                   resolveAsf(const QUrl &);
};

} } // End of namespaces

#endif
