/*
 * File:        phrase_list_model.h
 * Module:      phrase_list
 * Purpose:     The phrase list: one row per phrase in PHROM order, with
 *              its state, size and PHROM, and the edits made from the list
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_PHRASE_LIST_PHRASE_LIST_MODEL_H_
#define SPEAK5220_GUI_PHRASE_LIST_PHRASE_LIST_MODEL_H_

#include <QAbstractListModel>
#include <QList>
#include <QMimeData>
#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/project/phrase.h"
#include "core/project/phrom_allocator.h"
#include "core/project/project.h"
#include "core/util/status.h"
#include "gui/undo/project_edits.h"

namespace speak5220::app {

// Whether a phrase's frames are up to date (plan §3.4, §5).
enum class EncodingState {
  kNotEncoded,  // no frames yet
  kStale,       // frames made from other audio, settings or encoder
  kCurrent,     // frames match the phrase's inputs
  kLocked,      // frames are the phrase's content and never redone
};

// Plan §5: the model behind the phrase list. Rows are the project's
// phrases in list order, which is PHROM word order. The model shows a
// snapshot of the project (SetProject()), and makes its edits through
// ProjectEdits, so each is one undoable command; the document then sends
// the changed project back.
//
// Dragging rows moves them (one MovePhrases()); dropping files emits
// FilesDropped() for the window to import. Rows that start PHROM 2, 3, ...
// carry kPhromStartRole, from which the delegate draws the dividers.
//
// GUI thread only. The edits object must outlive the model.
class PhraseListModel : public QAbstractListModel {
  Q_OBJECT

 public:
  // The MIME type of dragged rows: their phrase ids, one per line.
  static constexpr const char* kPhraseIdsMimeType =
      "application/x-speak5220-phrase-ids";

  // The model's item data roles. The phrase's id (QString).
  static constexpr int kIdRole = Qt::UserRole + 1;
  // EncodingState, as int.
  static constexpr int kStateRole = Qt::UserRole + 2;
  // core::project::Placement, as int.
  static constexpr int kPlacementRole = Qt::UserRole + 3;
  // Its cost in its PHROM in bytes (int); null when not encoded.
  static constexpr int kSizeRole = Qt::UserRole + 4;
  // The PHROM it is in, from 1, and its word number there (int); 0 when
  // in no PHROM.
  static constexpr int kPhromRole = Qt::UserRole + 5;
  static constexpr int kWordNumberRole = Qt::UserRole + 6;
  // The PHROM this row starts, for PHROM 2 onwards (int); 0 otherwise.
  static constexpr int kPhromStartRole = Qt::UserRole + 7;

  explicit PhraseListModel(ProjectEdits& edits, QObject* parent = nullptr);

  // Shows `project`'s phrases and allocation, replacing the rows (a model
  // reset). Whether each encoding is current is remembered per phrase,
  // keyed by its settings and stored hash, so an edit elsewhere does not
  // hash every phrase's audio again.
  void SetProject(const core::project::Project& project);

  // The number of rows: rowCount() of the root.
  int PhraseCount() const { return static_cast<int>(rows_.size()); }

  // The phrase at `row`, the phrases at `rows` in list order, and the row
  // of a phrase.
  core::project::PhraseId IdAt(int row) const;
  std::vector<core::project::PhraseId> IdsAt(const QList<int>& rows) const;
  std::optional<int> RowOf(const core::project::PhraseId& id) const;

  // The edits below return whether they succeeded; a failure is also
  // reported by EditFailed(). An edit that would change nothing succeeds
  // without making an undo step.

  // Removes the phrases at `rows` (the caller confirms first).
  bool RemoveAt(const QList<int>& rows);
  // Moves the phrases at `rows`, together and in order, to just before the
  // row above the first of them, or to just after the row below the last.
  bool CanMoveUp(const QList<int>& rows) const;
  bool CanMoveDown(const QList<int>& rows) const;
  bool MoveUp(const QList<int>& rows);
  bool MoveDown(const QList<int>& rows);
  // Sorts the phrases A to Z.
  bool Sort();

  // QAbstractListModel.
  int rowCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  Qt::DropActions supportedDragActions() const override;
  Qt::DropActions supportedDropActions() const override;
  bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                       int column, const QModelIndex& parent) const override;
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                    int column, const QModelIndex& parent) override;

 signals:
  // An edit made from the list failed; nothing changed.
  void EditFailed(const QString& message);
  // Files were dropped on the list, to be imported before `row`.
  void FilesDropped(const QStringList& paths, int row);

 private:
  struct Row {
    core::project::PhraseId id;
    QString name;
    EncodingState state = EncodingState::kNotEncoded;
    core::project::PhraseSlot slot;
    int phrom_start = 0;
  };

  // A remembered staleness check: the inputs it was made from, and the
  // answer.
  struct CurrentCheck {
    std::string key;
    bool current = false;
  };

  EncodingState StateOf(const core::project::Phrase& phrase,
                        const core::project::Project& project,
                        const std::string& defaults_key);
  QString ToolTip(const Row& row) const;
  // Emits EditFailed() for a failure; returns whether `status` is OK.
  bool Reported(const core::Status& status);
  // The row a drop at (row, parent) lands before.
  int DropRow(int row, const QModelIndex& parent) const;

  ProjectEdits* edits_;
  std::vector<Row> rows_;
  std::map<core::project::PhraseId, CurrentCheck> current_checks_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_PHRASE_LIST_PHRASE_LIST_MODEL_H_
