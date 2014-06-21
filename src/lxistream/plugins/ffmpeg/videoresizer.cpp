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

#include "videoresizer.h"
#include "ffmpegcommon.h"
#include <QtConcurrent>

namespace LXiStream {
namespace FFMpegBackend {

VideoResizer::VideoResizer(const QString &scheme, QObject *parent)
  : SInterfaces::VideoResizer(parent),
    filterFlags(algoFlags(scheme)),
    filterOverlap(baseOverlap),
    scaleSize(),
    scaleAspectRatioMode(Qt::KeepAspectRatio),
    lastFormat(SVideoFormat::Format_Invalid),
    destFormat(SVideoFormat::Format_Invalid),
    formatConvert(NULL),
    numThreads(0),
    preFilter((scheme == "deinterlace") ? createDeinterlaceFilter() : NULL)
{
  for (int i=0; i<2; i++)
    swsContext[i] = NULL;
}

VideoResizer::~VideoResizer()
{
  for (int i=0; i<2; i++)
  if (swsContext[i])
    ::sws_freeContext(swsContext[i]);

  delete preFilter;
}

int VideoResizer::algoFlags(const QString &name)
{
  if (name == "lanczos")
    return SWS_LANCZOS;
  else if (name == "bicubic")
    return SWS_BICUBIC;
  else // if ((name == "bilinear") || (name == "deinterlace"))
    return SWS_BILINEAR;
}

void VideoResizer::setSize(const SSize &size)
{
  scaleSize = size;
}

SSize VideoResizer::size(void) const
{
  return scaleSize;
}

void VideoResizer::setAspectRatioMode(Qt::AspectRatioMode a)
{
  scaleAspectRatioMode = a;
}

Qt::AspectRatioMode VideoResizer::aspectRatioMode(void) const
{
  return scaleAspectRatioMode;
}

bool VideoResizer::needsResize(const SVideoFormat &format)
{
  if (lastFormat != format)
  {
    lastFormat = format;

    QSize size = lastFormat.size().absoluteSize();
    size.scale(scaleSize.absoluteSize(), scaleAspectRatioMode);
    if (scaleSize.aspectRatio() > 1.0)
      size.setWidth(int(size.width() / scaleSize.aspectRatio()));
    else if (scaleSize.aspectRatio() < 1.0)
      size.setHeight(int(size.height() / (1.0f / scaleSize.aspectRatio())));

    size.setHeight((size.height() + 1) & ~1);

    if ((lastFormat.format() == SVideoFormat::Format_YUYV422) ||
        (lastFormat.format() == SVideoFormat::Format_UYVY422))
    {
      formatConvert.setDestFormat(SVideoFormat::Format_YUV422P);
    }
    else
      formatConvert.setDestFormat(lastFormat.format());

    destFormat = SVideoFormat(
        formatConvert.destFormat(),
        size,
        lastFormat.frameRate(),
        lastFormat.fieldMode());

    for (int i=0; i<2; i++)
    if (swsContext[i])
    {
      ::sws_freeContext(swsContext[i]);
      swsContext[i] = NULL;
    }

    if (destFormat.size() != lastFormat.size())
    {
      QSize dstSize = lastFormat.size().size();
      dstSize.scale(destFormat.size().size(), Qt::KeepAspectRatio);

      filterOverlap = qMax(
          int((float(baseOverlap) * (float(lastFormat.size().height()) / float(dstSize.height()))) + 0.5f),
          baseOverlap + 0);

      if (filterOverlap < (lastFormat.size().height() / 4))
        numThreads = qBound(1, QThreadPool::globalInstance()->maxThreadCount(), 2);
      else
        numThreads = 1;

      for (int i=0; i<numThreads; i++)
      {
        const ::PixelFormat pf = FFMpegCommon::toFFMpegPixelFormat(destFormat);

        swsContext[i] = ::sws_getCachedContext(
            NULL,
            lastFormat.size().width(), lastFormat.size().height(), pf,
            destFormat.size().width(), destFormat.size().height(), pf,
            filterFlags,
            preFilter, NULL, NULL);

        if (swsContext[i] == NULL)
        { // Fallback
          swsContext[i] = ::sws_getCachedContext(
              NULL,
              lastFormat.size().width(), lastFormat.size().height(), pf,
              destFormat.size().width(), destFormat.size().height(), pf,
              0,
              preFilter, NULL, NULL);
        }
      }

      if (swsContext[1] == NULL)
        numThreads = 1;
    }
  }

  return swsContext[0] != NULL;
}

SVideoBuffer VideoResizer::processBuffer(const SVideoBuffer &videoBuffer)
{
  if (!scaleSize.isNull() && !videoBuffer.isNull() && VideoResizer::needsResize(videoBuffer.format()))
  {
    const SVideoBuffer srcBuffer = formatConvert.convert(videoBuffer);
    SVideoBuffer destBuffer(destFormat);

    int wf = 1, hf = 1;
    const bool planar = destFormat.planarYUVRatio(wf, hf);

    bool result = true;
    QVector< QFuture<bool> > futures;
    futures.reserve(numThreads);

    const int imageHeight = lastFormat.size().height();
    const int stripSrcHeight = qMin(imageHeight, (imageHeight / numThreads) + filterOverlap);
    for (int i=0; i<numThreads; i++)
    {
      const int stripSrcPos = (i == 0) ? 0 : (imageHeight - stripSrcHeight);

      Slice src;
      src.stride[0] = srcBuffer.lineSize(0);
      src.stride[1] = planar ? srcBuffer.lineSize(1) : 0;
      src.stride[2] = planar ? srcBuffer.lineSize(2) : 0;
      src.stride[3] = planar ? srcBuffer.lineSize(3) : 0;
      src.line[0] = (quint8 *)srcBuffer.scanLine(0, 0) + (stripSrcPos * src.stride[0]);
      src.line[1] = planar ? ((quint8 *)srcBuffer.scanLine(0, 1) + ((stripSrcPos / hf) * src.stride[1])) : NULL;
      src.line[2] = planar ? ((quint8 *)srcBuffer.scanLine(0, 2) + ((stripSrcPos / hf) * src.stride[2])) : NULL;
      src.line[3] = planar ? ((quint8 *)srcBuffer.scanLine(0, 3) + ((stripSrcPos / hf) * src.stride[3])) : NULL;

      Slice dst;
      dst.stride[0] = destBuffer.lineSize(0);
      dst.stride[1] = planar ? destBuffer.lineSize(1) : 0;
      dst.stride[2] = planar ? destBuffer.lineSize(2) : 0;
      dst.stride[3] = planar ? destBuffer.lineSize(3) : 0;
      dst.line[0] = (quint8 *)destBuffer.scanLine(0, 0);
      dst.line[1] = planar ? (quint8 *)destBuffer.scanLine(0, 1) : NULL;
      dst.line[2] = planar ? (quint8 *)destBuffer.scanLine(0, 2) : NULL;
      dst.line[3] = planar ? (quint8 *)destBuffer.scanLine(0, 3) : NULL;

      if (numThreads > 1)
        futures += QtConcurrent::run(this, &VideoResizer::processSlice, i, stripSrcPos, stripSrcHeight, src, dst);
      else
        result = processSlice(i, stripSrcPos, stripSrcHeight, src, dst);
    }

    if (numThreads > 1)
    for (int i=0; i<numThreads; i++)
      result &= futures[i].result();

    if (result)
    {
      destBuffer.setTimeStamp(srcBuffer.timeStamp());
      return destBuffer;
    }
  }

  return videoBuffer;
}

bool VideoResizer::processSlice(int strip, int stripPos, int stripHeight, const Slice &src, const Slice &dst)
{
  LXI_PROFILE_FUNCTION(TaskType_VideoProcessing);

  int wf = 1, hf = 1;
  const bool planar = destFormat.planarYUVRatio(wf, hf);

  const bool result = ::sws_scale(
      swsContext[strip],
      (uint8_t **)src.line,
      (int *)src.stride,
      stripPos, stripHeight,
      (uint8_t **)dst.line,
      (int *)dst.stride) >= 0;

  // Correct chroma lines on the top
  if ((strip == 0) && planar && result && destFormat.isYUV() && dst.line[1] && dst.line[2])
  {
    const int skip = 4 / hf;

    quint8 * const ref[2] = {
      dst.line[1] + (dst.stride[1] * skip),
      dst.line[2] + (dst.stride[2] * skip) };

    for (int y=0; y<skip-1; y++)
    {
      memcpy(dst.line[1] + (dst.stride[1] * y), ref[0], dst.stride[1]);
      memcpy(dst.line[2] + (dst.stride[2] * y), ref[1], dst.stride[2]);
    }
  }

  return result;
}

::SwsFilter * VideoResizer::createDeinterlaceFilter(void)
{
  struct Vector1 : ::SwsVector
  {
    inline Vector1(void) { c[0] = 1.0; coeff = c; length = 1; }

    double c[1];
  };

  struct Vector3 : ::SwsVector
  {
    inline Vector3(void) { c[0] = 0.25; c[1] = 0.5; c[2] = 0.25; coeff = c; length = 3; }

    double c[3];
  };

  struct Filter : ::SwsFilter
  {
    inline Filter(void) { lumH = &lh; lumV = &lv; chrH = &ch; chrV = &cv; }

    Vector1 lh, ch;
    Vector3 lv, cv;
  };

  return new Filter();
}

} } // End of namespaces
