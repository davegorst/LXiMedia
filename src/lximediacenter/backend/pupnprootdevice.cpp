/******************************************************************************
 *   Copyright (C) 2012  A.J. Admiraal                                        *
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

#include <upnp/upnp.h>
#include "pupnprootdevice.h"
#include "ixmlstructures.h"

struct PupnpRootDevice::Data
{
  class Event : public QEvent
  {
  public:
    static const QEvent::Type myType;
    explicit Event(Functor &f, QSemaphore *sem = NULL) : QEvent(myType), f(f), sem(sem) { }
    virtual ~Event() { if (sem) sem->release(); else delete &f; }

    Functor &f;

  private:
    QSemaphore * const sem;
  };

  static const char baseDir[];
  static const char deviceDescriptionFile[];
  static const char serviceDescriptionFile[];
  static const char serviceControlFile[];
  static const char serviceEventFile[];

  static PupnpRootDevice * me;

  quint16 port;
  QString deviceName;
  bool bindPublicInterfaces;

  QSet<QByteArray> availableAddresses;
  bool initialized;
  QMap<QString, RootDevice::HttpCallback *> httpCallbacks;
  QSet<QByteArray> serviceExts;
  QMap<QByteArray, QPair<Service *, QByteArray> > services;

  QMutex responsesMutex;
  QMap<QByteArray, QPair<QIODevice *, QByteArray> > responses;
  static const int clearResponsesInterval = 15000;
  QTimer clearResponsesTimer;

  bool rootDeviceRegistred;
  QMap<QByteArray, ::UpnpDevice_Handle> rootDeviceHandle;
  static const int advertisementExpiration = 1800;

  QSet<QObject *> pendingFiles;
  static const int updateInterfacesInterval = 10000;
  QTimer updateInterfacesTimer;
};

const char          PupnpRootDevice::Data::baseDir[] = "/upnp";
const char          PupnpRootDevice::Data::deviceDescriptionFile[] = "/upnp/device";
const char          PupnpRootDevice::Data::serviceDescriptionFile[] = "/upnp/service-";
const char          PupnpRootDevice::Data::serviceControlFile[] = "/upnp/control-";
const char          PupnpRootDevice::Data::serviceEventFile[] = "/upnp/event-";

PupnpRootDevice   * PupnpRootDevice::Data::me = NULL;
const QEvent::Type  PupnpRootDevice::Data::Event::myType = QEvent::Type(QEvent::registerEventType());

PupnpRootDevice::PupnpRootDevice(const QUuid &uuid, const QByteArray &deviceType, QObject *parent)
  : RootDevice(uuid, deviceType, parent),
    d(new Data())
{
  Q_ASSERT(d->me == NULL);

  d->me = this;
  d->port = 0;
  d->bindPublicInterfaces = false;
  d->initialized = false;
  d->rootDeviceRegistred = false;

  connect(&d->clearResponsesTimer, SIGNAL(timeout()), SLOT(clearResponses()));
  d->clearResponsesTimer.setSingleShot(true);

  connect(&d->updateInterfacesTimer, SIGNAL(timeout()), SLOT(updateInterfaces()));

  sApp->addLicense(
      " <h3>Portable SDK for UPnP Devices</h3>\n"
      " <p>Website: <a href=\"http://pupnp.sourceforge.net/\">pupnp.sourceforge.net</a></p>\n"
      " <p>Copyright &copy; 2000-2003 Intel Corporation. All rights reserved.</p>\n"
      " <p>Copyright &copy; 2011-2012 France Telecom. All rights reserved.</p>\n"
      " <p>Used under the terms of the BSD License.</p>\n");
}

PupnpRootDevice::~PupnpRootDevice()
{
  d->httpCallbacks.clear();

  cleanup();

  Q_ASSERT(d->me != NULL);
  d->me = NULL;

  delete d;
  *const_cast<Data **>(&d) = NULL;
}

void PupnpRootDevice::registerService(const QByteArray &serviceId, Service *service)
{
  QByteArray ext;
  for (int lc = qMax(serviceId.lastIndexOf(':'), serviceId.lastIndexOf('_')); (lc >= 0) && (lc < serviceId.size()); lc++)
  if ((serviceId[lc] >= 'A') && (serviceId[lc] <= 'Z'))
    ext += (serviceId[lc] + ('a' - 'A'));

  static const char id[] = "abcdefghijklmnopqrstuvwxyz";
  int n = 0;
  while (id[n] && (d->serviceExts.find(ext + QByteArray::number(n)) != d->serviceExts.end()))
    n++;

  if (id[n])
    d->services.insert(serviceId, qMakePair(service, ext + id[n]));

  RootDevice::registerService(serviceId, service);
}

void PupnpRootDevice::unregisterService(const QByteArray &serviceId)
{
  d->services.remove(serviceId);

  RootDevice::unregisterService(serviceId);
}

void PupnpRootDevice::registerHttpCallback(const QString &path, RootDevice::HttpCallback *callback)
{
  RootDevice::registerHttpCallback(path, callback);

  QString p = path;
  if (!p.endsWith('/'))
    p += '/';

  if (!d->httpCallbacks.contains(p))
  {
    d->httpCallbacks.insert(p, callback);
    if (d->initialized)
      ::UpnpAddVirtualDir(p.toUtf8());
  }
}

void PupnpRootDevice::unregisterHttpCallback(RootDevice::HttpCallback *callback)
{
  RootDevice::unregisterHttpCallback(callback);

  for (QMap<QString, RootDevice::HttpCallback *>::Iterator i = d->httpCallbacks.begin();
       i != d->httpCallbacks.end();)
  {
    if (*i == callback)
    {
      if (d->initialized)
        ::UpnpRemoveVirtualDir(i.key().toUtf8());

      i = d->httpCallbacks.erase(i);
    }
    else
      i++;
  }
}

bool PupnpRootDevice::isLocalAddress(const char *address)
{
  return
      (qstrncmp(address, "10.", 3) == 0) ||
      (qstrncmp(address, "127.", 4) == 0) ||
      (qstrncmp(address, "169.254.", 8) == 0) ||
      (qstrncmp(address, "172.16.", 7) == 0) ||
      (qstrncmp(address, "172.17.", 7) == 0) ||
      (qstrncmp(address, "172.18.", 7) == 0) ||
      (qstrncmp(address, "172.19.", 7) == 0) ||
      (qstrncmp(address, "172.20.", 7) == 0) ||
      (qstrncmp(address, "172.21.", 7) == 0) ||
      (qstrncmp(address, "172.22.", 7) == 0) ||
      (qstrncmp(address, "172.23.", 7) == 0) ||
      (qstrncmp(address, "172.24.", 7) == 0) ||
      (qstrncmp(address, "172.25.", 7) == 0) ||
      (qstrncmp(address, "172.26.", 7) == 0) ||
      (qstrncmp(address, "172.27.", 7) == 0) ||
      (qstrncmp(address, "172.28.", 7) == 0) ||
      (qstrncmp(address, "172.29.", 7) == 0) ||
      (qstrncmp(address, "172.30.", 7) == 0) ||
      (qstrncmp(address, "172.31.", 7) == 0) ||
      (qstrncmp(address, "192.168.", 8) == 0);
}

void PupnpRootDevice::initialize(quint16 port, const QString &deviceName, bool bindPublicInterfaces)
{
  d->port = port;
  d->deviceName = deviceName;
  d->bindPublicInterfaces = bindPublicInterfaces;

  QVector<const char *> addresses;
  for (char **i = ::UpnpGetAvailableIpAddresses(); i && *i; i++)
  {
    d->availableAddresses.insert(*i);
    if (bindPublicInterfaces || isLocalAddress(*i))
      addresses.append(*i);
  }

  addresses.append(NULL);
  d->initialized = ::UpnpInit3(&(addresses[0]), port) == UPNP_E_SUCCESS;
  if (!d->initialized)
    d->initialized = ::UpnpInit3(&(addresses[0]), 0) == UPNP_E_SUCCESS;

  if (d->initialized)
  {
    for (char **i = ::UpnpGetServerIpAddresses(); i && *i; i++)
      qDebug() << "Bound " << *i << ":" << ::UpnpGetServerPort();

    enableWebserver();

    for (QMap<QString, RootDevice::HttpCallback *>::ConstIterator i = d->httpCallbacks.begin();
         i != d->httpCallbacks.end();
         i++)
    {
      ::UpnpAddVirtualDir(i.key().toUtf8());
    }

    registerHttpCallback(d->baseDir, this);

    RootDevice::initialize(port, deviceName, bindPublicInterfaces);

    enableRootDevice();
  }

  d->updateInterfacesTimer.start(d->updateInterfacesInterval);
}

void PupnpRootDevice::close(void)
{
  d->updateInterfacesTimer.stop();

  unregisterHttpCallback(this);

  cleanup();

  RootDevice::close();
}

void PupnpRootDevice::cleanup(void)
{
  if (d->initialized)
  {
    d->initialized = false;

    if (d->rootDeviceRegistred)
    {
      d->rootDeviceRegistred = false;
      foreach(const ::UpnpDevice_Handle &handle, d->rootDeviceHandle)
        ::UpnpUnRegisterRootDevice(handle);
    }

    ::UpnpRemoveAllVirtualDirs();

    struct T : QThread
    {
      virtual void run() { ::UpnpFinish(); }
    } t;

    // Ugly, but needed as UpnpFinish waits for callbacks from the HTTP server.
    t.start();
    while (!t.isFinished()) { qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::WaitForMoreEvents, 16); }
    t.wait();

    clearResponses();
  }
}

void PupnpRootDevice::emitEvent(const QByteArray &serviceId)
{
  if (d->services.contains(serviceId))
  {
    IXMLStructures::EventablePropertySet propertyset;
    handleEvent(serviceId, propertyset);

    foreach(const ::UpnpDevice_Handle &handle, d->rootDeviceHandle)
    {
      ::UpnpNotifyExt(
            handle,
            udn(),
            serviceId,
            propertyset.doc);
    }
  }
}

void PupnpRootDevice::customEvent(QEvent *e)
{
  if (e->type() == Data::Event::myType)
    static_cast<Data::Event *>(e)->f();
  else
    RootDevice::customEvent(e);
}

HttpStatus PupnpRootDevice::httpRequest(const QUrl &request, QByteArray &contentType, QIODevice *&response)
{
  QString host = request.host();
  if (request.port() > 0)
    host += ':' + QString::number(request.port());

  if (request.path().startsWith(d->deviceDescriptionFile))
  {
    IXMLStructures::DeviceDescription desc(host);
    writeDeviceDescription(desc);

    for (QMap<QByteArray, QPair<Service *, QByteArray> >::Iterator i = d->services.begin();
         i != d->services.end();
         i++)
    {
      desc.addService(
            i->first->serviceType(),
            i.key(),
            d->serviceDescriptionFile + i->second + ".xml",
            d->serviceControlFile + i->second,
            d->serviceEventFile + i->second);
    }

    QBuffer * const buffer = new QBuffer();
    DOMString s = ixmlDocumenttoString(desc.doc);
    buffer->setData(s);
    ixmlFreeDOMString(s);
    contentType = mimeTextXml;
    response = buffer;

    return HttpStatus_Ok;
  }
  else if (request.path().startsWith(d->serviceDescriptionFile))
  {
    const uint l = qstrlen(d->serviceDescriptionFile);
    const QByteArray path = request.path().toUtf8();
    const QByteArray ext = path.mid(l, path.length() - 4 - l);

    for (QMap<QByteArray, QPair<Service *, QByteArray> >::Iterator i = d->services.begin();
         i != d->services.end();
         i++)
    {
      if (i->second == ext)
      {
        IXMLStructures::ServiceDescription desc;
        i->first->writeServiceDescription(desc);

        QBuffer * const buffer = new QBuffer();
        DOMString s = ixmlDocumenttoString(desc.doc);
        buffer->setData(s);
        ixmlFreeDOMString(s);
        contentType = mimeTextXml;
        response = buffer;

        return HttpStatus_Ok;
      }
    }
  }

  return HttpStatus_NotFound;
}

void PupnpRootDevice::clearResponses()
{
  QMutexLocker l(&d->responsesMutex);

  for (QMap<QByteArray, QPair<QIODevice *, QByteArray> >::Iterator i = d->responses.begin();
       i != d->responses.end();
       i = d->responses.erase(i))
  {
    i->first->deleteLater();
  }
}

void PupnpRootDevice::closedFile(QObject *file)
{
  d->pendingFiles.remove(file);
  if (d->pendingFiles.isEmpty())
    d->updateInterfacesTimer.start(d->updateInterfacesInterval);
}

void PupnpRootDevice::updateInterfaces()
{
  for (char **i = ::UpnpGetAvailableIpAddresses(); i && *i; i++)
  if (d->availableAddresses.find(*i) == d->availableAddresses.end())
  if (d->bindPublicInterfaces || isLocalAddress(*i))
  {
    close();
    initialize(d->port, d->deviceName, d->bindPublicInterfaces);
    break;
  }
}

void PupnpRootDevice::send(Functor &f) const
{
  QSemaphore sem;
  qApp->postEvent(Data::me, new Data::Event(f, &sem));
  sem.acquire();
}

void PupnpRootDevice::enableWebserver()
{
  struct T
  {
    static int get_info(const char *host, const char *filename, ::File_Info *info)
    {
      QByteArray contentType;
      QIODevice *response = NULL;
      if (Data::me->getResponse(host, filename, contentType, response, false) == HttpStatus_Ok)
      {
        info->file_length = (response && !response->isSequential()) ? response->size() : -1;
        info->last_modified = QDateTime::currentDateTime().toTime_t();
        info->is_directory = FALSE;
        info->is_readable = TRUE;
        info->content_type = ::ixmlCloneDOMString(contentType);
        return 0;
      }
      else
        return -1;
    }

    static ::UpnpWebFileHandle open(const char *host, const char *filename, ::UpnpOpenFileMode mode)
    {
      QByteArray contentType;
      QIODevice *response = NULL;
      if (Data::me->getResponse(host, filename, contentType, response, true) == HttpStatus_Ok)
      {
        if (response->open((mode == UPNP_READ) ? QIODevice::ReadOnly : QIODevice::WriteOnly))
        {
          return response;
        }
        else
          response->deleteLater();
      }

      return NULL;
    }

    static int read(::UpnpWebFileHandle fileHnd, char *buf, size_t len)
    {
      if (fileHnd)
        return qMax(int(reinterpret_cast<QIODevice *>(fileHnd)->read(buf, len)), 0);

      return UPNP_E_INVALID_HANDLE;
    }

    static int write(::UpnpWebFileHandle fileHnd, char *buf, size_t len)
    {
      if (fileHnd)
        return reinterpret_cast<QIODevice *>(fileHnd)->write(buf, len);

      return UPNP_E_INVALID_HANDLE;
    }

    static int seek(::UpnpWebFileHandle fileHnd, off_t offset, int origin)
    {
      if (fileHnd)
      {
        QIODevice * const ioDevice = reinterpret_cast<QIODevice *>(fileHnd);
        switch (origin)
        {
        case SEEK_SET:  return ioDevice->seek(offset) ? 0 : -1;
        case SEEK_CUR:  return ioDevice->seek(ioDevice->pos() + offset) ? 0 : -1;
        case SEEK_END:  return ioDevice->seek(ioDevice->size() + offset) ? 0 : -1;
        default:        return -1;
        }
      }

      return -1;
    }

    static int close(::UpnpWebFileHandle fileHnd)
    {
      if (fileHnd)
      {
        QIODevice * const ioDevice = reinterpret_cast<QIODevice *>(fileHnd);
        ioDevice->close();
        ioDevice->deleteLater();
      }

      return -1;
    }
  };

  ::UpnpEnableWebserver(TRUE);
  static const ::UpnpVirtualDirCallbacks callbacks = { &T::get_info, &T::open, &T::read, &T::write, &T::seek, &T::close };
  ::UpnpSetVirtualDirCallbacks(const_cast< ::UpnpVirtualDirCallbacks * >(&callbacks));
}

HttpStatus PupnpRootDevice::getResponse(const QByteArray &host, const QByteArray &path, QByteArray &contentType, QIODevice *&response, bool erase)
{
  QMutexLocker l(&d->responsesMutex);

  QMap<QByteArray, QPair<QIODevice *, QByteArray> >::Iterator i = d->responses.find(path);
  if (i != d->responses.end())
  {
    response = i->first;
    contentType = i->second;
    if (erase)
      d->responses.erase(i);

    return HttpStatus_Ok;
  }
  else
  {
    l.unlock();

    struct F : Functor
    {
      F(PupnpRootDevice *me, bool erase, const QUrl &url) : me(me), erase(erase), url(url), response(NULL), result(HttpStatus_InternalServerError) { }

      void operator()()
      {
        if (me->d->initialized)
        {
          result = me->handleHttpRequest(url, contentType, response);
          if (response)
          {
            connect(response, SIGNAL(destroyed(QObject*)), me, SLOT(closedFile(QObject*)));
            if (me->d->pendingFiles.isEmpty())
              me->d->updateInterfacesTimer.stop();

            me->d->pendingFiles.insert(response);

            if (!erase && (result == HttpStatus_Ok))
              me->d->clearResponsesTimer.start(me->d->clearResponsesInterval);
          }
        }
        else
          result = HttpStatus_InternalServerError;
      }

      PupnpRootDevice * const me;
      const bool erase;
      const QUrl url;
      QByteArray contentType;
      QIODevice *response;
      HttpStatus result;
    } f(this, erase, QUrl("http://" + host + QString::fromUtf8(path)));

    send(f);

    if (!erase && (f.result == HttpStatus_Ok))
    {
      l.relock();

      d->responses[path] = qMakePair(f.response, f.contentType);
    }

    contentType = f.contentType;
    response = f.response;
    return f.result;
  }
}

static void splitName(const QByteArray &fullName, QByteArray &prefix, QByteArray &name)
{
  const int colon = fullName.indexOf(':');
  if (colon >= 0)
  {
    prefix = fullName.left(colon);
    name = fullName.mid(colon + 1);
  }
  else
    name = fullName;
}

void PupnpRootDevice::enableRootDevice(void)
{
  struct T : QThread
  {
    static int callback(Upnp_EventType eventType, void *event, void *cookie)
    {
      PupnpRootDevice * const me = reinterpret_cast<PupnpRootDevice *>(cookie);

      if (eventType == UPNP_CONTROL_ACTION_REQUEST)
      {
        struct F : Functor
        {
          F(PupnpRootDevice *me, Upnp_Action_Request *request) : me(me), request(request) { }

          void operator()()
          {
            QMap<QByteArray, QPair<Service *, QByteArray> >::Iterator i = me->d->services.find(request->ServiceID);
            if ((i != me->d->services.end()) && me->d->initialized)
            {
              if ((strcmp(i->first->serviceType(), serviceTypeConnectionManager) == 0))
              {
                ConnectionManager * const service = static_cast<ConnectionManager *>(i->first);

                IXML_NodeList * const children = ixmlNode_getChildNodes(&request->ActionRequest->n);
                for (IXML_NodeList *i = children; i; i = i->next)
                {
                  QByteArray prefix = "ns0", name;
                  splitName(i->nodeItem->nodeName, prefix, name);

                  if (name == "GetCurrentConnectionIDs")
                  {
                    IXMLStructures::ActionGetCurrentConnectionIDs action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                  else if (name == "GetCurrentConnectionInfo")
                  {
                    IXMLStructures::ActionGetCurrentConnectionInfo action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                  else if (name == "GetProtocolInfo")
                  {
                    IXMLStructures::ActionGetProtocolInfo action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                }

                ixmlNodeList_free(children);
              }
              else if ((strcmp(i->first->serviceType(), serviceTypeContentDirectory) == 0))
              {
                ContentDirectory * const service = static_cast<ContentDirectory *>(i->first);

                IXML_NodeList * const children = ixmlNode_getChildNodes(&request->ActionRequest->n);
                for (IXML_NodeList *i = children; i; i = i->next)
                {
                  QByteArray prefix = "ns0", name;
                  splitName(i->nodeItem->nodeName, prefix, name);

                  if (name == "Browse")
                  {
                    IXMLStructures::ActionBrowse action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                  else if (name == "Search")
                  {
                    IXMLStructures::ActionSearch action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                  else if (name == "GetSearchCapabilities")
                  {
                    IXMLStructures::ActionGetSearchCapabilities action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                  else if (name == "GetSortCapabilities")
                  {
                    IXMLStructures::ActionGetSortCapabilities action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                  else if (name == "GetSystemUpdateID")
                  {
                    IXMLStructures::ActionGetSystemUpdateID action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                  else if (name == "X_GetFeatureList")
                  {
                    IXMLStructures::ActionGetFeatureList action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                }

                ixmlNodeList_free(children);
              }
              else if ((strcmp(i->first->serviceType(), serviceTypeMediaReceiverRegistrar) == 0))
              {
                MediaReceiverRegistrar * const service = static_cast<MediaReceiverRegistrar *>(i->first);

                IXML_NodeList * const children = ixmlNode_getChildNodes(&request->ActionRequest->n);
                for (IXML_NodeList *i = children; i; i = i->next)
                {
                  QByteArray prefix = "ns0", name;
                  splitName(i->nodeItem->nodeName, prefix, name);

                  if (name == "IsAuthorized")
                  {
                    IXMLStructures::ActionIsAuthorized action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                  else if (name == "IsValidated")
                  {
                    IXMLStructures::ActionIsValidated action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                  else if (name == "RegisterDevice")
                  {
                    IXMLStructures::ActionRegisterDevice action(i->nodeItem, request->ActionResult, prefix);
                    service->handleAction(request->Host, action);
                  }
                }

                ixmlNodeList_free(children);
              }
            }
          }

          PupnpRootDevice * const me;
          Upnp_Action_Request * const request;
        } f(me, reinterpret_cast<Upnp_Action_Request *>(event));
        me->send(f);

        return 0;
      }
      else if (eventType == UPNP_EVENT_SUBSCRIPTION_REQUEST)
      {
        struct F : Functor
        {
          F(PupnpRootDevice *me, Upnp_Subscription_Request *request) : me(me), request(request) { }

          void operator()()
          {
            if (me->d->initialized)
            {
              udn = me->udn();
              me->handleEvent(request->ServiceId, propertyset);

              QMap<QByteArray, ::UpnpDevice_Handle>::Iterator i = me->d->rootDeviceHandle.find(request->Host);
              if (i != me->d->rootDeviceHandle.end())
                ::UpnpAcceptSubscriptionExt(*i, udn, request->ServiceId, propertyset.doc, request->Sid);
            }
          }

          PupnpRootDevice * const me;
          Upnp_Subscription_Request * const request;
          IXMLStructures::EventablePropertySet propertyset;
          QByteArray udn;
        } f(me, reinterpret_cast<Upnp_Subscription_Request *>(event));
        me->send(f);

        return 0;
      }

      qDebug() << "enableRootDevice::callback Unsupported eventType" << eventType;
      return -1;
    }

    T(PupnpRootDevice *me, const QByteArray &path) : me(me), path(path), result(UPNP_E_INTERNAL_ERROR) { }

    virtual void run()
    {
      for (char **i = ::UpnpGetServerIpAddresses(); i && *i; i++)
      {
        const QByteArray host = QByteArray(*i) + ":" + QByteArray::number(::UpnpGetServerPort());
        if (::UpnpRegisterRootDevice(
              "http://" + host + path,
              &T::callback, me,
              &(me->d->rootDeviceHandle[host])) == UPNP_E_SUCCESS)
        {
          result = UPNP_E_SUCCESS;
        }
      }
    }

    PupnpRootDevice * const me;
    const QByteArray path;
    volatile int result;
  } t(this, QByteArray(d->deviceDescriptionFile) + ".xml");

  // Ugly, but needed as UpnpRegisterRootDevice retrieves files from the HTTP server.
  t.start();
  while (!t.isFinished()) { qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::WaitForMoreEvents, 16); }
  t.wait();

  if (t.result == UPNP_E_SUCCESS)
  {
    d->rootDeviceRegistred = true;

    foreach(const ::UpnpDevice_Handle &handle, d->rootDeviceHandle)
      ::UpnpSendAdvertisement(handle, d->advertisementExpiration);
  }
  else
    qWarning() << "UpnpRegisterRootDevice" << t.path << "failed:" << t.result;
}