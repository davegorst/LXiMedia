/***************************************************************************
 *   Copyright (C) 2007 by A.J. Admiraal                                   *
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

#ifndef LXISTREAM_SAUDIOMATRIXNODE_H
#define LXISTREAM_SAUDIOMATRIXNODE_H

#include <QtCore>
#include <LXiCore>
#include "../saudiobuffer.h"
#include "../sgraph.h"

namespace LXiStream {


/*! This audio filter can be used to apply a surround matrix to an audio stream.
    This surround matrix can be used to map a stereo audio stream to a surrond
    setup by, dor example duplicating the left and right channels to the front
    and rear channels. It can also mix the left and right channels into one
    channel for the center speaker and the subwoofer.
 */
class S_DSO_PUBLIC SAudioMatrixNode : public QObject,
                                      public SGraph::Node
{
Q_OBJECT
public:
  enum Mode { ALL = -1, MONO = 0, STEREO = 1, QUADRA = 2, SURROUND = 3 };
  enum Channel { FRONT_LEFT = 0, CENTER = 1, FRONT_RIGHT = 2, REAR_LEFT = 3, REAR_RIGHT = 4, LFE = 5 };

public:
                                SAudioMatrixNode(SGraph *);
  virtual                       ~SAudioMatrixNode();

  quint32                       inChannels(void) const;
  void                          setInChannels(quint32);
  quint32                       outChannels(void) const;
  void                          setOutChannels(quint32);
  const QList<qreal>          & matrix(void) const;
  void                          setMatrix(const QList<qreal> &);

public slots:
  void                          input(const SAudioBuffer &);

signals:
  void                          output(const SAudioBuffer &);

private:
  void                          processTask(const SAudioBuffer &);

  static Channel                getChannel(Mode, unsigned);
  static Mode                   getMode(unsigned);
  void                          prepareMatrix(unsigned);

private:
  struct Data;
  Data                  * const d;
};


} // End of namespace

#endif
