/*
 * File:        phrase_list_panel.h
 * Module:      phrase_list
 * Purpose:     The phrase list's view and its toolbar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_PHRASE_LIST_PHRASE_LIST_PANEL_H_
#define SPEAK5220_GUI_PHRASE_LIST_PHRASE_LIST_PANEL_H_

#include <QList>
#include <QWidget>
#include <vector>

#include "core/project/phrase.h"

class QAction;
class QListView;

namespace speak5220::app {

class PhraseListModel;

// The phrase list, a QListView over a PhraseListModel with
// multi-selection and drag-and-drop, under a row of buttons for the given
// actions (add, remove, up, down, A-Z). The selection is kept by phrase
// id across the model's resets, so it follows phrases that move.
// GUI thread only; the model must outlive the panel.
class PhraseListPanel : public QWidget {
  Q_OBJECT

 public:
  PhraseListPanel(PhraseListModel& model, const QList<QAction*>& actions,
                  QWidget* parent = nullptr);

  QListView* View() const { return view_; }

  // The selected rows, ascending.
  QList<int> SelectedRows() const;
  // The selected phrases, in list order.
  std::vector<core::project::PhraseId> SelectedIds() const;
  // The current phrase when exactly one is selected; empty otherwise.
  core::project::PhraseId SingleSelectedId() const;
  // Selects these phrases (those still in the list), making the first
  // current.
  void Select(const std::vector<core::project::PhraseId>& ids);

 signals:
  // The selection changed, by the user or by a reset.
  void SelectionChanged();

 private:
  void RememberSelection();
  void RestoreSelection();

  PhraseListModel* model_;
  QListView* view_;
  std::vector<core::project::PhraseId> remembered_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_PHRASE_LIST_PHRASE_LIST_PANEL_H_
