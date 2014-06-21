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

#include "module.h"
#include "alsainput.h"
#include "alsamixer.h"
#include "alsaoutput.h"

namespace LXiStreamDevice {
namespace AlsaBackend {

bool Module::registerClasses(void)
{
  int result = false;
  int card = -1;
  if (snd_card_next(&card) >= 0)
  while (card >= 0) // Iterate through all the cards
  {
    char *name = NULL;

    if (snd_card_get_name(card, &name) >= 0)
    {
      QString dev = "hw:" + QString::number(card);
      snd_ctl_t *ctl = NULL;
      if (snd_ctl_open(&ctl, dev.toLatin1().data(), 0) == 0)
      {
        snd_ctl_card_info_t *info = NULL;

        if (snd_ctl_card_info_malloc(&info) == 0)
        if (snd_ctl_card_info(ctl, info) == 0)
        {
          const QString longName = snd_ctl_card_info_get_longname(info);

          char **hints = NULL;
          if (snd_device_name_hint(card, "pcm", reinterpret_cast<void ***>(&hints)) == 0)
          {
            for (unsigned i=0; hints[i]!=NULL; i++)
            {
              QString name(hints[i]);
              const bool inputOnly = name.contains("IOIDInput");
              {
                const int no = name.indexOf("NAME");
                const int co = name.indexOf("CARD");
                if ((no >= 0) && (co > no))
                  name = name.mid(no + 4, co - (no + 4));
                else
                  continue;
              }

              if ((name == "default:") || (name == "sysdefault:")) // Analog output
              {
                const QString dev = longName + ", hw:" + QString::number(card);

                if (!inputOnly)
                  AlsaOutput::registerClass<AlsaOutput>(SFactory::Scheme(-card, dev));

                AlsaMixer mixer(dev);
                if (mixer.open())
                {
                  foreach (const QString &channel, mixer.listInputChannels())
                    AlsaInput::registerClass<AlsaInput>(SFactory::Scheme(-card, dev + '@' + channel));
                }
                else
                  AlsaInput::registerClass<AlsaInput>(SFactory::Scheme(-card, dev));

                result = true;
              }
              else if (name == "iec958:") // Iec958 (S/PDIF) output
              {
                const QString dev = longName + ", iec958:" + QString::number(card);

                if (!inputOnly)
                  AlsaOutput::registerClass<AlsaOutput>(SFactory::Scheme(-card, dev));

                AlsaInput::registerClass<AlsaInput>(SFactory::Scheme(-card, dev));

                result = true;
              }
            }
          }

          snd_device_name_free_hint(reinterpret_cast<void **>(hints));
        }

        if (info)
          snd_ctl_card_info_free(info);

        snd_ctl_close(ctl);
      }

      free(name);
    }

    // Next card
    if (snd_card_next(&card) < 0)
      break;
  }

  return result;
}

void Module::unload(void)
{
}

QByteArray Module::about(void)
{
  return "ALSA plugin by A.J. Admiraal";
}

QByteArray Module::licenses(void)
{
  return QByteArray();
}


QString deviceName(const QString &dev)
{
  const QStringList l = dev.mid(dev.lastIndexOf(", ") + 2).split('@');
  if (!l.isEmpty())
    return l.first();

  return QString::null;
}

QString channelName(const QString &dev)
{
  const QStringList l = dev.mid(dev.lastIndexOf(", ") + 2).split('@');
  if (l.count() > 1)
    return l.last();

  return QString::null;
}

} } // End of namespaces
