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

#ifndef LINUXDVBBACKEND_MODULE_H
#define LINUXDVBBACKEND_MODULE_H

#include <LXiStream>

namespace LXiStream {
namespace LinuxDvbBackend {


class Module : public QObject,
               public SModule
{
Q_OBJECT
Q_INTERFACES(LXiStream::SModule)
public:
  virtual void                  registerClasses(void);
  virtual void                  unload(void);
};


} } // End of namespaces

#endif
