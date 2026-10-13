/*
 * File:        waveform_pane.cpp
 * Module:      waveform
 * Purpose:     One waveform pane: its title and note above the view
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/waveform_pane.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "gui/waveform/waveform_view.h"

namespace speak5220::app {

WaveformPane::WaveformPane(const QString& title, QWidget* parent)
    : QWidget(parent),
      title_(new QLabel(title, this)),
      note_(new QLabel(this)),
      view_(new WaveformView(this)) {
  QFont bold = title_->font();
  bold.setBold(true);
  title_->setFont(bold);
  note_->setForegroundRole(QPalette::PlaceholderText);
  view_->setAccessibleName(title);

  auto* heading = new QHBoxLayout;
  heading->addWidget(title_);
  heading->addWidget(note_);
  heading->addStretch();
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addLayout(heading);
  layout->addWidget(view_, 1);
}

void WaveformPane::SetTitle(const QString& title) {
  title_->setText(title);
  view_->setAccessibleName(title);
}

QString WaveformPane::Title() const { return title_->text(); }

void WaveformPane::SetNote(const QString& note) { note_->setText(note); }

QString WaveformPane::Note() const { return note_->text(); }

}  // namespace speak5220::app
