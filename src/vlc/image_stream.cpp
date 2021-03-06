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

#include "vlc/image_stream.h"
#include "vlc/media.h"
#include "vlc/instance.h"
#include "vlc/image.h"
#include <vlc/vlc.h>
#include <thread>

namespace vlc {

platform::process::function_handle image_stream::transcode_function =
        platform::process::register_function(&image_stream::transcode_process);

image_stream::image_stream()
    : std::istream(nullptr),
      process()
{
}

image_stream::~image_stream()
{
    close();
}

int image_stream::transcode_process(platform::process &process)
{
    std::vector<std::string> vlc_options;
    vlc_options.push_back("--no-sub-autodetect-file");
    vlc::instance instance(vlc_options);

    std::string mrl, mime;
    process >> mrl >> mime;

    auto media = media::from_mrl(instance, mrl);

    unsigned width = 0, height = 0;
    process >> width >> height;

    struct T
    {
        static void callback(const libvlc_event_t *e, void *opaque)
        {
            T * const t = reinterpret_cast<T *>(opaque);
            std::lock_guard<std::mutex> _(t->mutex);

            if (e->type == libvlc_MediaPlayerEncounteredError)
                t->encountered_error = true;
            else if (e->type == libvlc_MediaPlayerTimeChanged)
                t->position = e->u.media_player_time_changed.new_time;

            t->condition.notify_one();
        }

        static void play(void */*opaque*/, const void */*samples*/, unsigned /*count*/, int64_t /*pts*/)
        {
        }

        static void * lock(void *opaque, void **planes)
        {
            T * const t = reinterpret_cast<T *>(opaque);
            return *planes = t->pixel_buffer.scan_line(0);
        }

        static void display(void *opaque, void *)
        {
            T * const t = reinterpret_cast<T *>(opaque);
            t->displayed = true;
            t->condition.notify_one();
        }

        std::mutex mutex;
        std::condition_variable condition;
        bool displayed;
        bool encountered_error;
        libvlc_time_t position;
        image pixel_buffer;

        libvlc_media_player_t *player;
    } t;

    t.displayed = false;
    t.encountered_error = false;
    t.position = 0;
    t.pixel_buffer = image(width, height);

    t.player = libvlc_media_player_new_from_media(media);
    if (t.player)
    {
        libvlc_media_player_set_rate(t.player, 10.0f);

        libvlc_audio_set_callbacks(t.player, &T::play, nullptr, nullptr, nullptr, nullptr, &t);
        libvlc_audio_set_format(t.player, "S16N", 44100, 2);
        libvlc_video_set_callbacks(t.player, &T::lock, nullptr, &T::display, &t);
        libvlc_video_set_format(t.player, "RV32", width, height, width * sizeof(uint32_t));

        auto event_manager = libvlc_media_player_event_manager(t.player);
        libvlc_event_attach(event_manager, libvlc_MediaPlayerTimeChanged, &T::callback, &t);
        libvlc_event_attach(event_manager, libvlc_MediaPlayerEncounteredError, &T::callback, &t);

        if (libvlc_media_player_play(t.player) == 0)
        {
            std::unique_lock<std::mutex> l(t.mutex);

            while (!t.displayed && !t.encountered_error &&
                   (t.position < 5000) && process)
            {
                t.condition.wait(l);
            }

            l.unlock();
            libvlc_media_player_stop(t.player);
        }

        libvlc_event_detach(event_manager, libvlc_MediaPlayerEncounteredError, &T::callback, &t);
        libvlc_event_detach(event_manager, libvlc_MediaPlayerTimeChanged, &T::callback, &t);

        libvlc_media_player_release(t.player);

        if (t.displayed && !t.encountered_error && process)
        {
            t.pixel_buffer.swap_rb();

            if      (mime == "image/png") t.pixel_buffer.save_png(process);
            else if (mime == "image/jpeg") t.pixel_buffer.save_jpeg(process);

            process << std::flush;
        }
    }

    return 0;
}

bool image_stream::open(
        const std::string &mrl,
        const std::string &mime,
        unsigned width, unsigned height)
{
    close();

    process.reset(new platform::process(transcode_function));
    *process << mrl << ' '
             << mime << ' '
             << width << ' '
             << height << std::endl;

    std::istream::rdbuf(process->rdbuf());
    return true;
}

void image_stream::close()
{
    std::istream::rdbuf(nullptr);

    if (process)
    {
        if (process->joinable())
        {
            // Flush pipe to prevent deadlock while stopping player.
            std::thread flush([this] { while (*process) process->get(); });

            process->send_term();
            process->join();

            flush.join();
        }

        process = nullptr;
    }
}

} // End of namespace
