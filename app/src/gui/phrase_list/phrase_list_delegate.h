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

#include <QColor>
#include <QModelIndex>
#include <QPalette>
#include <QSize>
#include <QString>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

class QPainter;

namespace speak5220::app {

// Each row shows the phrase's name, its state (stale, not
// encoded, locked) and its size in bytes; the name of a phrase changed
// since the project was last saved is marked, as the window title marks
// a modified project; the name of a phrase too large for a PHROM is in
// the theme's warning colour; a row with
// PhraseListModel::kPhromStartRole has a "PHROM N" divider above it.
// GUI thread only.
class PhraseListDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  // The name drawn for a row: the phrase's name, with " *" after it when
  // PhraseListModel::kModifiedRole is set.
  static QString NameText(const QModelIndex& index);
  // The text drawn at the right of a row: "312 B", "312 B · stale",
  // "not encoded", "1.2 KB · locked".
  static QString DetailText(const QModelIndex& index);
  // The colour of a row's name on `palette`: the theme's warning colour
  // (ColoursFor()) for a phrase too large for a PHROM, otherwise the
  // palette's text.
  static QColor NameColour(const QPalette& palette, const QModelIndex& index);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;

 protected:
  // Draws NameText() as the row's text.
  void initStyleOption(QStyleOptionViewItem* option,
                       const QModelIndex& index) const override;

 private:
  static int DividerHeight(const QStyleOptionViewItem& option,
                           const QModelIndex& index);
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_PHRASE_LIST_PHRASE_LIST_DELEGATE_H_
