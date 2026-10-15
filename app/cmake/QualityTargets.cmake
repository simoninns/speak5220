# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# The developer convenience targets:
#   format  applies clang-format, ruff format, nixfmt and gersemi in place;
#   lint    runs every read-only check, clang-tidy included.
# Both are thin wrappers over scripts in app/scripts/, which the pre-commit
# hooks and CI also use.

get_filename_component(_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

add_custom_target(
    format
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/format.sh"
    WORKING_DIRECTORY "${_repo_root}"
    COMMENT "Formatting sources in place"
    USES_TERMINAL
    VERBATIM
)

add_custom_target(
    lint
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/lint.sh" "${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY "${_repo_root}"
    COMMENT "Running format, lint and static analysis checks"
    USES_TERMINAL
    VERBATIM
)
