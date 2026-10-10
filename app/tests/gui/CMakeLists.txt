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
