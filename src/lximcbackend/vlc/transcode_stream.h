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

#ifndef VLC_TRANSCODE_STREAM_H
#define VLC_TRANSCODE_STREAM_H

#include <istream>
#include <memory>
#include <string>

class messageloop;

namespace vlc {

class instance;

class transcode_stream : public std::istream
{
public:
  transcode_stream(class messageloop &, class instance &);
  ~transcode_stream();

  bool open(const std::string &mrl, const std::string &transcode, const std::string &mux);
  bool attach(transcode_stream &);
  void close();

private:
  class streambuf;
  class source;

  static const size_t block_size;
  static const size_t block_count;
  class messageloop &messageloop;
  class instance &instance;
  std::unique_ptr<class streambuf> streambuf;
  std::shared_ptr<class source> source;
};

} // End of namespace

#endif