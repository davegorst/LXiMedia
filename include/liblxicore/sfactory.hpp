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

#include "sfactory.h"

namespace LXiCore {

template <class _interface>
    _interface * SFactorizable<_interface>::create(QObject *parent, const QString &scheme, bool nonNull)
{
  return static_cast<_interface *>(factory().createObject(_interface::staticMetaObject.className(), parent, scheme, nonNull));
}

template <class _interface>
QStringList SFactorizable<_interface>::available(void)
{
  QStringList result;
  foreach (const SFactory::Scheme &scheme, factory().registredSchemes(_interface::staticMetaObject.className()))
    result += scheme.name();

  return result;
}

template <class _interface>
SFactory & SFactorizable<_interface>::factory(void)
{
  static SFactory f;

  return f;
}

} // End of namespace