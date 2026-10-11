/*
 * File:        status_text.cpp
 * Module:      main_window
 * Purpose:     The status bar's summary of a project's size in KB and in
 *              PHROMs
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/status_text.h"

#include <QString>
#include <QStringList>

#include "core/project/phrom_allocator.h"

namespace speak5220::app {

namespace project = core::project;

namespace {

// The status bar's kilobyte: a PHROM is 16 KB.
constexpr double kBytesPerKb = 1024.0;

QString Kb(int bytes) {
  return QString::number(bytes / kBytesPerKb, 'f', 1) + QStringLiteral(" KB");
}

// "1 phrase", "2 phrases".
QString Phrases(int count) {
  return count == 1 ? QStringLiteral("1 phrase")
                    : QStringLiteral("%1 phrases").arg(count);
}

}  // namespace

QString ProjectSizeText(const project::PhromAllocation& allocation) {
  QStringList parts;
  const int phroms = allocation.PhromsUsed();
  if (phroms == 0) {
    parts << QStringLiteral("No PHROMs in use");
  } else {
    parts << Kb(allocation.TotalBytes());
    parts << QStringLiteral("%1 %2 of %3")
                 .arg(phroms)
                 .arg(phroms == 1 ? QStringLiteral("PHROM")
                                  : QStringLiteral("PHROMs"))
                 .arg(project::kPhromBudget);
    parts << QStringLiteral("%1 free in PHROM %2")
                 .arg(Kb(allocation.FreeInLastPhrom()))
                 .arg(phroms);
    if (!allocation.WithinBudget()) {
      parts << QStringLiteral("over budget");
    }
  }

  int too_large = 0;
  int not_encoded = 0;
  for (const project::PhraseSlot& slot : allocation.phrases) {
    if (slot.placement == project::Placement::kTooLarge) {
      ++too_large;
    } else if (slot.placement == project::Placement::kNotEncoded) {
      ++not_encoded;
    }
  }
  if (too_large > 0) {
    parts << Phrases(too_large) + QStringLiteral(" too large for a PHROM");
  }
  if (not_encoded > 0) {
    parts << Phrases(not_encoded) + QStringLiteral(" not encoded");
  }
  return parts.join(QStringLiteral(" · "));
}

}  // namespace speak5220::app
