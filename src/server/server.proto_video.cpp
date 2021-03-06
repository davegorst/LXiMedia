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

#include "server.h"
#include "platform/string.h"
#include "pupnp/upnp.h"
#include "vlc/instance.h"

template <size_t _count>
static void add_source_video_protocols(
        class pupnp::connection_manager &connection_manager,
        const char *name,
        const char *mime, const char *suffix,
        unsigned sample_rate, unsigned channels,
        const unsigned (& width)[_count], const unsigned (& height)[_count],
        const float (& aspect)[_count],
        const unsigned (& frame_rate_num)[_count], const unsigned (& frame_rate_den)[_count],
        const char *audio_codec, unsigned audio_rate,
        const char *video_codec, unsigned video_rate,
        const char *mux,
        const char *fast_encode_options, const char *slow_encode_options)
{
    for (size_t i = 0; i < _count; i++)
        connection_manager.add_source_video_protocol(
                    name,
                    mime, suffix,
                    sample_rate, channels,
                    width[i], height[i], aspect[i],
                    frame_rate_num[i], frame_rate_den[i],
                    audio_codec, audio_rate,
                    video_codec, video_rate,
                    mux,
                    fast_encode_options,
                    slow_encode_options);
}

template <size_t _count>
static void add_source_video_protocols(
        class pupnp::connection_manager &connection_manager,
        const char *name,
        const char *mime, const char *suffix,
        unsigned sample_rate, unsigned channels,
        unsigned width, unsigned height, float aspect,
        const unsigned (& frame_rate_num)[_count], const unsigned (& frame_rate_den)[_count],
        const char *audio_codec, unsigned audio_rate,
        const char *video_codec, unsigned video_rate,
        const char *mux,
        const char *fast_encode_options, const char *slow_encode_options)
{
    for (size_t i = 0; i < _count; i++)
        connection_manager.add_source_video_protocol(
                    name,
                    mime, suffix,
                    sample_rate, channels,
                    width, height, aspect,
                    frame_rate_num[i], frame_rate_den[i],
                    audio_codec, audio_rate,
                    video_codec, video_rate,
                    mux,
                    fast_encode_options,
                    slow_encode_options);
}

template <size_t _count>
static void add_source_video_protocols(
        class pupnp::connection_manager &connection_manager,
        const char * const (& name)[6], const bool (& has_format)[3],
        unsigned sample_rate, unsigned channels,
        const unsigned (& width)[_count], const unsigned (& height)[_count],
        const float (& aspect)[_count],
        const char *audio_codec, unsigned audio_rate,
        const char *video_codec, unsigned video_rate,
        const char *fast_encode_options, const char *slow_encode_options)
{
    static const unsigned eu_frame_rate_num[] = { 24000, 25000 }, eu_frame_rate_den[] = { 1000, 1000 };
    static const unsigned na_frame_rate_num[] = { 24000, 30000, 30000 }, na_frame_rate_den[] = { 1001, 1001, 1000 };

    for (size_t i = 0; i < _count / 2; i++)
    {
        if (has_format[0])
            add_source_video_protocols(
                        connection_manager,
                        name[0],
                        pupnp::upnp::mime_video_mpegm2ts, "m2ts",
                        sample_rate, channels,
                        width[i], height[i], aspect[i],
                        eu_frame_rate_num, eu_frame_rate_den,
                        audio_codec, audio_rate,
                        video_codec, video_rate,
                        "m2ts",
                        fast_encode_options,
                        slow_encode_options);

        if (has_format[1])
            add_source_video_protocols(
                        connection_manager,
                        name[1],
                        pupnp::upnp::mime_video_mpegts, "ts",
                        sample_rate, channels,
                        width[i], height[i], aspect[i],
                        eu_frame_rate_num, eu_frame_rate_den,
                        audio_codec, audio_rate,
                        video_codec, video_rate,
                        "ts",
                        fast_encode_options,
                        slow_encode_options);

        if (has_format[2])
            add_source_video_protocols(
                        connection_manager,
                        name[2],
                        pupnp::upnp::mime_video_mpeg, "mpg",
                        sample_rate, channels,
                        width[i], height[i], aspect[i],
                        eu_frame_rate_num, eu_frame_rate_den,
                        audio_codec, audio_rate,
                        video_codec, video_rate,
                        "ps",
                        fast_encode_options,
                        slow_encode_options);
    }

    for (size_t i = _count / 2; i < _count; i++)
    {
        if (has_format[0])
            add_source_video_protocols(
                        connection_manager,
                        name[3],
                        pupnp::upnp::mime_video_mpegm2ts, "m2ts",
                        sample_rate, channels,
                        width[i], height[i], aspect[i],
                        na_frame_rate_num, na_frame_rate_den,
                        audio_codec, audio_rate,
                        video_codec, video_rate,
                        "m2ts",
                        fast_encode_options,
                        slow_encode_options);

        if (has_format[1])
            add_source_video_protocols(
                        connection_manager,
                        name[4],
                        pupnp::upnp::mime_video_mpegts, "ts",
                        sample_rate, channels,
                        width[i], height[i], aspect[i],
                        na_frame_rate_num, na_frame_rate_den,
                        audio_codec, audio_rate,
                        video_codec, video_rate,
                        "ts",
                        fast_encode_options,
                        slow_encode_options);

        if (has_format[2])
            add_source_video_protocols(
                        connection_manager,
                        name[5],
                        pupnp::upnp::mime_video_mpeg, "mpg",
                        sample_rate, channels,
                        width[i], height[i], aspect[i],
                        na_frame_rate_num, na_frame_rate_den,
                        audio_codec, audio_rate,
                        video_codec, video_rate,
                        "ps",
                        fast_encode_options,
                        slow_encode_options);
    }
}

void server::add_video_protocols()
{
    const bool vlc_2_1 = compare_version(vlc::instance::version(), "2.1") == 0;
    const auto surround_mode = settings.surround_mode();
    const bool has_surround51 = (surround_mode == ::surround_mode::surround51);
    const auto video_mode = settings.video_mode();
    const bool has_vcd              = (video_mode == ::video_mode::auto_) || (video_mode == ::video_mode::vcd       );
    const bool has_dvd              = (video_mode == ::video_mode::auto_) || (video_mode == ::video_mode::dvd       );
    const bool has_hdtv_720         = (video_mode == ::video_mode::auto_) || (video_mode == ::video_mode::hdtv_720  );
    const bool has_hdtv_1080        = (video_mode == ::video_mode::auto_) || (video_mode == ::video_mode::hdtv_1080 );
    const bool has_mpeg2 = settings.mpeg2_enabled();
    const bool has_mpeg2_format[] = { settings.video_mpegm2ts_enabled(), settings.video_mpegts_enabled(), settings.video_mpeg_enabled() };
    const bool has_mpeg4 = settings.mpeg4_enabled();
    const bool has_mpeg4_format[] = { settings.video_mpegm2ts_enabled(), settings.video_mpegts_enabled(), false };

    // See: http://www.videolan.org/doc/streaming-howto/en/ch03.html

    static const unsigned width_sd[] = { 720, 720, 640, 704 }, height_sd[] = { 576, 576, 480, 480 };
    static const float aspect_sd[] = { 1.06667f, 1.42222f, 1.0f, 1.21212f};
    static const unsigned width_720[] = { 1280, 1280 }, height_720[] = { 720, 720 };
    static const float aspect_720[] = { 1.0f, 1.0f};
    static const unsigned width_1080[] = { 1920, 1920 }, height_1080[] = { 1080, 1080 };
    static const float aspect_1080[] = { 1.0f, 1.0f};

    /////////////////////////////////////////////////////////////////////////////
    // MPEG1 VCD
    if (has_mpeg2 && has_vcd)
    {
        static const unsigned width[] = { 352, 320 }, height[] = { 288, 240 };
        static const float aspect[] = { 1.0f, 1.0f };
        static const unsigned frame_rate_num[] = { 25000, 30000 }, frame_rate_den[] = { 1000, 1001 };

        add_source_video_protocols(
                    connection_manager,
                    "MPEG1",
                    pupnp::upnp::mime_video_mpeg, "mpg",
                    44100, 2,
                    width, height, aspect,
                    frame_rate_num, frame_rate_den,
                    "mpga", 256, "mp1v", 2048, "mpeg1",
                    "venc=ffmpeg{keyint=0}",
                    "venc=ffmpeg{bframes=0}");
    }

    /////////////////////////////////////////////////////////////////////////////
    // MPEG2 PAL/NTSC
    if (has_mpeg2 && has_dvd)
    {
        connection_manager.add_source_video_protocol(
                    "MPEG_PS_PAL",
                    pupnp::upnp::mime_video_mpeg, "mpg",
                    44100, 2,
                    720, 576, 1.06667f,
                    25000, 1000,
                    "mpga", 256, "mp2v", !vlc_2_1 ? 6144 : 12288, "ps",
                    "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");

        connection_manager.add_source_video_protocol(
                    "MPEG_PS_PAL",
                    pupnp::upnp::mime_video_mpeg, "mpg",
                    44100, 2,
                    720, 576, 1.42222f,
                    25000, 1000,
                    "mpga", 256, "mp2v", !vlc_2_1 ? 6144 : 12288, "ps",
                    "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");

        connection_manager.add_source_video_protocol(
                    "MPEG_PS_NTSC",
                    pupnp::upnp::mime_video_mpeg, "mpg",
                    44100, 2,
                    640, 480, 1.0f,
                    30000, 1001,
                    "mpga", 256, "mp2v", !vlc_2_1 ? 6144 : 12288, "ps",
                    "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");

        connection_manager.add_source_video_protocol(
                    "MPEG_PS_NTSC",
                    pupnp::upnp::mime_video_mpeg, "mpg",
                    44100, 2,
                    704, 480, 1.21212f,
                    30000, 1001,
                    "mpga", 256, "mp2v", !vlc_2_1 ? 6144 : 12288, "ps",
                    "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");
    }

    if (has_mpeg2 && has_dvd && has_surround51)
    {
        connection_manager.add_source_video_protocol(
                    "MPEG_PS_PAL_XAC3",
                    pupnp::upnp::mime_video_mpeg, "mpg",
                    48000, 6,
                    720, 576, 1.06667f,
                    25000, 1000,
                    "a52", 448, "mp2v", !vlc_2_1 ? 6144 : 12288, "ps",
                    "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");

        connection_manager.add_source_video_protocol(
                    "MPEG_PS_PAL_XAC3",
                    pupnp::upnp::mime_video_mpeg, "mpg",
                    48000, 6,
                    720, 576, 1.42222f,
                    25000, 1000,
                    "a52", 448, "mp2v", !vlc_2_1 ? 6144 : 12288, "ps",
                    "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");

        connection_manager.add_source_video_protocol(
                     "MPEG_PS_NTSC_XAC3",
                    pupnp::upnp::mime_video_mpeg, "mpg",
                    48000, 6,
                    640, 480, 1.0f,
                    30000, 1001,
                    "a52", 448, "mp2v", !vlc_2_1 ? 6144 : 12288, "ps",
                    "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");

        connection_manager.add_source_video_protocol(
                     "MPEG_PS_NTSC_XAC3",
                    pupnp::upnp::mime_video_mpeg, "mpg",
                    48000, 6,
                    704, 480, 1.21212f,
                    30000, 1001,
                    "a52", 448, "mp2v", !vlc_2_1 ? 6144 : 12288, "ps",
                    "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");
    }

    /////////////////////////////////////////////////////////////////////////////
    // MPEG2 SD
    {
        static const char * const name[] = {
            "MPEG_TS_SD_EU", "MPEG_TS_SD_EU_ISO", "MPEG_PS_SD_EU_NONSTD",
            "MPEG_TS_SD_NA", "MPEG_TS_SD_NA_ISO", "MPEG_PS_SD_NA_NONSTD" };

        if (has_mpeg2 && has_dvd)
            add_source_video_protocols(
                        connection_manager,
                        name, has_mpeg2_format,
                        44100, 2,
                        width_sd, height_sd, aspect_sd,
                        "mpga", 256, "mp2v", !vlc_2_1 ? 6144 : 12288,
                        "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");

        if (has_mpeg2 && has_dvd && has_surround51)
            add_source_video_protocols(
                        connection_manager,
                        name, has_mpeg2_format,
                        48000, 6,
                        width_sd, height_sd, aspect_sd,
                        "a52", 448, "mp2v", !vlc_2_1 ? 6144 : 12288,
                        "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");
    }

    /////////////////////////////////////////////////////////////////////////////
    // MPEG2 HD
    {
        static const char * const name[] = {
            "MPEG_TS_HD_EU", "MPEG_TS_HD_EU_ISO", "MPEG_PS_HD_EU_NONSTD",
            "MPEG_TS_HD_NA", "MPEG_TS_HD_NA_ISO", "MPEG_PS_HD_NA_NONSTD" };

        if (has_mpeg2 && has_hdtv_720)
            add_source_video_protocols(
                        connection_manager,
                        name, has_mpeg2_format,
                        44100, 2,
                        width_720, height_720, aspect_720,
                        "mpga", 256, "mp2v", !vlc_2_1 ? 8192 : 16384,
                        "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");

        if (has_mpeg2 && has_hdtv_720 && has_surround51)
            add_source_video_protocols(
                        connection_manager,
                        name, has_mpeg2_format,
                        48000, 6,
                        width_720, height_720, aspect_720,
                        "a52", 448, "mp2v", !vlc_2_1 ? 8192 : 16384,
                        "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");

        if (has_mpeg2 && has_hdtv_1080)
            add_source_video_protocols(
                        connection_manager,
                        name, has_mpeg2_format,
                        44100, 2,
                        width_1080, height_1080, aspect_1080,
                        "mpga", 256, "mp2v", !vlc_2_1 ? 12288 : 24576,
                        "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");

        if (has_mpeg2 && has_hdtv_1080 && has_surround51)
            add_source_video_protocols(
                        connection_manager,
                        name, has_mpeg2_format,
                        48000, 6,
                        width_1080, height_1080, aspect_1080,
                        "a52", 448, "mp2v", !vlc_2_1 ? 12288 : 24576,
                        "venc=ffmpeg{keyint=0}", "venc=ffmpeg{bframes=0}");
    }

    /////////////////////////////////////////////////////////////////////////////
    // MPEG4 VCD
    if (has_mpeg4 && has_vcd)
    {
        static const char * const name[] = {
            "AVC_TS_BL_CIF30_AC3", "AVC_TS_BL_CIF30_AC3_ISO", nullptr,
            "AVC_TS_BL_CIF30_AC3", "AVC_TS_BL_CIF30_AC3_ISO", nullptr };
        static const unsigned width[] = { 352, 320 }, height[] = { 288, 240 };
        static const float aspect[] = { 1.0f, 1.0f };

        add_source_video_protocols(
                    connection_manager,
                    name, has_mpeg4_format,
                    48000, 2,
                    width, height, aspect,
                    "a52", 192, "h264", 2048,
                    "venc=x264{keyint=1,bframes=0}",
                    "venc=x264{keyint=25,bframes=3}");
    }

    /////////////////////////////////////////////////////////////////////////////
    // MPEG4 SD
    if (has_mpeg4 && has_dvd)
    {
        static const char * const name[] = {
            "AVC_TS_MP_SD_AC3", "AVC_TS_MP_SD_AC3_ISO", nullptr,
            "AVC_TS_MP_SD_AC3", "AVC_TS_MP_SD_AC3_ISO", nullptr };

        add_source_video_protocols(
                    connection_manager,
                    name, has_mpeg4_format,
                    48000, 2,
                    width_sd, height_sd, aspect_sd,
                    "a52", 192, "h264", 2048,
                    "venc=x264{keyint=1,bframes=0}",
                    "venc=x264{keyint=25,bframes=3}");
    }

    if (has_mpeg4 && has_dvd && has_surround51)
    {
        static const char * const name[] = {
            "AVC_TS_MP_SD_AC3", "AVC_TS_MP_SD_AC3_ISO", nullptr,
            "AVC_TS_MP_SD_AC3", "AVC_TS_MP_SD_AC3_ISO", nullptr };

        add_source_video_protocols(
                    connection_manager,
                    name, has_mpeg4_format,
                    48000, 6,
                    width_sd, height_sd, aspect_sd,
                    "a52", 448, "h264", 2048,
                    "venc=x264{keyint=1,bframes=0}",
                    "venc=x264{keyint=25,bframes=3}");
    }

    /////////////////////////////////////////////////////////////////////////////
    // MPEG4 HD
    {
        static const char * const name_ac3[] = {
            "AVC_TS_MP_HD_AC3", "AVC_TS_MP_HD_AC3_ISO", nullptr,
            "AVC_TS_MP_HD_AC3", "AVC_TS_MP_HD_AC3_ISO", nullptr };

        if (has_mpeg4 && has_hdtv_720)
            add_source_video_protocols(
                        connection_manager,
                        name_ac3, has_mpeg4_format,
                        48000, 2,
                        width_720, height_720, aspect_720,
                        "a52", 192, "h264", 4096,
                        "venc=x264{keyint=1,bframes=0}",
                        "venc=x264{keyint=25,bframes=3}");

        if (has_mpeg4 && has_hdtv_720 && has_surround51)
            add_source_video_protocols(
                        connection_manager,
                        name_ac3, has_mpeg4_format,
                        48000, 6,
                        width_720, height_720, aspect_720,
                        "a52", 448, "h264", 4096,
                        "venc=x264{keyint=1,bframes=0}",
                        "venc=x264{keyint=25,bframes=3}");

        if (has_mpeg4 && has_hdtv_1080)
            add_source_video_protocols(
                        connection_manager,
                        name_ac3, has_mpeg4_format,
                        48000, 2,
                        width_1080, height_1080, aspect_1080,
                        "a52", 192, "h264", 8192,
                        "venc=x264{keyint=1,bframes=0}",
                        "venc=x264{keyint=25,bframes=3}");

        if (has_mpeg4 && has_hdtv_1080 && has_surround51)
            add_source_video_protocols(
                        connection_manager,
                        name_ac3, has_mpeg4_format,
                        48000, 6,
                        width_1080, height_1080, aspect_1080,
                        "a52", 448, "h264", 8192,
                        "venc=x264{keyint=1,bframes=0}",
                        "venc=x264{keyint=25,bframes=3}");
    }
}
