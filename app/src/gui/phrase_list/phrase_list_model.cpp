/*
 * File:        phrase_list_model.cpp
 * Module:      phrase_list
 * Purpose:     The phrase list: one row per phrase in PHROM order, with
 *              its state, size and PHROM, and the edits made from the list
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/phrase_list/phrase_list_model.h"

#include <QAbstractListModel>
#include <QByteArray>
#include <QList>
#include <QMimeData>
#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/project/phrom_allocator.h"
#include "core/project/project.h"
#include "core/util/status.h"
#include "gui/undo/project_edits.h"

namespace speak5220::app {

namespace project = core::project;

namespace {

// Separates the parts of a staleness key; it occurs in no option text.
constexpr char kKeySeparator = '\n';

// The default options as text, one per line in EncoderOptionInfos() order:
// part of every phrase's staleness key.
std::string DefaultsKey(const core::encoder::EncoderOptions& defaults) {
  std::string key;
  for (const core::encoder::EncoderOptionInfo& info :
       core::encoder::EncoderOptionInfos()) {
    key += core::encoder::EncoderOptionText(defaults, info.field);
    key += kKeySeparator;
  }
  return key;
}

// The rows, ascending and without repeats, that are in [0, count).
QList<int> ValidRows(QList<int> rows, int count) {
  std::ranges::sort(rows);
  const auto repeats = std::ranges::unique(rows);
  rows.erase(repeats.begin(), repeats.end());
  rows.removeIf([count](int row) { return row < 0 || row >= count; });
  return rows;
}

QString StateText(EncodingState state) {
  switch (state) {
    case EncodingState::kNotEncoded:
      return QStringLiteral("not encoded");
    case EncodingState::kStale:
      return QStringLiteral("stale: its settings have changed");
    case EncodingState::kCurrent:
      return QStringLiteral("encoded");
    case EncodingState::kLocked:
      return QStringLiteral("locked");
  }
  return {};
}

}  // namespace

PhraseListModel::PhraseListModel(ProjectEdits& edits, QObject* parent)
    : QAbstractListModel(parent), edits_(&edits) {}

void PhraseListModel::SetProject(const project::Project& project,
                                 const project::Project* saved) {
  const project::PhromAllocation allocation = project.Allocation();
  const std::string defaults_key =
      DefaultsKey(project.Settings().default_encoder);
  const std::vector<project::Phrase>& phrases = project.Phrases();

  std::vector<Row> rows;
  rows.reserve(phrases.size());
  std::map<project::PhraseId, CurrentCheck> kept_checks;
  for (std::size_t i = 0; i < phrases.size(); ++i) {
    const project::Phrase& phrase = phrases[i];
    Row row;
    row.id = phrase.id;
    row.name = QString::fromStdString(phrase.name);
    row.state = StateOf(phrase, project, defaults_key);
    row.slot = allocation.phrases[i];
    if (saved != nullptr) {
      const project::Phrase* before = saved->FindPhrase(phrase.id);
      row.modified = before == nullptr || *before != phrase;
    }
    rows.push_back(std::move(row));
    if (const auto check = current_checks_.find(phrase.id);
        check != current_checks_.end()) {
      kept_checks.insert(*check);
    }
  }
  // A divider above the first phrase of each PHROM after
  // the first, at the allocator's boundaries.
  for (std::size_t phrom = 1; phrom < allocation.phroms.size(); ++phrom) {
    const std::vector<int>& members = allocation.phroms[phrom].phrases;
    if (!members.empty()) {
      rows[static_cast<std::size_t>(members.front())].phrom_start =
          static_cast<int>(phrom) + 1;
    }
  }

  beginResetModel();
  rows_ = std::move(rows);
  current_checks_ = std::move(kept_checks);
  endResetModel();
}

EncodingState PhraseListModel::StateOf(const project::Phrase& phrase,
                                       const project::Project& project,
                                       const std::string& defaults_key) {
  if (phrase.locked) {
    return EncodingState::kLocked;
  }
  if (!phrase.encoding.has_value()) {
    return EncodingState::kNotEncoded;
  }
  // The encoding is current when its hash matches the phrase's
  // inputs. A phrase's audio never changes once imported, so the inputs
  // that can change are the options and the stored encoding.
  std::string key = defaults_key;
  for (const auto& [field, text] : phrase.encoder) {
    key += field;
    key += '=';
    key += text;
    key += kKeySeparator;
  }
  key += phrase.encoding->encoder_version + kKeySeparator +
         phrase.encoding->settings_hash + kKeySeparator +
         std::to_string(phrase.audio.size());
  CurrentCheck& check = current_checks_[phrase.id];
  if (check.key != key) {
    const core::StatusOr<bool> current = project::EncodingCache::IsCurrent(
        phrase, project.Settings().default_encoder);
    check.key = std::move(key);
    check.current = current.IsOk() && *current;
  }
  return check.current ? EncodingState::kCurrent : EncodingState::kStale;
}

project::PhraseId PhraseListModel::IdAt(int row) const {
  if (row < 0 || row >= PhraseCount()) {
    return {};
  }
  return rows_[static_cast<std::size_t>(row)].id;
}

std::vector<project::PhraseId> PhraseListModel::IdsAt(
    const QList<int>& rows) const {
  std::vector<project::PhraseId> ids;
  for (const int row : ValidRows(rows, PhraseCount())) {
    ids.push_back(rows_[static_cast<std::size_t>(row)].id);
  }
  return ids;
}

std::optional<int> PhraseListModel::RowOf(const project::PhraseId& id) const {
  for (std::size_t i = 0; i < rows_.size(); ++i) {
    if (rows_[i].id == id) {
      return static_cast<int>(i);
    }
  }
  return std::nullopt;
}

bool PhraseListModel::RemoveAt(const QList<int>& rows) {
  const std::vector<project::PhraseId> ids = IdsAt(rows);
  if (ids.empty()) {
    return true;
  }
  return Reported(edits_->RemovePhrases(ids));
}

bool PhraseListModel::CanMoveUp(const QList<int>& rows) const {
  const QList<int> valid = ValidRows(rows, PhraseCount());
  return !valid.isEmpty() && valid.front() > 0;
}

bool PhraseListModel::CanMoveDown(const QList<int>& rows) const {
  const QList<int> valid = ValidRows(rows, PhraseCount());
  return !valid.isEmpty() && valid.back() < PhraseCount() - 1;
}

bool PhraseListModel::MoveUp(const QList<int>& rows) {
  if (!CanMoveUp(rows)) {
    return true;
  }
  const QList<int> valid = ValidRows(rows, PhraseCount());
  return Reported(edits_->MovePhrases(IdsAt(valid), valid.front() - 1));
}

bool PhraseListModel::MoveDown(const QList<int>& rows) {
  if (!CanMoveDown(rows)) {
    return true;
  }
  const QList<int> valid = ValidRows(rows, PhraseCount());
  // MovePhrases() places them before the phrase at the destination: the
  // one after the phrase below the last of them.
  return Reported(edits_->MovePhrases(IdsAt(valid), valid.back() + 2));
}

bool PhraseListModel::Sort() {
  if (PhraseCount() < 2) {
    return true;
  }
  return Reported(edits_->SortPhrases());
}

int PhraseListModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant PhraseListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() >= PhraseCount()) {
    return {};
  }
  const Row& row = rows_[static_cast<std::size_t>(index.row())];
  const bool placed = row.slot.placement == project::Placement::kPlaced;
  switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
      return row.name;
    case Qt::ToolTipRole:
      return ToolTip(row);
    case kIdRole:
      return QString::fromStdString(row.id);
    case kStateRole:
      return static_cast<int>(row.state);
    case kPlacementRole:
      return static_cast<int>(row.slot.placement);
    case kSizeRole:
      if (row.slot.placement == project::Placement::kNotEncoded) {
        return {};
      }
      return row.slot.cost;
    case kPhromRole:
      return placed ? row.slot.phrom : 0;
    case kWordNumberRole:
      return placed ? row.slot.word_number : 0;
    case kPhromStartRole:
      return row.phrom_start;
    case kModifiedRole:
      return row.modified;
    default:
      return {};
  }
}

QString PhraseListModel::ToolTip(const Row& row) const {
  QStringList parts;
  switch (row.slot.placement) {
    case project::Placement::kPlaced:
      parts << QStringLiteral("PHROM %1, word %2, %3 bytes")
                   .arg(row.slot.phrom)
                   .arg(row.slot.word_number)
                   .arg(row.slot.cost);
      break;
    case project::Placement::kTooLarge:
      parts << QStringLiteral(
                   "%1 bytes: too large for a PHROM (%2 bytes); shorten or "
                   "re-encode it")
                   .arg(row.slot.cost)
                   .arg(project::kPhromWordCapacity);
      break;
    case project::Placement::kNotEncoded:
      break;
  }
  parts << StateText(row.state);
  if (row.modified) {
    parts << QStringLiteral("changed since last saved");
  }
  return parts.join(QStringLiteral("; "));
}

Qt::ItemFlags PhraseListModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) {
    // Drops land between rows, never on one.
    return Qt::ItemIsDropEnabled;
  }
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

QStringList PhraseListModel::mimeTypes() const {
  return {QString::fromLatin1(kPhraseIdsMimeType),
          QStringLiteral("text/uri-list")};
}

QMimeData* PhraseListModel::mimeData(const QModelIndexList& indexes) const {
  QList<int> rows;
  for (const QModelIndex& index : indexes) {
    rows.append(index.row());
  }
  QStringList ids;
  for (const project::PhraseId& id : IdsAt(rows)) {
    ids << QString::fromStdString(id);
  }
  auto* data = new QMimeData();
  data->setData(QString::fromLatin1(kPhraseIdsMimeType),
                ids.join(QLatin1Char('\n')).toUtf8());
  return data;
}

Qt::DropActions PhraseListModel::supportedDragActions() const {
  return Qt::MoveAction;
}

Qt::DropActions PhraseListModel::supportedDropActions() const {
  return Qt::MoveAction | Qt::CopyAction;
}

bool PhraseListModel::canDropMimeData(const QMimeData* data,
                                      Qt::DropAction action, int /*row*/,
                                      int /*column*/,
                                      const QModelIndex& /*parent*/) const {
  if (data == nullptr) {
    return false;
  }
  if (data->hasFormat(QString::fromLatin1(kPhraseIdsMimeType))) {
    return action == Qt::MoveAction;
  }
  const QList<QUrl> urls = data->urls();
  return std::ranges::any_of(urls,
                             [](const QUrl& url) { return url.isLocalFile(); });
}

bool PhraseListModel::dropMimeData(const QMimeData* data, Qt::DropAction action,
                                   int row, int column,
                                   const QModelIndex& parent) {
  if (!canDropMimeData(data, action, row, column, parent)) {
    return false;
  }
  const int destination = DropRow(row, parent);
  const QString ids_type = QString::fromLatin1(kPhraseIdsMimeType);
  if (data->hasFormat(ids_type)) {
    const QStringList texts =
        QString::fromUtf8(data->data(ids_type)).split(QLatin1Char('\n'));
    std::vector<project::PhraseId> ids;
    QList<int> rows;
    for (const QString& text : texts) {
      ids.push_back(text.toStdString());
      if (const std::optional<int> from = RowOf(ids.back())) {
        rows.append(*from);
      }
    }
    // Dropping a block of rows inside or next to itself changes nothing,
    // so nothing goes on the undo stack.
    const QList<int> sorted = ValidRows(rows, PhraseCount());
    const bool contiguous = !sorted.isEmpty() &&
                            std::cmp_equal(sorted.size(), ids.size()) &&
                            sorted.back() - sorted.front() + 1 == sorted.size();
    if (contiguous && destination >= sorted.front() &&
        destination <= sorted.back() + 1) {
      return false;
    }
    // The rows are moved here, so the view must not remove them as well:
    // the model does not implement removeRows() or moveRows(), whose
    // defaults do nothing.
    return Reported(edits_->MovePhrases(ids, destination));
  }
  QStringList paths;
  for (const QUrl& url : data->urls()) {
    if (url.isLocalFile()) {
      paths << url.toLocalFile();
    }
  }
  emit FilesDropped(paths, destination);
  return true;
}

int PhraseListModel::DropRow(int row, const QModelIndex& parent) const {
  if (row >= 0 && row <= PhraseCount()) {
    return row;
  }
  if (parent.isValid()) {
    return parent.row();
  }
  return PhraseCount();
}

bool PhraseListModel::Reported(const core::Status& status) {
  if (!status.IsOk()) {
    emit EditFailed(QString::fromStdString(status.Message()));
  }
  return status.IsOk();
}

}  // namespace speak5220::app
