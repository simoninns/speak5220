/*
 * File:        phrase_list_panel.cpp
 * Module:      phrase_list
 * Purpose:     The phrase list's view and its toolbar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/phrase_list/phrase_list_panel.h"

#include <QAbstractItemView>
#include <QAction>
#include <QHBoxLayout>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QList>
#include <QListView>
#include <QModelIndex>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <optional>
#include <vector>

#include "core/project/phrase.h"
#include "gui/phrase_list/phrase_list_delegate.h"
#include "gui/phrase_list/phrase_list_model.h"

namespace speak5220::app {

namespace project = core::project;

PhraseListPanel::PhraseListPanel(PhraseListModel& model,
                                 const QList<QAction*>& actions,
                                 QWidget* parent)
    : QWidget(parent), model_(&model), view_(new QListView(this)) {
  auto* buttons = new QHBoxLayout();
  buttons->setContentsMargins(0, 0, 0, 0);
  for (QAction* action : actions) {
    auto* button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    // The button shows a symbol (+, an arrow); a screen reader reads what
    // it does.
    button->setAccessibleName(action->toolTip());
    buttons->addWidget(button);
  }
  buttons->addStretch();

  view_->setAccessibleName(QStringLiteral("Phrases"));
  view_->setModel(model_);
  view_->setItemDelegate(new PhraseListDelegate(view_));
  view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  view_->setDragDropMode(QAbstractItemView::DragDrop);
  view_->setDefaultDropAction(Qt::MoveAction);
  view_->setDragEnabled(true);
  view_->setAcceptDrops(true);
  view_->setDropIndicatorShown(true);
  view_->setUniformItemSizes(false);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addLayout(buttons);
  layout->addWidget(view_);

  connect(model_, &PhraseListModel::modelAboutToBeReset, this,
          &PhraseListPanel::RememberSelection);
  connect(model_, &PhraseListModel::modelReset, this,
          &PhraseListPanel::RestoreSelection);
  connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &PhraseListPanel::SelectionChanged);
  connect(view_->selectionModel(), &QItemSelectionModel::currentChanged, this,
          &PhraseListPanel::SelectionChanged);
}

QList<int> PhraseListPanel::SelectedRows() const {
  QList<int> rows;
  for (const QModelIndex& index : view_->selectionModel()->selectedIndexes()) {
    rows.append(index.row());
  }
  std::ranges::sort(rows);
  return rows;
}

std::vector<project::PhraseId> PhraseListPanel::SelectedIds() const {
  return model_->IdsAt(SelectedRows());
}

project::PhraseId PhraseListPanel::SingleSelectedId() const {
  const QList<int> rows = SelectedRows();
  if (rows.size() != 1) {
    return {};
  }
  return model_->IdAt(rows.front());
}

void PhraseListPanel::Select(const std::vector<project::PhraseId>& ids) {
  QItemSelection selection;
  std::optional<QModelIndex> first;
  for (const project::PhraseId& id : ids) {
    if (const std::optional<int> row = model_->RowOf(id)) {
      const QModelIndex index = model_->index(*row);
      selection.select(index, index);
      if (!first) {
        first = index;
      }
    }
  }
  QItemSelectionModel* selection_model = view_->selectionModel();
  if (first) {
    selection_model->setCurrentIndex(*first, QItemSelectionModel::NoUpdate);
    view_->scrollTo(*first);
  }
  selection_model->select(selection, QItemSelectionModel::ClearAndSelect);
  emit SelectionChanged();
}

void PhraseListPanel::RememberSelection() { remembered_ = SelectedIds(); }

void PhraseListPanel::RestoreSelection() {
  Select(remembered_);
  remembered_.clear();
}

}  // namespace speak5220::app
