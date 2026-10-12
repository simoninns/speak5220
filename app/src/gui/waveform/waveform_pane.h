/*
 * File:        waveform_pane.h
 * Module:      waveform
 * Purpose:     One waveform pane: its title and note above the view
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_WAVEFORM_WAVEFORM_PANE_H_
#define SPEAK5220_GUI_WAVEFORM_WAVEFORM_PANE_H_

#include <QString>
#include <QWidget>

class QHBoxLayout;
class QLabel;

namespace speak5220::app {

class WaveformView;

// Plan §5: a pane of the phrase editor: the title and a note above the
// view. Playback, zoom, scroll and the play start are controlled under
// both panes (PhraseWaveforms). GUI thread only.
class WaveformPane : public QWidget {
  Q_OBJECT

 public:
  explicit WaveformPane(const QString& title, QWidget* parent = nullptr);

  WaveformView* View() const { return view_; }

  // The title, which also names the view for accessibility.
  void SetTitle(const QString& title);
  QString Title() const;
  // A note beside the title, such as "stale"; empty for none.
  void SetNote(const QString& note);
  QString Note() const;
  // Puts `widget` at the right of the heading, owned by the pane.
  void AddHeadingWidget(QWidget* widget);
  // Makes the heading at least `height` pixels high, so this pane's view
  // can be the same height as one whose heading holds taller widgets.
  void SetMinimumHeadingHeight(int height);

 private:
  QLabel* title_;
  QLabel* note_;
  QHBoxLayout* heading_;
  WaveformView* view_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_WAVEFORM_WAVEFORM_PANE_H_
