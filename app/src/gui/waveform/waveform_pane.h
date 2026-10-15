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

class QLabel;

namespace speak5220::app {

class WaveformView;

// The waveform of one of the phrase editor's tabs: the title and
// a note above the view. Playback, zoom, scroll and the play start are
// controlled under it (PhraseWaveforms). GUI thread only.
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

 private:
  QLabel* title_;
  QLabel* note_;
  WaveformView* view_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_WAVEFORM_WAVEFORM_PANE_H_
