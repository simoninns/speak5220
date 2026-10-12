/*
 * File:        grip_splitter.h
 * Module:      main_window
 * Purpose:     A splitter whose handle shows a grip, so it can be found on
 *              every platform's style
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_MAIN_WINDOW_GRIP_SPLITTER_H_
#define SPEAK5220_GUI_MAIN_WINDOW_GRIP_SPLITTER_H_

#include <QSplitter>

class QSplitterHandle;
class QWidget;

namespace speak5220::app {

// A QSplitter whose handles draw a short bar across their middle, in the
// palette's text colour, or its highlight while the pointer is over them.
// The macOS style draws a splitter handle as a faint dot, which cannot be
// seen on a dark window; this grip looks the same in every style. GUI
// thread only.
class GripSplitter : public QSplitter {
 public:
  explicit GripSplitter(Qt::Orientation orientation, QWidget* parent = nullptr);

 protected:
  QSplitterHandle* createHandle() override;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_MAIN_WINDOW_GRIP_SPLITTER_H_
