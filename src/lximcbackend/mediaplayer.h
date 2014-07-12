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

#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include "messageloop.h"
#include "pupnp/content_directory.h"
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

class mediaplayer : private pupnp::content_directory::item_source
{
public:
  mediaplayer(class messageloop &, pupnp::content_directory &);
  virtual ~mediaplayer();

protected: // From content_directory::item_source
  virtual std::vector<pupnp::content_directory::item> list_contentdir_items(const std::string &client, const std::string &path, size_t start, size_t &count) override;
  virtual pupnp::content_directory::item get_contentdir_item(const std::string &client, const std::string &path) override;

private:
  std::string to_system_path(const std::string &) const;
  std::string to_virtual_path(const std::string &) const;

  static std::vector<std::string> list_files(const std::string &path);

private:
  class messageloop &messageloop;
  class pupnp::content_directory &content_directory;
  const std::string root_path;

  std::vector<std::string> root_paths;
};

#endif
