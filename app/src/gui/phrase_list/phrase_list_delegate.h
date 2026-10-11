/*
 * File:        phrase_list_delegate.h
 * Module:      phrase_list
 * Purpose:     Draws a phrase list row: the name, its state and size, and
 *              the divider where a new PHROM starts
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_PHRASE_LIST_PHRASE_LIST_DELEGATE_H_
#define SPEAK5220_GUI_PHRASE_LIST_PHRASE_LIST_DELEGATE_H_

#include <QModelIndex>
#include <QSize>
#include <QString>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

class QPainter;

namespace speak5220::app {

// Plan §5: each row shows the phrase's name, its state (stale, not
// encoded, locked) and its size in bytes; a row with
// PhraseListModel::kPhromStartRole has a "PHROM N" divider above it.
// GUI thread only.
class PhraseListDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  // The text drawn at the right of a row: "312 B", "312 B · stale",
  // "not encoded", "1.2 KB · locked".
  static QString DetailText(const QModelIndex& index);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;

 private:
  static int DividerHeight(const QStyleOptionViewItem& option,
                           const QModelIndex& index);
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_PHRASE_LIST_PHRASE_LIST_DELEGATE_H_
