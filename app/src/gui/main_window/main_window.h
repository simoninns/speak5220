/*
 * File:        main_window.h
 * Module:      main_window
 * Purpose:     The application's top-level window
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_MAIN_WINDOW_MAIN_WINDOW_H_
#define SPEAK5220_GUI_MAIN_WINDOW_MAIN_WINDOW_H_

#include <QMainWindow>

namespace speak5220::app {

// The top-level window. Not thread-safe: use on the GUI thread only.
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_MAIN_WINDOW_MAIN_WINDOW_H_
