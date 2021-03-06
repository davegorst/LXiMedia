/******************************************************************************
 *   Copyright (C) 2015  A.J. Admiraal                                        *
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

#ifndef PLATFORM_PATH_H
#define PLATFORM_PATH_H

#include <string>
#include <vector>

namespace platform {

std::string clean_path(const std::string &);
std::string mrl_from_path(const std::string &);
std::string path_from_mrl(const std::string &);

std::vector<std::string> list_root_directories();

enum class file_filter { all, directories, large_files };
std::vector<std::string> list_files(
        const std::string &path,
        file_filter filter = file_filter::large_files,
        size_t max_count = size_t(-1));

std::vector<std::string> list_removable_media();

std::string file_date(const std::string &path);

std::string temp_file_path(const std::string &suffix);
void remove_file(const std::string &);
void rename_file(const std::string &old_path, const std::string &new_path);

std::string home_dir();
std::string config_dir();

#ifdef WIN32
std::string volume_name(const std::string &);
std::wstring to_windows_path(const std::string &);
std::string from_windows_path(const std::wstring &);
#endif

} // End of namespace

#endif
