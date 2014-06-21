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

#ifndef __VIDEORESIZER_H
#define __VIDEORESIZER_H

#include <QtCore>
#include <LXiStream>
#include "ffmpegcommon.h"

namespace LXiStream {
namespace FFMpegBackend {

class VideoResizer : public SInterfaces::VideoResizer
{
Q_OBJECT
private:
  struct Slice
  {
    int stride[4];
    quint8 * line[4];
  };

public:
                                VideoResizer(const QString &, QObject *);
  virtual                       ~VideoResizer();

  static int                    algoFlags(const QString &);

public: // From SInterfaces::VideoResizer
  virtual void                  setSize(const SSize &);
  virtual SSize                 size(void) const;
  virtual void                  setAspectRatioMode(Qt::AspectRatioMode);
  virtual Qt::AspectRatioMode   aspectRatioMode(void) const;

  virtual bool                  needsResize(const SVideoFormat &);
  virtual SVideoBuffer          processBuffer(const SVideoBuffer &);

private:
  bool                          processSlice(int, int, int, const Slice &, const Slice &);

  static ::SwsFilter          * createDeinterlaceFilter(void);

private:
  static const int              baseOverlap = 8;
  const int                     filterFlags;
  int                           filterOverlap;
  SSize                         scaleSize;
  Qt::AspectRatioMode           scaleAspectRatioMode;
  SVideoFormat                  lastFormat;
  SVideoFormat                  destFormat;
  SVideoFormatConvertNode       formatConvert;

  int                           numThreads;
  ::SwsContext                * swsContext[2];
  ::SwsFilter                 * preFilter;
};


} } // End of namespaces

#endif
