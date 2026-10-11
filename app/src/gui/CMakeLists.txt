# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# The GUI's widgets are a library, so the GUI tests can link them; the
# speak5220 executable adds only main().
add_library(
    speak5220_gui
    STATIC
    encoder_panel/encoder_panel.cpp
    encoder_panel/encoder_settings_form.cpp
    encoder_panel/encoding_summary.cpp
    encoder_panel/option_slider.cpp
    encoder_panel/phrase_encoder.cpp
    encoder_panel/quality_scale_view.cpp
    main_window/dialogs.cpp
    main_window/main_window.cpp
    main_window/project_document.cpp
    main_window/recent_files.cpp
    main_window/status_text.cpp
    optimise/optimise_dialog.cpp
    optimise/optimise_session.cpp
    phrase_editor/phrase_details.cpp
    phrase_list/phrase_list_delegate.cpp
    phrase_list/phrase_list_model.cpp
    phrase_list/phrase_list_panel.cpp
    playback/playback_icon.cpp
    playback/player.cpp
    playback/qt_audio_output.cpp
    theme/colour_scheme.cpp
    theme/colours.cpp
    undo/project_commands.cpp
    undo/project_edits.cpp
    waveform/frame_map.cpp
    waveform/peak_cache.cpp
    waveform/phrase_waveforms.cpp
    waveform/time_axis.cpp
    waveform/waveform_pane.cpp
    waveform/waveform_view.cpp
)

set_target_properties(speak5220_gui PROPERTIES AUTOMOC ON)
target_include_directories(speak5220_gui PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    speak5220_gui
    PUBLIC s5core Qt6::Widgets Qt6::Multimedia
    PRIVATE speak5220_compile_options
)

# The desktop's light or dark preference comes from the XDG desktop portal
# over D-Bus on Linux and the BSDs; Qt follows the desktop itself on
# Windows and macOS (plan §5, "Colours").
if(TARGET Qt6::DBus)
    target_sources(speak5220_gui PRIVATE theme/portal_colour_scheme.cpp)
    target_link_libraries(speak5220_gui PRIVATE Qt6::DBus)
    target_compile_definitions(
        speak5220_gui
        PRIVATE SPEAK5220_HAVE_DESKTOP_PORTAL
    )
endif()

add_executable(speak5220 WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(speak5220 PRIVATE speak5220_gui speak5220_compile_options)

install(
    TARGETS speak5220
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)
