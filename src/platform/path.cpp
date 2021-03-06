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

#include "path.h"
#include "string.h"
#include "uuid.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <unordered_set>

static const size_t min_file_size = 65536;

namespace platform {

std::string clean_path(const std::string &path)
{
    std::string result;

    bool ls = false;
    for (char i : path)
    {
        if ((i != '/') || !ls)
            result.push_back(i);

        ls = i == '/';
    }

    if ((result.size() > 1) && (result.back() == '/'))
        result.pop_back();

    return result;
}

std::string mrl_from_path(const std::string &path)
{
#if !defined(WIN32)
    return "file://" + to_percent(path);
#else
    return "file:///" + to_percent(path);
#endif
}

std::string path_from_mrl(const std::string &mrl)
{
#if !defined(WIN32)
    if (starts_with(mrl, "file://"))
        return from_percent(mrl.substr(7));
#else
    if (starts_with(mrl, "file:"))
    {
        size_t pos = 5;
        while ((mrl.length() > pos) && (mrl[pos] == '/'))
            pos++;

        return from_percent(mrl.substr(pos));
    }
#endif

    return std::string();
}

} // End of namespace

#if defined(__unix__) || defined(__APPLE__)
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__APPLE__)
# include <sys/param.h>
# include <sys/mount.h>
#endif
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>

namespace platform {

std::vector<std::string> list_root_directories()
{
    std::vector<std::string> result;
    result.emplace_back("/");

    return result;
}

static const std::unordered_set<std::string> & hidden_dirs()
{
    static std::unordered_set<std::string> hidden_dirs;
    static std::once_flag flag;
    std::call_once(flag, []
    {
        hidden_dirs.insert("/bin"   );
        hidden_dirs.insert("/boot"  );
        hidden_dirs.insert("/dev"   );
        hidden_dirs.insert("/etc"   );
        hidden_dirs.insert("/lib"   );
        hidden_dirs.insert("/proc"  );
        hidden_dirs.insert("/sbin"  );
        hidden_dirs.insert("/sys"   );
        hidden_dirs.insert("/tmp"   );
        hidden_dirs.insert("/usr"   );
        hidden_dirs.insert("/var"   );
#if defined(__APPLE__)
        hidden_dirs.insert("/Applications"  );
        hidden_dirs.insert("/Library"       );
        hidden_dirs.insert("/Network"       );
        hidden_dirs.insert("/System"        );
#endif
    });

    return hidden_dirs;
}

static const std::unordered_set<std::string> & hidden_names()
{
    static std::unordered_set<std::string> hidden_names;
    static std::once_flag flag;
    std::call_once(flag, []
    {
        hidden_names.insert("@eadir");
        hidden_names.insert("lost+found");
    });

    return hidden_names;
}

static const std::unordered_set<std::string> & hidden_suffixes()
{
    static std::unordered_set<std::string> hidden_suffixes;
    static std::once_flag flag;
    std::call_once(flag, []
    {
        hidden_suffixes.insert(".db" );
        hidden_suffixes.insert(".nfo");
        hidden_suffixes.insert(".sub");
        hidden_suffixes.insert(".idx");
        hidden_suffixes.insert(".srt");
        hidden_suffixes.insert(".txt");
    });

    return hidden_suffixes;
}

static std::string suffix_of(const std::string &name)
{
    const size_t ldot = name.find_last_of('.');
    if (ldot != name.npos)
        return name.substr(ldot);

    return name;
}

std::vector<std::string> list_files(
        const std::string &path,
        file_filter filter,
        size_t max_count)
{
    std::string cpath = path;
    while (!cpath.empty() && (cpath.back() == '/')) cpath.pop_back();
    cpath.push_back('/');

    auto &hidden_dirs = platform::hidden_dirs();
    for (auto &i : hidden_dirs)
        if (starts_with(cpath, i + '/'))
            return std::vector<std::string>();

    std::vector<std::string> result;

    auto dir = ::opendir(cpath.c_str());
    if (dir)
    {
        auto &hidden_names = platform::hidden_names();
        auto &hidden_suffixes = platform::hidden_suffixes();

        for (auto dirent = ::readdir(dir);
             dirent && (result.size() < max_count);
             dirent = ::readdir(dir))
        {
            struct stat stat;
            if ((dirent->d_name[0] != '.') &&
                (::stat((cpath + dirent->d_name).c_str(), &stat) == 0))
            {
                const std::string name = dirent->d_name;
                if (!name.empty() && (name[name.length() - 1] != '~'))
                {
                    const std::string lname = to_lower(name);
                    if (S_ISDIR(stat.st_mode) &&
                        (hidden_names.find(lname) == hidden_names.end()) &&
                        (hidden_dirs.find(cpath + name) == hidden_dirs.end()))
                    {
                        result.emplace_back(name + '/');
                    }
                    else if (filter == file_filter::all)
                    {
                        result.emplace_back(std::move(name));
                    }
                    else if ((filter == file_filter::large_files) &&
                             (size_t(stat.st_size) >= min_file_size) &&
                             (hidden_suffixes.find(suffix_of(lname)) == hidden_suffixes.end()))
                    {
                        result.emplace_back(std::move(name));
                    }
                }
            }
        }

        ::closedir(dir);
    }

    return result;
}

#if defined(__APPLE__)
std::vector<std::string> list_removable_media()
{
    static const char media_dir[] = "/Volumes/";

    std::vector<std::string> result;
    for (auto &i : list_files(media_dir, platform::file_filter::directories))
        if (!list_files(media_dir + i, platform::file_filter::large_files, 1).empty())
        {
            struct statfs stat;
            if ((statfs((media_dir + i).c_str(), &stat) == 0) &&
                ((stat.f_flags & MNT_ROOTFS) == 0) &&
                (strcmp(stat.f_fstypename, "smbfs") != 0) &&
                (strcmp(stat.f_fstypename, "nfs") != 0))
            {
                result.emplace_back(media_dir + i);
            }
        }

    return result;
}
#else
std::vector<std::string> list_removable_media()
{
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
    {
        const std::string media_dir = "/media/" + std::string(pw->pw_name) + '/';

        std::vector<std::string> result;
        for (auto &i : list_files(media_dir, platform::file_filter::directories))
            if (!list_files(media_dir + i, platform::file_filter::large_files, 1).empty())
                result.emplace_back(media_dir + i);

        return result;
    }

    return std::vector<std::string>();
}
#endif

std::string file_date(const std::string &path)
{
    struct stat stat;
    if (::stat(path.c_str(), &stat) == 0)
    {
        struct tm tm;
#if defined(__APPLE__)
        if (localtime_r(&(stat.st_mtimespec.tv_sec), &tm))
#else
        if (localtime_r(&(stat.st_mtim.tv_sec), &tm))
#endif
        {
            char buffer[64];
            if (strftime(buffer, sizeof(buffer), "%F %H:%M", &tm) > 0)
                return buffer;
        }
    }

    return std::string();
}

std::string temp_file_path(const std::string &suffix)
{
    return std::string("/tmp/") + std::string(uuid::generate()) + '.' + suffix;
}

void remove_file(const std::string &path)
{
    remove(path.c_str());
}

void rename_file(const std::string &old_path, const std::string &new_path)
{
    rename(old_path.c_str(), new_path.c_str());
}

std::string home_dir()
{
    const char *home = getenv("HOME");
    if (home)
        return clean_path(home);

    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
        return clean_path(pw->pw_dir);

    return std::string();
}

std::string config_dir()
{
    const std::string home = home_dir();
    if (!home.empty())
    {
        const auto result = home +
#if defined(__APPLE__)
                "/Library/Application Support/net.sf.lximediaserver";
#else
                "/.config/lximediaserver";
#endif

        mkdir(result.c_str(), S_IRWXU);

        return result;
    }

    return std::string();
}

} // End of namespace

#elif defined(WIN32)
#include <windows.h>
#include <iostream>

namespace platform {

std::vector<std::string> list_root_directories()
{
    std::vector<std::string> result;

    wchar_t drives[4096];
    if (GetLogicalDriveStrings(sizeof(drives) / sizeof(*drives), drives) != 0)
    {
        for (wchar_t *i = drives; *i; )
        {
            const size_t len = wcslen(i);
            result.emplace_back(from_windows_path(std::wstring(i, len)));
            i += len + 1;
        }
    }

    return result;
}

std::string volume_name(const std::string &path)
{
    wchar_t volume_name[MAX_PATH + 1];
    if (GetVolumeInformation(
                to_windows_path(path).c_str(),
                volume_name,
                sizeof(volume_name) / sizeof(*volume_name),
                NULL, NULL, NULL, NULL, 0))
    {
        return from_utf16(volume_name);
    }

    return std::string();
}

bool starts_with(const std::wstring &text, const std::wstring &find)
{
    if (text.length() >= find.length())
        return wcsncmp(&text[0], &find[0], find.length()) == 0;

    return false;
}

static const std::wstring clean_path(const std::wstring &input)
{
    std::wstring result = input;
    while (!result.empty() && (result.back() == L'\\')) result.pop_back();
    return result;
}

static const std::wstring to_lower(const std::wstring &input)
{
    std::wstring result = input;
    std::transform(result.begin(), result.end(), result.begin(), ::towlower);
    return result;
}

static const std::unordered_set<std::wstring> & hidden_dirs()
{
    static std::unordered_set<std::wstring> hidden_dirs;
    static std::once_flag flag;
    std::call_once(flag, []
    {
        const wchar_t *dir = _wgetenv(L"SystemDrive");
        if (dir == nullptr) dir = L"c:";
        hidden_dirs.insert(to_lower(clean_path(dir) + L"\\program files"));
        hidden_dirs.insert(to_lower(clean_path(dir) + L"\\program files (x86)"));
        hidden_dirs.insert(to_lower(clean_path(dir) + L"\\windows"));
        hidden_dirs.insert(to_lower(clean_path(dir) + L"\\temp"));
        hidden_dirs.insert(to_lower(clean_path(dir) + L"\\recycler"));

        dir = _wgetenv(L"ProgramFiles");
        if (dir) hidden_dirs.insert(to_lower(clean_path(dir)));
        dir = _wgetenv(L"ProgramFiles(x86)");
        if (dir) hidden_dirs.insert(to_lower(clean_path(dir)));
        dir = _wgetenv(L"SystemRoot");
        if (dir) hidden_dirs.insert(to_lower(clean_path(dir)));
        dir = _wgetenv(L"TEMP");
        if (dir) hidden_dirs.insert(to_lower(clean_path(dir)));
        dir = _wgetenv(L"TMP");
        if (dir) hidden_dirs.insert(to_lower(clean_path(dir)));
        dir = _wgetenv(L"windir");
        if (dir) hidden_dirs.insert(to_lower(clean_path(dir)));

        wchar_t temp_path[MAX_PATH];
        if (GetTempPath(sizeof(temp_path) / sizeof(*temp_path), temp_path) > 0)
            hidden_dirs.insert(to_lower(clean_path(temp_path)));
    });

    return hidden_dirs;
}

static const std::unordered_set<std::wstring> & hidden_names()
{
    static std::unordered_set<std::wstring> hidden_names;
    static std::once_flag flag;
    std::call_once(flag, []
    {
        hidden_names.insert(L"@eadir");
        hidden_names.insert(L"lost+found");
        hidden_names.insert(L"recycler");
    });

    return hidden_names;
}

static const std::unordered_set<std::wstring> & hidden_suffixes()
{
    static std::unordered_set<std::wstring> hidden_suffixes;
    static std::once_flag flag;
    std::call_once(flag, []
    {
        hidden_suffixes.insert(L".db" );
        hidden_suffixes.insert(L".nfo");
        hidden_suffixes.insert(L".sub");
        hidden_suffixes.insert(L".idx");
        hidden_suffixes.insert(L".srt");
        hidden_suffixes.insert(L".txt");
    });

    return hidden_suffixes;
}

static std::wstring suffix_of(const std::wstring &name)
{
    const size_t ldot = name.find_last_of(L'.');
    if (ldot != name.npos)
        return name.substr(ldot);

    return name;
}

std::vector<std::string> list_files(
        const std::string &path,
        file_filter filter,
        size_t max_count)
{
    const std::wstring wpath = clean_path(platform::to_windows_path(::to_lower(path))) + L'\\';

    auto &hidden_dirs = platform::hidden_dirs();
    for (auto &i : hidden_dirs)
        if (starts_with(wpath, i + L'\\'))
            return std::vector<std::string>();

    std::vector<std::string> result;

    WIN32_FIND_DATA find_data;
    HANDLE handle = FindFirstFile((wpath + L'*').c_str(), &find_data);
    if (handle != INVALID_HANDLE_VALUE)
    {
        auto &hidden_names = platform::hidden_names();
        auto &hidden_suffixes = platform::hidden_suffixes();

        do
        {
            if ((find_data.cFileName[0] != L'.') &&
                ((find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0) &&
                ((find_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) == 0))
            {
                std::wstring name = find_data.cFileName, lname = to_lower(name);
                if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    (hidden_names.find(lname) == hidden_names.end()) &&
                    (hidden_dirs.find(wpath + lname) == hidden_dirs.end()))
                {
                    result.emplace_back(from_utf16(name) + '/');
                }
                else if (filter == file_filter::all)
                {
                    result.emplace_back(from_utf16(name));
                }
                else if ((filter == file_filter::large_files) &&
                         (hidden_suffixes.find(suffix_of(lname)) ==
                          hidden_suffixes.end()))
                {
                    struct __stat64 stat;
                    if ((::_wstat64(
                                (wpath + find_data.cFileName).c_str(),
                                &stat) == 0) &&
                        (size_t(stat.st_size) >= min_file_size))
                    {
                        result.emplace_back(from_utf16(name));
                    }
                }
            }
        } while((result.size() < max_count) && (FindNextFile(handle, &find_data) != 0));

        FindClose(handle);
    }

    return result;
}

std::vector<std::string> list_removable_media()
{
    std::vector<std::string> result;
    for (auto &i : list_root_directories())
    {
        switch (GetDriveType(to_windows_path(i).c_str()))
        {
        case DRIVE_REMOVABLE:
        case DRIVE_CDROM:
            if (!list_files(i, file_filter::large_files, 1).empty())
                result.emplace_back(i);

            break;

        // Detect USB hard drives
        case DRIVE_FIXED:
            if (!i.empty())
            {
                HANDLE deviceHandle = CreateFile(
                            (L"\\\\.\\" + to_windows_path(i.substr(0, i.find_first_of('/')))).c_str(),
                            0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, 0, NULL);
                if (deviceHandle != INVALID_HANDLE_VALUE)
                {
                    STORAGE_PROPERTY_QUERY query;
                    memset(&query, 0, sizeof(query));
                    query.PropertyId = StorageDeviceProperty;
                    query.QueryType = PropertyStandardQuery;

                    DWORD bytes;
                    STORAGE_DEVICE_DESCRIPTOR devd;
                    if (DeviceIoControl(
                                deviceHandle,
                                IOCTL_STORAGE_QUERY_PROPERTY,
                                &query, sizeof(query),
                                &devd, sizeof(devd),
                                &bytes, NULL))
                    {
                        if ((devd.BusType == BusTypeUsb) && !list_files(i, file_filter::large_files, 1).empty())
                            result.emplace_back(i);
                    }

                    CloseHandle(deviceHandle);
                }
            }
            break;

        default:
            break;
        }
    }

    return result;
}

std::string file_date(const std::string &path)
{
    struct _stat stat;
    if (::_wstat(to_windows_path(path).c_str(), &stat) == 0)
    {
        struct tm tm;
        if (localtime_r(&(stat.st_mtime), &tm))
        {
            wchar_t buffer[64];
            if (wcsftime(buffer, sizeof(buffer) / sizeof(*buffer), L"%Y-%m-%d %H:%M", &tm) > 0)
                return from_utf16(buffer);
        }
    }

    return std::string();
}

std::string temp_file_path(const std::string &suffix)
{
    wchar_t temp_path[MAX_PATH];
    if (GetTempPath(sizeof(temp_path) / sizeof(*temp_path), temp_path) > 0)
    {
        return platform::from_windows_path(
                    std::wstring(temp_path) + L'\\' +
                    platform::to_windows_path(std::string(uuid::generate()) + '.' + suffix));
    }

    throw std::runtime_error("failed to get temp directory");
}

void remove_file(const std::string &path)
{
    _wremove(platform::to_windows_path(path).c_str());
}

void rename_file(const std::string &old_path, const std::string &new_path)
{
    _wrename(
                platform::to_windows_path(old_path).c_str(),
                platform::to_windows_path(new_path).c_str());
}

std::string home_dir()
{
    const wchar_t * const appdata = _wgetenv(L"APPDATA");
    if (appdata)
        return from_windows_path(appdata);

    return std::string();
}

std::string config_dir()
{
    const std::string home = home_dir();
    if (!home.empty())
    {
        const std::string result = home + "/LXiMediaServer";
        _wmkdir(to_windows_path(result).c_str());
        return result;
    }

    return std::string();
}

std::wstring to_windows_path(const std::string &src)
{
    std::wstring dst = to_utf16(src);
    std::replace(dst.begin(), dst.end(), L'/', L'\\');

    return dst;
}

std::string from_windows_path(const std::wstring &src)
{
    std::string dst = from_utf16(src);
    std::replace(dst.begin(), dst.end(), '\\', '/');

    return dst;
}

} // End of namespace

#endif
