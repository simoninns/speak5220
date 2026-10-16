# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# The GUI's widgets are a library, so the GUI tests can link them; the
# speak5220 executable adds only main().
add_library(
    speak5220_gui
    STATIC
    about/about_dialog.cpp
    about/third_party_notices.cpp
    encoder_panel/encoder_panel.cpp
    encoder_panel/encoder_settings_form.cpp
    encoder_panel/encoding_summary.cpp
    encoder_panel/option_slider.cpp
    encoder_panel/phrase_encoder.cpp
    encoder_panel/quality_rating_view.cpp
    export/export_dialog.cpp
    main_window/dialogs.cpp
    main_window/main_window.cpp
    main_window/project_document.cpp
    main_window/recent_files.cpp
    main_window/status_text.cpp
    main_window/view_preferences.cpp
    optimise/optimise_dialog.cpp
    optimise/optimise_session.cpp
    phrase_editor/phrase_details.cpp
    phrase_list/phrase_list_delegate.cpp
    phrase_list/phrase_list_model.cpp
    phrase_list/phrase_list_panel.cpp
    preferences/preferences_dialog.cpp
    project_settings/project_settings_dialog.cpp
    playback/playback_icon.cpp
    playback/player.cpp
    playback/qt_audio_output.cpp
    theme/colour_scheme.cpp
    theme/colours.cpp
    undo/project_commands.cpp
    undo/project_edits.cpp
    waveform/frame_map.cpp
    waveform/grip_splitter.cpp
    waveform/peak_cache.cpp
    waveform/phrase_tab.cpp
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
# Windows and macOS.
if(TARGET Qt6::DBus)
    target_sources(speak5220_gui PRIVATE theme/portal_colour_scheme.cpp)
    target_link_libraries(speak5220_gui PRIVATE Qt6::DBus)
    target_compile_definitions(
        speak5220_gui
        PRIVATE SPEAK5220_HAVE_DESKTOP_PORTAL
    )
endif()

# The icon set and the About box's logo, generated from assets/logo/ by
# app/scripts/generate-icons.sh
set(SPEAK5220_RESOURCES "${PROJECT_SOURCE_DIR}/resources")
set(SPEAK5220_APP_ID io.github.simoninns.speak5220)

add_executable(
    speak5220
    WIN32
    MACOSX_BUNDLE
    main.cpp
    "${SPEAK5220_RESOURCES}/speak5220.qrc"
)
target_link_libraries(speak5220 PRIVATE speak5220_gui speak5220_compile_options)
# macOS shows its open and save panels only for a bundle with an identifier:
# without one, QFileDialog returns at once with no file
set_target_properties(
    speak5220
    PROPERTIES
        AUTORCC ON
        MACOSX_BUNDLE_GUI_IDENTIFIER ${SPEAK5220_APP_ID}
        MACOSX_BUNDLE_BUNDLE_NAME Speak5220
        MACOSX_BUNDLE_ICON_FILE speak5220.icns
)
target_compile_definitions(
    speak5220
    PRIVATE SPEAK5220_APP_ID="${SPEAK5220_APP_ID}"
)

# The icon the desktop shows for the program file and its windows: in the
# executable on Windows, in the bundle's Resources on macOS
if(WIN32)
    target_sources(speak5220 PRIVATE "${SPEAK5220_RESOURCES}/speak5220.rc")
elseif(APPLE)
    set(SPEAK5220_ICNS "${SPEAK5220_RESOURCES}/speak5220.icns")
    target_sources(speak5220 PRIVATE "${SPEAK5220_ICNS}")
    set_source_files_properties(
        "${SPEAK5220_ICNS}"
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )
endif()

install(
    TARGETS speak5220
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

# Linux and the BSDs: the menu entry, the software centre's description
# (AppStream) and the icon theme's sizes, named by the application id
if(UNIX AND NOT APPLE)
    install(
        FILES "${SPEAK5220_RESOURCES}/${SPEAK5220_APP_ID}.desktop"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(
        FILES "${SPEAK5220_RESOURCES}/${SPEAK5220_APP_ID}.metainfo.xml"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
    )
    install(
        DIRECTORY "${SPEAK5220_RESOURCES}/icons/"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons
    )
endif()
