cmake_minimum_required(VERSION 2.8)

option(MPV_BUILD_TARGET_DESKTOP "Build project for desktop to ease debugging" FALSE)
set(MPV_FFMPEG_LIBS_DIR "" CACHE STRING "Specfiy FFMPEG static library directory")


if (NOT MPV_BUILD_TARGET_DESKTOP)
    # apply vitasdk toolchain setup
    if(NOT DEFINED ENV{VITASDK})
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif()
    set(CMAKE_TOOLCHAIN_FILE
        "$ENV{VITASDK}/share/vita.toolchain.cmake"
        CACHE PATH "toolchain file")
    include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

    # application informtions
    set(VITA_APP_NAME "MPV-Vita")
    set(VITA_TITLEID  "MPV000001")
    set(VITA_VERSION  "01.00")
endif()

project(mpv-vita)

set(mpv_core_sources
    ## Audio
    audio/aframe.c
    audio/chmap.c
    audio/chmap_sel.c
    audio/decode/ad_lavc.c
    audio/decode/ad_spdif.c
    audio/filter/af_drop.c
    audio/filter/af_format.c
    audio/filter/af_lavcac3enc.c
    audio/filter/af_scaletempo.c
    audio/filter/af_scaletempo2.c
    audio/filter/af_scaletempo2_internals.c
    audio/fmt-conversion.c
    audio/format.c
    audio/out/ao.c
    audio/out/ao_lavc.c
    audio/out/ao_null.c
    audio/out/ao_pcm.c
    audio/out/buffer.c

    ## Core
    common/av_common.c
    common/av_log.c
    common/codecs.c
    common/common.c
    common/encode_lavc.c
    common/msg.c
    common/playlist.c
    common/recorder.c
    common/stats.c
    common/tags.c
    common/version.c

    ## Demuxers
    demux/codec_tags.c
    demux/cue.c
    demux/cache.c
    demux/demux.c
    demux/demux_cue.c
    demux/demux_disc.c
    demux/demux_edl.c
    demux/demux_lavf.c
    demux/demux_mf.c
    demux/demux_mkv.c
    demux/demux_mkv_timeline.c
    demux/demux_null.c
    demux/demux_playlist.c
    demux/demux_raw.c
    demux/demux_timeline.c
    demux/ebml.c
    demux/packet.c
    demux/timeline.c

    ## Filters
    filters/f_async_queue.c
    filters/f_autoconvert.c
    filters/f_auto_filters.c
    filters/f_decoder_wrapper.c
    filters/f_demux_in.c
    filters/f_hwtransfer.c
    filters/f_lavfi.c
    filters/f_output_chain.c
    filters/f_swresample.c
    filters/f_swscale.c
    filters/f_utils.c
    filters/filter.c
    filters/frame.c
    filters/user_filters.c

    ## Input
    input/cmd.c
    input/event.c
    input/input.c
    input/keycodes.c

    ## Misc
    misc/bstr.c
    misc/charset_conv.c
    misc/dispatch.c
    misc/json.c
    misc/natural_sort.c
    misc/node.c
    misc/rendezvous.c
    misc/thread_pool.c
    misc/thread_tools.c

    ## Options
    options/m_config_core.c
    options/m_config_frontend.c
    options/m_option.c
    options/m_property.c
    options/options.c
    options/parse_commandline.c
    options/parse_configfile.c
    options/path.c

    ## Player
    player/audio.c
    player/client.c
    player/command.c
    player/configfiles.c
    player/external_files.c
    player/loadfile.c
    player/main.c
    player/misc.c
    player/osd.c
    player/playloop.c
    player/screenshot.c
    player/scripting.c
    player/sub.c
    player/video.c

    ## Streams
    stream/cookies.c
    stream/stream.c
    stream/stream_avdevice.c
    stream/stream_cb.c
    stream/stream_concat.c
    stream/stream_edl.c
    stream/stream_file.c
    stream/stream_lavf.c
    stream/stream_memory.c
    stream/stream_mf.c
    stream/stream_null.c
    stream/stream_slice.c

    ## Subtitles
    sub/ass_mp.c
    sub/dec_sub.c
    sub/draw_bmp.c
    sub/filter_sdh.c
    sub/img_convert.c
    sub/lavc_conv.c
    sub/osd.c
    sub/sd_ass.c
    sub/sd_lavc.c

    ## Video
    video/csputils.c
    video/decode/vd_lavc.c
    video/filter/refqueue.c
    video/filter/vf_format.c
    video/filter/vf_sub.c
    video/fmt-conversion.c
    video/hwdec.c
    video/image_loader.c
    video/image_writer.c
    video/img_format.c
    video/mp_image.c
    video/mp_image_pool.c
    video/out/aspect.c
    video/out/bitmap_packer.c
    video/out/dither.c
    video/out/dr_helper.c
    video/out/filter_kernels.c
    video/out/gpu/context.c
    video/out/gpu/error_diffusion.c
    video/out/gpu/hwdec.c
    video/out/gpu/lcms.c
    video/out/gpu/libmpv_gpu.c
    video/out/gpu/osd.c
    video/out/gpu/ra.c
    video/out/gpu/shader_cache.c
    video/out/gpu/spirv.c
    video/out/gpu/user_shaders.c
    video/out/gpu/utils.c
    video/out/gpu/video.c
    video/out/gpu/video_shaders.c
    video/out/libmpv_sw.c
    video/out/vo.c
    video/out/vo_gpu.c
    video/out/vo_image.c
    video/out/vo_lavc.c
    video/out/vo_libmpv.c
    video/out/vo_null.c
    video/out/vo_tct.c
    video/out/win_state.c
    video/repack.c
    video/sws_utils.c

    ## osdep
    osdep/io.c
    osdep/semaphore_osx.c
    osdep/subprocess.c
    osdep/threads.c
    osdep/timer.c

    ## tree_allocator
    ta/ta.c
    ta/ta_talloc.c
    ta/ta_utils.c

    ## Vita
    sub/osd_dummy.c
    input/ipc-dummy.c
    osdep/path-vita.c
    osdep/timer-linux.c
    osdep/terminal-dummy.c
    osdep/subprocess-dummy.c
)


function(_generate_source _out_var _abs_out_path _script_path)
    set(_abs_script_path ${CMAKE_CURRENT_SOURCE_DIR}/${_script_path})
    get_filename_component(_abs_out_dir ${_abs_out_path} DIRECTORY)
    add_custom_command(OUTPUT ${_abs_out_path}
        DEPENDS ${_abs_script_path}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_abs_out_dir}
        COMMAND ${_abs_script_path} ${ARGN} ${_abs_out_path}
    )

    list(APPEND _out_result ${${_out_var}} ${_abs_out_path} )
    set(${_out_var} ${_out_result} PARENT_SCOPE)
endfunction()

# generate file
set(mpv_core_generated_files)
set(_core_gen_dir ${CMAKE_CURRENT_BINARY_DIR}/generated_files)
_generate_source(mpv_core_generated_files ${_core_gen_dir}/generated/version.h
    version.py)
_generate_source(mpv_core_generated_files ${_core_gen_dir}/generated/ebml_defs.inc
    TOOLS/matroska.py --generate-definitions)
_generate_source(mpv_core_generated_files ${_core_gen_dir}/generated/ebml_types.h
    TOOLS/matroska.py --generate-header)


if (EXISTS ${MPV_FFMPEG_LIBS_DIR})
    set(_pkg_ffmpeg_inc_dirs
        ${MPV_FFMPEG_LIBS_DIR}/include)
    set(_pkg_ffmpeg_libs
        ${MPV_FFMPEG_LIBS_DIR}/lib/libavutil.a
        ${MPV_FFMPEG_LIBS_DIR}/lib/libavcodec.a
        ${MPV_FFMPEG_LIBS_DIR}/lib/libavformat.a
        ${MPV_FFMPEG_LIBS_DIR}/lib/libswscale.a
        ${MPV_FFMPEG_LIBS_DIR}/lib/libavfilter.a
        ${MPV_FFMPEG_LIBS_DIR}/lib/libswresample.a
        m
        z
        mp3lame
    )
else()
    include(FindPkgConfig)
    pkg_check_modules(_pkg_avutil REQUIRED libavutil>=56.12.100)
    pkg_check_modules(_pkg_avcodec REQUIRED libavcodec>=58.12.100)
    pkg_check_modules(_pkg_avformat REQUIRED libavformat>=58.9.100)
    pkg_check_modules(_pkg_swscale REQUIRED libswscale>=5.0.101)
    pkg_check_modules(_pkg_avfilter REQUIRED libavfilter>=7.14.100)
    pkg_check_modules(_pkg_swresample REQUIRED libswresample>=3.0.100)
    set(_pkg_ffmpeg_inc_dirs
        ${_pkg_avutil_INCLUDE_DIRS}
        ${_pkg_avcodec_INCLUDE_DIRS}
        ${_pkg_avformat_INCLUDE_DIRS}
        ${_pkg_swscale_INCLUDE_DIRS}
        ${_pkg_avfilter_INCLUDE_DIRS}
        ${_pkg_swresample_INCLUDE_DIRS}
    )
    set(_pkg_ffmpeg_libs
        ${_pkg_avutil_STATIC_LIBRARIES}
        ${_pkg_avcodec_STATIC_LIBRARIES}
        ${_pkg_avformat_STATIC_LIBRARIES}
        ${_pkg_swscale_STATIC_LIBRARIES}
        ${_pkg_avfilter_STATIC_LIBRARIES}
        ${_pkg_swresample_STATIC_LIBRARIES}
    )
endif()


include(FindPkgConfig)
pkg_check_modules(_pkg_ass REQUIRED libass>=0.12.2)


add_library(mpv-core STATIC
    ${mpv_core_sources}
    ${mpv_core_generated_files}
)

target_link_libraries(mpv-core
    m
    -pthread
    ${_pkg_ass_STATIC_LIBRARIES}
    ${_pkg_ffmpeg_libs}
)

target_include_directories(mpv-core PRIVATE
    osdep/vita
    ${_core_gen_dir}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${_pkg_ass_INCLUDE_DIRS}
    ${_pkg_ffmpeg_inc_dirs}
)

add_executable(mpv-test
    osdep/main-fn-unix.c)

target_link_libraries(mpv-test mpv-core)
