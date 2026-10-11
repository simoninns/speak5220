# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(support)

speak5220_add_test_lane(
    speak5220_gui_unit_tests
    unit
    LIBRARIES speak5220_gui speak5220_gui_test_main
)
speak5220_add_test_lane(
    speak5220_gui_functional_tests
    functional
    LIBRARIES speak5220_gui speak5220_gui_test_main
)
# The functional lane opens the fixture projects in app/projects/ (copied
# into a temporary directory before anything is saved).
target_compile_definitions(
    speak5220_gui_functional_tests
    PRIVATE SPEAK5220_PROJECTS_DIR="${PROJECT_SOURCE_DIR}/projects"
)
