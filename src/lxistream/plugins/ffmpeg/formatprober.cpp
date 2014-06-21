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

#include "formatprober.h"
#include "bufferreader.h"
#include "videodecoder.h"
#include "videoencoder.h"
#include "videoresizer.h"

namespace LXiStream {
namespace FFMpegBackend {

FormatProber::FormatProber(const QString &, QObject *parent)
  : SInterfaces::FormatProber(parent),
    bufferReader(NULL),
    lastIoDevice(NULL),
    waitForKeyFrame(true)
{
}

FormatProber::~FormatProber()
{
  if (bufferReader)
  {
    bufferReader->stop();
    delete bufferReader;
  }
}

void FormatProber::readFormat(ProbeInfo &pi, const QByteArray &buffer)
{
  if (!pi.fileInfo.isDir && (buffer.size() >= (defaultProbeSize / 2)))
  {
    QByteArray fileName("");
    if (!pi.filePath.isEmpty())
    {
      const QString path = pi.filePath.path();
      fileName = path.mid(path.lastIndexOf('/') + 1).toUtf8();
    }

    if (!fileName.toLower().endsWith(".sub")) // .sub files cause crashes
    {
      ::AVProbeData probeData;
      probeData.filename = fileName.data();
      probeData.buf = (uchar *)buffer.data();
      probeData.buf_size = buffer.size();

      ::AVInputFormat * format = ::av_probe_input_format(&probeData, true);
      if (format && format->name)
      {
        pi.format.format = format->name;
        pi.format.fileTypeName = format->long_name;

        if (FFMpegCommon::isVideoFormat(format->name))
          pi.format.fileType = ProbeInfo::FileType_Video;
        else if (FFMpegCommon::isAudioFormat(format->name))
          pi.format.fileType = ProbeInfo::FileType_Audio;

        pi.isFormatProbed = true;
      }
    }
  }
}

void FormatProber::readContent(ProbeInfo &pi, QIODevice *ioDevice)
{
  BufferReader * const bufferReader = createBufferReader(pi.format.format, ioDevice, pi.filePath);
  if (bufferReader)
  {
    for (int title=0, n=bufferReader->numTitles(); title<n; title++)
    {
      while (pi.content.titles.count() <= title)
        pi.content.titles += ProbeInfo::Title();

      ProbeInfo::Title &mainTitle = pi.content.titles[title];

      const STime duration = bufferReader->duration();
      if (duration.isValid() && (duration > mainTitle.duration))
        mainTitle.duration = duration;

      const QList<Chapter> chapters = bufferReader->chapters();
      if (!chapters.isEmpty())
        mainTitle.chapters = chapters;

      const QList<AudioStreamInfo> audioStreams = bufferReader->audioStreams(title);
      if (!audioStreams.isEmpty())
        mainTitle.audioStreams = audioStreams;

      const QList<VideoStreamInfo> videoStreams = bufferReader->videoStreams(title);
      if (!videoStreams.isEmpty())
        mainTitle.videoStreams = videoStreams;

      // This is needed to detect all subtitle streams in MPEG files.
      if (pi.format.format.contains("vob") ||
          pi.format.format.contains("mpeg"))
      {
        int iter = 4096, bufferCount = 0;

        while ((bufferCount < minBufferCount) && (--iter > 0))
        {
          if (!bufferReader->process())
            break;

          while (!videoBuffers.isEmpty())
          {
            videoBuffers.takeFirst();
            bufferCount++;
          }
        }

        // Skip first 5%
        const STime duration = bufferReader->duration();
        if (!pi.format.format.contains("mpegts") && duration.isPositive()) // mpegts has very slow seeking.
          bufferReader->setPosition(duration / 20);

        while ((bufferCount < maxBufferCount) && (--iter > 0))
        {
          if (!bufferReader->process())
            break;

          while (!videoBuffers.isEmpty())
          {
            videoBuffers.takeFirst();
            bufferCount++;
          }
        }
      }

      // Subtitle streams may not be visible after reading some data (as the
      // first subtitle usually appears after a few minutes).
      const QList<DataStreamInfo> dataStreams = bufferReader->dataStreams(title);
      if (!dataStreams.isEmpty())
        mainTitle.dataStreams = dataStreams;

      videoBuffers.clear();
    }

    setMetadata(pi, bufferReader);

    pi.isContentProbed = true;
  }
}

SVideoBuffer FormatProber::readThumbnail(const ProbeInfo &pi, QIODevice *ioDevice, const QSize &thumbSize)
{
  if (ioDevice)
  {
    QUrl filePath;
    QFile const * file = qobject_cast<QFile *>(ioDevice);
    if (file)
    {
      filePath.setPath(file->fileName());
      filePath.setScheme("file");
    }

    BufferReader * const bufferReader = createBufferReader(pi.format.format, ioDevice, filePath);
    if (bufferReader)
      return readThumbnail(pi, bufferReader, thumbSize);
  }

  return SVideoBuffer();
}

void FormatProber::produce(const SEncodedAudioBuffer &)
{
}

void FormatProber::produce(const SEncodedVideoBuffer &videoBuffer)
{
  if (!waitForKeyFrame || videoBuffer.isKeyFrame())
  {
    videoBuffers += videoBuffer;
    waitForKeyFrame = false;
  }
}

void FormatProber::produce(const SEncodedDataBuffer &)
{
}

SVideoBuffer FormatProber::readThumbnail(const ProbeInfo &pi, BufferReader *bufferReader, const QSize &thumbSize)
{
  SVideoBuffer result;

  for (int title=0, n=bufferReader->numTitles(); (title<n) && result.isNull(); title++)
  {
    const QList<VideoStreamInfo> videoStreams = bufferReader->videoStreams(title);
    if (!videoStreams.isEmpty())
    {
      static const int minDist = 80;
      int iter = 4096;

      bufferReader->selectStreams(title, QVector<StreamId>() << videoStreams.first());

      while ((videoBuffers.count() < minBufferCount) && (--iter > 0))
      {
        if (!bufferReader->process())
          break;

        while (!videoBuffers.isEmpty() && videoBuffers.first().codec().isNull())
          videoBuffers.takeFirst();
      }

      // Skip first 5%
      const STime duration = bufferReader->duration();
      if (!pi.format.format.contains("mpegts") && duration.isPositive()) // mpegts has very slow seeking.
        bufferReader->setPosition(duration / 20);

      // Extract the thumbnail
      if (!videoBuffers.isEmpty() && !videoBuffers.first().codec().isNull())
      {
        VideoDecoder videoDecoder(QString::null, this);
        if (videoDecoder.openCodec(
                videoBuffers.first().codec(),
                bufferReader,
                VideoDecoder::Flag_Fast))
        {
          SVideoBuffer thumbnail;
          int bestDist = -1, counter = 0;

          do
          {
            while (!videoBuffers.isEmpty())
            foreach (const SVideoBuffer &decoded, videoDecoder.decodeBuffer(videoBuffers.takeFirst()))
            if (!decoded.isNull())
            {
              // Get all greyvalues
              QVector<quint8> pixels;
              pixels.reserve(4096);
              for (unsigned y=0, n=decoded.format().size().height(), i=n/32; y<n; y+=i)
              {
                const quint8 * const line = reinterpret_cast<const quint8 *>(decoded.scanLine(y, 0));
                for (int x=0, n=decoded.format().size().width(), i=n/32; x<n; x+=i)
                  pixels += line[x];
              }

              qSort(pixels);
              const int dist = (counter / 10) + (int(pixels[pixels.count() * 3 / 4]) - int(pixels[pixels.count() / 4]));
              if (dist >= bestDist)
              {
                thumbnail = decoded;
                bestDist = dist;
              }

              counter++;
            }

            if (bestDist < minDist)
            while ((videoBuffers.count() < minBufferCount) && (--iter > 0))
            {
              if (!bufferReader->process())
                break;
            }
          }
          while (!videoBuffers.isEmpty() && (bestDist < minDist) && (counter < maxBufferCount));

          // Build thumbnail
          if (!thumbnail.isNull())
          {
            VideoResizer videoResizer("bilinear", this);
            videoResizer.setSize(thumbSize);
            videoResizer.setAspectRatioMode(Qt::KeepAspectRatio);
            result = videoResizer.processBuffer(thumbnail);
          }
        }
      }

      videoBuffers.clear();
    }
  }

  return result;
}

BufferReader * FormatProber::createBufferReader(const QString &format, QIODevice *ioDevice, const QUrl &)
{
  if (ioDevice != lastIoDevice)
  {
    if (bufferReader)
    {
      bufferReader->stop();
      delete bufferReader;
      bufferReader = NULL;
    }

    bufferReader = new BufferReader(QString::null, this);
    if (bufferReader->openFormat(format))
    if (bufferReader->start(ioDevice, this, false, true))
    {
      lastIoDevice = ioDevice;

      videoBuffers.clear();
      waitForKeyFrame = true;

      return bufferReader;
    }

    delete bufferReader;
    bufferReader = NULL;
  }
  else if (bufferReader)
    bufferReader->setPosition(STime::null);

  videoBuffers.clear();
  waitForKeyFrame = true;

  return bufferReader;
}

void FormatProber::setMetadata(ProbeInfo &pi, const BufferReader *bufferReader)
{
  for (AVDictionaryEntry *e = av_dict_get(bufferReader->context()->metadata, "", NULL, AV_DICT_IGNORE_SUFFIX);
       e != NULL;
       e = av_dict_get(bufferReader->context()->metadata, "", e, AV_DICT_IGNORE_SUFFIX))
  {
    setMetadata(pi, e->key, QString::fromUtf8(e->value).simplified());
  }
}

void FormatProber::setMetadata(ProbeInfo &pi, const char *name, const QString &value)
{
  if (!value.isEmpty())
    pi.content.metadata.insert(QString::fromUtf8(name), value);
}

} } // End of namespaces
