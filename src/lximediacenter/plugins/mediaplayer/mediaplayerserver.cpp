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

#include "mediaplayerserver.h"
#include "mediaplayersandbox.h"
#include "module.h"

namespace LXiMediaCenter {
namespace MediaPlayerBackend {

const char MediaPlayerServer::dirSplit =
#if defined(Q_OS_UNIX)
    ':';
#elif  defined(Q_OS_WIN)
    ';';
#else
#error Not implemented.
#endif

const Qt::CaseSensitivity MediaPlayerServer::caseSensitivity =
#if defined(Q_OS_UNIX)
    Qt::CaseSensitive;
#elif  defined(Q_OS_WIN)
    Qt::CaseInsensitive;
#else
#error Not implemented.
#endif

const QEvent::Type  MediaPlayerServer::responseEventType = QEvent::Type(QEvent::registerEventType());
const int           MediaPlayerServer::maxSongDurationMin = 7;

MediaPlayerServer::MediaPlayerServer(const QString &, QObject *parent)
  : MediaServer(parent),
    masterServer(NULL),
    mediaDatabase(NULL)
{
  PluginSettings settings(Module::pluginName);

  QStringList paths;
  foreach (const QString &root, settings.value("RootPaths").toStringList())
  if (!root.trimmed().isEmpty() && !isHidden(root) && QDir(root).exists())
    paths.append(root);

  setRootPaths(paths);
}

void MediaPlayerServer::initialize(MasterServer *masterServer)
{
  this->masterServer = masterServer;
  this->mediaDatabase = MediaDatabase::createInstance(masterServer);

  MediaServer::initialize(masterServer);
}

void MediaPlayerServer::close(void)
{
  MediaServer::close();
}

QString MediaPlayerServer::serverName(void) const
{
  return Module::pluginName;
}

QString MediaPlayerServer::serverIconPath(void) const
{
  return "/img/media-tape.png";
}

SHttpServer::ResponseMessage MediaPlayerServer::httpRequest(const SHttpServer::RequestMessage &request, QIODevice *socket)
{
  if (request.isGet())
  {
    if (request.url().hasQueryItem("thumbnail"))
    {
      const QSize size = SSize::fromString(request.url().queryItemValue("thumbnail")).size();
      QString defaultIcon = ":/img/null.png";
      QByteArray content;

      const FileNode node = mediaDatabase->readNode(realPath(request.file()));
      if (!node.isNull())
      {
        if (!node.thumbnail().isNull())
        {
          SImage image = node.thumbnail();
          if (!image.isNull())
            image = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

          QImage result(size, QImage::Format_ARGB32);
          QPainter p;
          p.begin(&result);
          p.setCompositionMode(QPainter::CompositionMode_Source); // Ignore alpha
          p.fillRect(result.rect(), Qt::transparent);
          p.setCompositionMode(QPainter::CompositionMode_SourceOver); // Process alpha
          p.drawImage(
              (result.width() / 2) - (image.width() / 2),
              (result.height() / 2) - (image.height() / 2),
              image);

          if (request.url().hasQueryItem("overlay"))
          {
            QImage overlayImage(":/lximediacenter/images/" + request.url().queryItemValue("overlay") + ".png");
            if (!overlayImage.isNull())
            {
              overlayImage = overlayImage.scaled(size / 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);

              p.drawImage(
                  (result.width() / 2) - (overlayImage.width() / 2),
                  (result.height() / 2) - (overlayImage.height() / 2),
                  overlayImage);
            }
          }

          p.end();

          QBuffer b(&content);
          result.save(&b, "PNG");
        }

        switch (node.fileType())
        {
        case FileNode::ProbeInfo::FileType_None:  defaultIcon = ":/img/misc.png";           break;
        case FileNode::ProbeInfo::FileType_Audio: defaultIcon = ":/img/audio-file.png";     break;
        case FileNode::ProbeInfo::FileType_Video: defaultIcon = ":/img/video-file.png";     break;
        case FileNode::ProbeInfo::FileType_Image: defaultIcon = ":/img/image-file.png";     break;
        case FileNode::ProbeInfo::FileType_Disc:  defaultIcon = ":/img/media-optical.png";  break;
        }
      }

      if (content.isEmpty())
      {
        QImage image(defaultIcon);
        if (!image.isNull())
        {
          image = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

          QBuffer b(&content);
          image.save(&b, "PNG");
        }
      }

      if (!content.isEmpty())
        return SSandboxServer::ResponseMessage(request, SSandboxServer::Status_Ok, content, SHttpEngine::mimeImagePng);
      else
        return SSandboxServer::ResponseMessage(request, SHttpServer::Status_NotFound);
    }
    else if (request.url().hasQueryItem("save_settings"))
    {
      PluginSettings settings(Module::pluginName);
      settings.setValue("SlideDuration", qBound(2500, request.url().queryItemValue("slideduration").toInt(), 60000));

      SHttpServer::ResponseMessage response(request, SHttpServer::Status_MovedPermanently);
      response.setField("Location", "http://" + request.host() + "/settings");
      return response;
    }
    else if (request.url().hasQueryItem("folder_tree"))
    {
      QStringList paths = rootPaths.values();

      const QString open = request.url().queryItemValue("open");
      const QStringList allopen = !open.isEmpty()
                                  ? QString::fromUtf8(qUncompress(QByteArray::fromHex(open.toAscii()))).split(dirSplit)
                                  : QStringList();

      const QString checkon = QString::fromUtf8(QByteArray::fromHex(request.url().queryItemValue("checkon").toAscii()));
      const QString checkoff = QString::fromUtf8(QByteArray::fromHex(request.url().queryItemValue("checkoff").toAscii()));
      if (!checkon.isEmpty() || !checkoff.isEmpty())
      {
        if (!checkon.isEmpty())
          paths.append(checkon.endsWith('/') ? checkon : (checkon + '/'));

        if (!checkoff.isEmpty())
        for (QStringList::Iterator i=paths.begin(); i!=paths.end(); )
        if (i->compare(checkoff, caseSensitivity) == 0)
          i = paths.erase(i);
        else
          i++;

        setRootPaths(paths);
      }

      HtmlParser htmlParser;
      htmlParser.setField("SERVER_PATH", QUrl(serverPath()).toEncoded());
      htmlParser.setField("DIRS", QByteArray(""));
      generateDirs(htmlParser, driveInfoList.values(), 0, allopen, paths);

      SHttpServer::ResponseMessage response(request, SHttpServer::Status_Ok);
      response.setField("Cache-Control", "no-cache");
      response.setContentType(SHttpEngine::mimeTextHtml);
      response.setContent(htmlParser.parse(htmlSettingsDirTreeIndex));
      return response;
    }
  }

  return MediaServer::httpRequest(request, socket);
}

MediaPlayerServer::Stream * MediaPlayerServer::streamVideo(const SHttpServer::RequestMessage &request)
{
  SSandboxClient::Priority priority = SSandboxClient::Priority_Normal;
  if (request.url().queryItemValue("priority") == "low")
    priority = SSandboxClient::Priority_Low;
  else if (request.url().queryItemValue("priority") == "high")
    priority = SSandboxClient::Priority_High;

  SSandboxClient * const sandbox = masterServer->createSandbox(priority);
  connect(sandbox, SIGNAL(consoleLine(QString)), SLOT(consoleLine(QString)));
  sandbox->ensureStarted();

  const QString filePath = realPath(request.file());
  if (!filePath.isEmpty())
  {
    QUrl rurl;
    rurl.setPath(MediaPlayerSandbox::path);
    rurl.addQueryItem("play", QString::null);
    typedef QPair<QString, QString> QStringPair;
    foreach (const QStringPair &queryItem, request.url().queryItems())
      rurl.addQueryItem(queryItem.first, queryItem.second);

    if (request.hasField("timeSeekRange.dlna.org"))
    {
      int pos = rurl.queryItemValue("position").toInt();
      rurl.removeAllQueryItems("position");

      QString ntp = request.field("timeSeekRange.dlna.org");
      ntp = ntp.mid(ntp.indexOf("ntp=") + 4);
      ntp = ntp.left(ntp.indexOf('-'));
      pos += int(ntp.toFloat() + 0.5f);

      rurl.addQueryItem("position", QString::number(pos));
    }

    Stream *stream = new Stream(this, sandbox, request.path());
    if (stream->setup(rurl, filePath.toUtf8()))
      return stream; // The graph owns the socket now.

    delete stream;
  }

  disconnect(sandbox, SIGNAL(consoleLine(QString)), this, SLOT(consoleLine(QString)));
  masterServer->recycleSandbox(sandbox);

  return NULL;
}

SHttpServer::ResponseMessage MediaPlayerServer::sendPhoto(const SHttpServer::RequestMessage &request)
{
  const QString filePath = realPath(request.file());
  if (!filePath.isEmpty())
  {
    SSize size(4096, 4096);
    if (request.url().hasQueryItem("resolution"))
      size = SSize::fromString(request.url().queryItemValue("resolution"));

    QString format = "png";
    if (request.url().hasQueryItem("format"))
      format = request.url().queryItemValue("format");

    QString contentType = "image/" + format.toLower();

    const QString contentFeatures = QByteArray::fromHex(request.url().queryItemValue("contentFeatures").toAscii());
    const MediaProfiles::ImageProfile imageProfile = mediaProfiles().imageProfileFor(contentFeatures);
    if (imageProfile != 0) // DLNA stream.
    {
      MediaProfiles::correctFormat(imageProfile, size);
      format = mediaProfiles().formatFor(imageProfile);
      contentType = mediaProfiles().mimeTypeFor(imageProfile);
    }

    SHttpServer::ResponseMessage response(request, SHttpServer::Status_Ok);
    response.setContentType(contentType);
    if (!contentFeatures.isEmpty())
    {
      response.setField("transferMode.dlna.org", "Interactive");
      response.setField("contentFeatures.dlna.org", contentFeatures);
    }

    if (!request.isHead())
    {
      const QByteArray result = mediaDatabase->readImage(filePath, size.size(), format);
      if (!result.isEmpty())
        response.setContent(result);
      else
        return SHttpServer::ResponseMessage(request, SHttpServer::Status_NotFound);
    }

    return response;
  }

  return SHttpServer::ResponseMessage(request, SHttpServer::Status_NotFound);
}

int MediaPlayerServer::countItems(const QString &virtualPath)
{
  if (virtualPath != serverPath())
  {
    int result = mediaDatabase->countAlbumFiles(realPath(virtualPath));
    if (result > 1)
      result++; // "Play all" item

    result += countAlbums(virtualPath);
    return result;
  }
  else
    return rootPaths.count();
}

QList<MediaPlayerServer::Item> MediaPlayerServer::listItems(const QString &virtualPath, unsigned start, unsigned count)
{
  const bool returnAll = count == 0;

  if (virtualPath != serverPath())
  {
    QList<Item> result = listAlbums(virtualPath, start, count);

    if (returnAll || (count > 0))
    {
      const QString path = realPath(virtualPath);
      if (mediaDatabase->countAlbumFiles(path) > 1)
      {
        if (start == 0)
        {
          result += makePlayAllItem(virtualPath);
          count--;
        }
        else
          start--;
      }

      if (returnAll || (count > 0))
      {
        foreach (const FileNode &node, mediaDatabase->getAlbumFiles(path, start, count))
          result.append(makeItem(node));
      }
    }

    return result;
  }
  else
  {
    QList<Item> result;

    const QStringList paths = rootPaths.keys();
    for (unsigned i=start, n=0; (int(i)<paths.count()) && (returnAll || (n<count)); i++, n++)
    {
      Item item;
      item.isDir = true;
      item.title = paths[i];
      item.path = virtualPath + paths[i] + '/';
      item.iconUrl = "/img/folder-video.png";

      result += item;
    }

    return result;
  }
}

MediaPlayerServer::Item MediaPlayerServer::getItem(const QString &virtualPath)
{
  if (virtualPath.endsWith("/.all"))
    return makePlayAllItem(virtualPath.left(virtualPath.length() - 4));
  else
    return makeItem(mediaDatabase->readNode(realPath(virtualPath)));
}

void MediaPlayerServer::customEvent(QEvent *e)
{
  if (e->type() == responseEventType)
  {
    ResponseEvent * const event = static_cast<ResponseEvent *>(e);

    SSandboxServer::sendHttpResponse(event->request, event->response, event->socket, false);
  }
  else
    MediaServer::customEvent(e);
}

bool MediaPlayerServer::isHidden(const QString &path)
{
  static QSet<QString> hiddenDirs;
  if (hiddenDirs.isEmpty())
  {
    const QDir root = QDir::root();

#if defined(Q_OS_UNIX)
    hiddenDirs += root.absoluteFilePath("bin");
    hiddenDirs += root.absoluteFilePath("boot");
    hiddenDirs += root.absoluteFilePath("dev");
    hiddenDirs += root.absoluteFilePath("etc");
    hiddenDirs += root.absoluteFilePath("lib");
    hiddenDirs += root.absoluteFilePath("proc");
    hiddenDirs += root.absoluteFilePath("sbin");
    hiddenDirs += root.absoluteFilePath("sys");
    hiddenDirs += root.absoluteFilePath("tmp");
    hiddenDirs += root.absoluteFilePath("usr");
    hiddenDirs += root.absoluteFilePath("var");
#endif

#if defined(Q_OS_MACX)
    hiddenDirs += root.absoluteFilePath("Applications");
    hiddenDirs += root.absoluteFilePath("cores");
    hiddenDirs += root.absoluteFilePath("Developer");
    hiddenDirs += root.absoluteFilePath("private");
    hiddenDirs += root.absoluteFilePath("System");
#endif

#if defined(Q_OS_WIN)
    hiddenDirs += root.absoluteFilePath("Program Files");
    hiddenDirs += root.absoluteFilePath("Program Files (x86)");
    hiddenDirs += root.absoluteFilePath("WINDOWS");
#endif

    foreach (const QFileInfo &drive, QDir::drives())
    {
      hiddenDirs += QDir(drive.absoluteFilePath()).absoluteFilePath("lost+found");

#if defined(Q_OS_WIN)
      hiddenDirs += QDir(drive.absoluteFilePath()).absoluteFilePath("RECYCLER");
      hiddenDirs += QDir(drive.absoluteFilePath()).absoluteFilePath("System Volume Information");
#endif
    }
  }

  const QFileInfo info(path);

  QString absoluteFilePath = info.absolutePath();
  if (!absoluteFilePath.endsWith('/')) absoluteFilePath += '/';

  QString canonicalFilePath = (info.exists() ? info.canonicalFilePath() : info.absolutePath());
  if (!canonicalFilePath.endsWith('/')) canonicalFilePath += '/';

  foreach (const QString &hidden, hiddenDirs)
  {
    const QString path = hidden.endsWith('/') ? hidden : (hidden + '/');
    if (absoluteFilePath.startsWith(path, caseSensitivity) || canonicalFilePath.startsWith(path, caseSensitivity))
      return true;
  }

  return false;
}

void MediaPlayerServer::setRootPaths(const QStringList &paths)
{
  struct T
  {
    static QString albumName(const QString &path, int recurse)
    {
      QStringList dirs = path.split('/', QString::SkipEmptyParts);

      QString result;
      for (int r=0; (r<recurse) && !dirs.isEmpty(); r++)
        result = dirs.takeLast() + (result.isEmpty() ? "" : "_") + result;

      return result;
    }
  };

  PluginSettings settings(Module::pluginName);

  settings.setValue("RootPaths", paths);

  rootPaths.clear();
  foreach (QString path, paths)
  {
    if (!path.endsWith('/'))
      path += '/';

    const QString album = T::albumName(path, 1);
    rootPaths.insert(album, path);
  }
}

QString MediaPlayerServer::virtualPath(const QString &realPath) const
{
  for (QMap<QString, QString>::ConstIterator i=rootPaths.begin(); i!=rootPaths.end(); i++)
  if (realPath.startsWith(i.value(), caseSensitivity))
    return serverPath() + i.key() + '/' + realPath.mid(i.value().length());

  return QString::null;
}

QString MediaPlayerServer::realPath(const QString &virtualPath) const
{
  const QString basepath = serverPath();

  if (virtualPath.startsWith(basepath))
  {
    const int s = virtualPath.indexOf('/', basepath.length());
    if (s >= 1)
    {
      QMap<QString, QString>::ConstIterator i = rootPaths.find(virtualPath.mid(basepath.length(), s - basepath.length()));
      if (i != rootPaths.end())
        return i.value() + virtualPath.mid(s + 1);
    }
  }

  return QString::null;
}

int MediaPlayerServer::countAlbums(const QString &virtualPath)
{
  return mediaDatabase->countAlbums(realPath(virtualPath));
}

QList<MediaPlayerServer::Item> MediaPlayerServer::listAlbums(const QString &virtualPath, unsigned &start, unsigned &count)
{
  QList<Item> result;

  const QString path = realPath(virtualPath);
  foreach (const QString &album, mediaDatabase->getAlbums(path, start, count))
  {
    Item item;
    item.isDir = true;
    item.title = album;
    item.path = virtualPath + album + '/';
    item.iconUrl = "/img/folder-video.png";

    result += item;
    if (count > 0)
      count--;
  }

  start = unsigned(qMax(0, int(start) - mediaDatabase->countAlbums(path)));

  return result;
}

MediaPlayerServer::Item MediaPlayerServer::makeItem(const FileNode &node)
{
  Item item;

  if (!node.isNull())
  {
    if (!node.audioStreams().isEmpty() && !node.videoStreams().isEmpty())
    {
      item.type = Item::Type_Video;
    }
    else if (!node.audioStreams().isEmpty())
    {
      if (node.duration().toMin() <= maxSongDurationMin)
        item.type = Item::Type_Music;
      else
        item.type = Item::Type_Audio;
    }
    else if (!node.imageCodec().isNull())
    {
      item.type = Item::Type_Image;
    }
    else
      item.type = Item::Type_None;
  
    if (item.type != Item::Type_None)
    {
      item.played = mediaDatabase->lastPlayed(node.filePath()).isValid();
      item.path = virtualPath(node.filePath());
      item.url = item.path;
      item.iconUrl = item.url;
      item.iconUrl.addQueryItem("thumbnail", "128x128");

      item.title = node.metadata("title").toString();
      if (item.title.isEmpty())
        item.title = node.fileName();

      item.artist = node.metadata("author").toString();
      item.album = node.metadata("album").toString();
      item.track = node.metadata("track").toInt();

      item.duration = node.duration().toSec();

      for (int a=0, an=node.audioStreams().count(); a < an; a++)
      for (int d=0, dn=node.dataStreams().count(); d <= dn; d++)
      {
        Item::Stream stream;

        stream.title = QString::number(a + 1) + ". " + node.audioStreams()[a].fullName();
        stream.queryItems += qMakePair(QString("language"), node.audioStreams()[a].toString());

        if (d < dn)
        {
          stream.title += ", " + QString::number(d + 1) + ". " + node.dataStreams()[d].fullName() + " " + tr("subtitles");
          stream.queryItems += qMakePair(QString("subtitles"), node.dataStreams()[d].toString());
        }
        else
          stream.queryItems += qMakePair(QString("subtitles"), QString());

        item.streams += stream;
      }

      foreach (const SMediaInfo::Chapter &chapter, node.chapters())
        item.chapters += Item::Chapter(chapter.title, chapter.begin.toSec());

      if (!node.audioStreams().isEmpty())
      {
        const SAudioCodec &codec = node.audioStreams().first().codec;
        item.audioFormat.setChannelSetup(codec.channelSetup());
        item.audioFormat.setSampleRate(codec.sampleRate());
      }

      if (!node.videoStreams().isEmpty())
      {
        const SVideoCodec &codec = node.videoStreams().first().codec;
        item.videoFormat.setSize(codec.size());
        item.videoFormat.setFrameRate(codec.frameRate());
      }

      if (!node.imageCodec().isNull())
        item.imageSize = node.imageCodec().size();
    }
    else
    {
      item.path = virtualPath(node.filePath());
      item.url = "";
      item.iconUrl = "/img/misc.png";
      item.title = node.fileName();
    }
  }

  return item;
}

MediaPlayerServer::Item MediaPlayerServer::makePlayAllItem(const QString &virtualPath)
{
  const QString path = realPath(virtualPath);
  const int numItems = mediaDatabase->countAlbumFiles(path);

  Item item;
  item.isDir = false;
  item.path = virtualPath + ".all";
  item.url = item.path;
  item.iconUrl = "/img/arrow-right.png";

  item.type = Item::Type_Video;
  item.title = tr("Play all");

  int audio = 0, video = 0, image = 0;
  foreach (const FileNode &node, mediaDatabase->getAlbumFiles(path, qMax(0, (numItems / 2) - 8), 16))
  if (!node.audioStreams().isEmpty() && !node.videoStreams().isEmpty())
    video++;
  else if (!node.audioStreams().isEmpty())
    audio++;
  else if (!node.imageCodec().isNull())
    image++;

  if ((audio > video) && (audio > image))
  {
    item.type = Item::Type_AudioBroadcast;
    item.title = tr("Play all");
  }
  else if ((video > audio) && (video > image))
  {
    item.type = Item::Type_VideoBroadcast;
    item.title = tr("Play all");
  }
  else if ((image > audio) && (image > video))
  {
    item.type = Item::Type_VideoBroadcast;
    item.title = tr("Slideshow");
  }

  return item;
}

void MediaPlayerServer::consoleLine(const QString &line)
{
  if (line.startsWith("#PLAYED:"))
    mediaDatabase->setLastPlayed(QString::fromUtf8(QByteArray::fromHex(line.mid(8).toAscii())));
}


MediaPlayerServer::Stream::Stream(MediaPlayerServer *parent, SSandboxClient *sandbox, const QString &url)
  : MediaServer::Stream(parent, url),
    sandbox(sandbox)
{
}

MediaPlayerServer::Stream::~Stream()
{
  disconnect(sandbox, SIGNAL(consoleLine(QString)), parent, SLOT(consoleLine(QString)));
  static_cast<MediaPlayerServer *>(parent)->masterServer->recycleSandbox(sandbox);
}

bool MediaPlayerServer::Stream::setup(const QUrl &url, const QByteArray &content)
{
  SHttpEngine::RequestMessage message(sandbox);
  message.setRequest("POST", url.toEncoded(QUrl::RemoveScheme | QUrl::RemoveAuthority));
  message.setContent(content);

  sandbox->openRequest(message, &proxy, SLOT(setSource(QIODevice *)));

  return true;
}

} } // End of namespaces
