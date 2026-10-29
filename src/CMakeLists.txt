add_executable(${PROJECT_NAME})

# Regression gate: an implicit function declaration is latent UB (the compiler
# assumes int() and default argument promotions), so treat it as an error for
# the application sources only. External libraries/submodules are not affected.
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>)

# Expose POSIX declarations (clock_gettime, CLOCK_MONOTONIC, ...) hidden by the
# strict -std=c11 mode (__STRICT_ANSI__). _GNU_SOURCE also keeps the glibc
# default extensions (M_PI, usleep, ...) that _POSIX_C_SOURCE alone disables.
# Must be defined for every translation unit before any system header, hence a
# target-level definition.
target_compile_definitions(${PROJECT_NAME} PRIVATE _GNU_SOURCE)

target_sources(${PROJECT_NAME} PUBLIC
    diag_log.c
    main.c main_screen.c
    channels.c dialog_channels.c broadcast_db.c broadcast_overlay.c cJSON.c
    styles.c spectrum.c radio.c dsp.cpp util.cpp
    app_ports.cpp
    waterfall.c rotary.cpp keyboard.c kbd_rollover.c encoder.c
    events.c msg.c msg_tiny.c keypad.c
    hkey.c clock.c
    meter.c band_info.cpp tx_info.c tx_level.c tx_player.c
    audio.c mfk.cpp cw.cpp panel.cpp
    rtty.c screenshot.c display.cpp gps.c
    ui_list.c app_info.c
    app_radio.c
    dialog.c dialog_swrscan.c
    dxcc.c
    dialog_ft8.c dialog_freq.c dialog_gps.c dialog_msg_cw.c
    ft8_ui/table_view.c
    ft8_udp.c
    dialog_msg_voice.c dialog_recorder.c dialog_qth.c dialog_callsign.c digi_menu.c
    textarea_window.c cw_encoder.c buttons.cpp vol.cpp recorder.c
    voice.cpp voice_text.cpp voice_decoder.cpp voice_review.cpp voice_keys.cpp cw_tune_ui.c adif.c qso_log.c scheduler.cpp
    format.c msg_cw_store.c
    dialog_wifi.c wifi.cpp controls.cpp usb_devices.cpp
    bt_audio.c
    knobs.cpp lock_manager.c indicators.cpp freq_info.cpp
    dialog_bt.c bt_ctl.c bt_dev.c bt_spp.c
    # The apps on their way in: each one's sources below its name
    # WSPR
    # JS8
    dialog_js8.c js8_wf.c js8_clock.c
    # PSK31
    # Olivia
    # WeFax
    dialog_wefax.c wefax/decoder.c wefax/stations.c
    # SSTV
    dialog_sstv.c sstv_resources.c sstv/decoder.c sstv/encoder.c sstv/autodetect.c sstv/vis.c sstv/linesync.c sstv/preview.c sstv/sstv_denoise.c sstv/sstv_dnn.c sstv/syncgrid.c
    dialog_winlink.c winlink_rig.c
)

add_subdirectory(fonts)
add_subdirectory(ft8)
add_subdirectory(widgets)
add_subdirectory(qth)
add_subdirectory(cfg)
add_subdirectory(cat)
add_subdirectory(settings)
add_subdirectory(cw_decoder)
add_subdirectory(js8)

include_directories(utf8)
include_directories(${CMAKE_SYSROOT}/usr/include/RHVoice/)
include_directories(${CMAKE_SYSROOT}/usr/include/ft8lib/)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    libnm dbus-glib-1 sqlite3 libpng libgps sndfile libudev libpulse libpulse-simple alsa bluez
)


target_link_libraries(${PROJECT_NAME} PRIVATE
PkgConfig::deps
ft8_core ft8_app QTH
cfg
cat_core cat_transport
cat_lan
Threads::Threads
lvgl lvgl::drivers
aether_x6100_control
liquid
ft8
cw_decoder
JS8
)

# Speech engine (voice_synth.h): rhvoice (default) or espeak - classic
# eSpeak 1.48 (Buildroot's BR2_PACKAGE_ESPEAK) or eSpeak NG.
set(X6100_TTS "rhvoice" CACHE STRING "Speech engine: rhvoice or espeak")
set_property(CACHE X6100_TTS PROPERTY STRINGS rhvoice espeak)

if(X6100_TTS STREQUAL "espeak")
    # Classic eSpeak if there, else eSpeak NG: their headers differ in path.
    find_library(ESPEAK_LIB NAMES espeak)
    if(ESPEAK_LIB)
        target_compile_definitions(${PROJECT_NAME} PRIVATE ESPEAK_CLASSIC=1)
    else()
        find_library(ESPEAK_LIB NAMES espeak-ng REQUIRED)
    endif()
    target_sources(${PROJECT_NAME} PRIVATE voice_synth_espeak.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ESPEAK_LIB})
    message(STATUS "Speech engine: eSpeak (${ESPEAK_LIB})")
elseif(X6100_TTS STREQUAL "rhvoice")
    target_sources(${PROJECT_NAME} PRIVATE voice_synth_rhvoice.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE RHVoice RHVoice_core RHVoice_audio)
    message(STATUS "Speech engine: RHVoice")
else()
    message(FATAL_ERROR "X6100_TTS must be rhvoice or espeak, not ${X6100_TTS}")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -g -fno-omit-frame-pointer -fasynchronous-unwind-tables)
    target_link_options(${PROJECT_NAME} PRIVATE -g -rdynamic)
endif()

# target_compile_options(${PROJECT_NAME} PRIVATE -g -fsanitize=address -fsanitize=undefined  -fno-omit-frame-pointer -fno-sanitize-recover)
# target_link_options(${PROJECT_NAME} PRIVATE -g -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -fno-sanitize-recover -static-libasan -static-libubsan)
