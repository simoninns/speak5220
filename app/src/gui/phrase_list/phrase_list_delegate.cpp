/*
 * File:        phrase_list_delegate.cpp
 * Module:      phrase_list
 * Purpose:     Draws a phrase list row: the name, its state and size, and
 *              the divider where a new PHROM starts
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/phrase_list/phrase_list_delegate.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QVariant>

#include "core/project/phrom_allocator.h"
#include "gui/phrase_list/phrase_list_model.h"
#include "gui/theme/colours.h"

namespace speak5220::app {

namespace {

// Space around the divider's label and the detail text, in pixels.
constexpr int kDividerPadding = 2;
constexpr int kDetailMargin = 6;

}  // namespace

QString PhraseListDelegate::NameText(const QModelIndex& index) {
  QString name = index.data(Qt::DisplayRole).toString();
  if (index.data(PhraseListModel::kModifiedRole).toBool()) {
    name += QStringLiteral(" *");
  }
  return name;
}

void PhraseListDelegate::initStyleOption(QStyleOptionViewItem* option,
                                         const QModelIndex& index) const {
  QStyledItemDelegate::initStyleOption(option, index);
  option->text = NameText(index);
}

QString PhraseListDelegate::DetailText(const QModelIndex& index) {
  QStringList parts;
  const QVariant size = index.data(PhraseListModel::kSizeRole);
  if (size.isValid()) {
    parts << QStringLiteral("%1 B").arg(size.toInt());
  }
  switch (static_cast<EncodingState>(
      index.data(PhraseListModel::kStateRole).toInt())) {
    case EncodingState::kNotEncoded:
      parts << QStringLiteral("not encoded");
      break;
    case EncodingState::kStale:
      parts << QStringLiteral("stale");
      break;
    case EncodingState::kLocked:
      parts << QStringLiteral("locked");
      break;
    case EncodingState::kCurrent:
      break;
  }
  return parts.join(QStringLiteral(" · "));
}

QColor PhraseListDelegate::NameColour(const QPalette& palette,
                                      const QModelIndex& index) {
  if (static_cast<core::project::Placement>(
          index.data(PhraseListModel::kPlacementRole).toInt()) ==
      core::project::Placement::kTooLarge) {
    return ColoursFor(palette).warning;
  }
  return palette.color(QPalette::Text);
}

int PhraseListDelegate::DividerHeight(const QStyleOptionViewItem& option,
                                      const QModelIndex& index) {
  if (index.data(PhraseListModel::kPhromStartRole).toInt() == 0) {
    return 0;
  }
  return option.fontMetrics.height() + (2 * kDividerPadding);
}

void PhraseListDelegate::paint(QPainter* painter,
                               const QStyleOptionViewItem& option,
                               const QModelIndex& index) const {
  const int divider = DividerHeight(option, index);
  QStyleOptionViewItem item = option;
  item.rect.setTop(option.rect.top() + divider);
  item.palette.setColor(QPalette::Text, NameColour(option.palette, index));

  if (divider > 0) {
    // "PHROM N" where each PHROM after the first starts.
    const QRect band(option.rect.left(), option.rect.top(), option.rect.width(),
                     divider);
    painter->save();
    painter->fillRect(band, option.palette.alternateBase());
    painter->setPen(option.palette.color(QPalette::Mid));
    painter->drawLine(band.topLeft(), band.topRight());
    QFont font = option.font;
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(option.palette.color(QPalette::Text));
    painter->drawText(
        band.adjusted(kDetailMargin, 0, -kDetailMargin, 0),
        Qt::AlignLeft | Qt::AlignVCenter,
        QStringLiteral("PHROM %1")
            .arg(index.data(PhraseListModel::kPhromStartRole).toInt()));
    painter->restore();
  }

  QStyledItemDelegate::paint(painter, item, index);

  const QString detail = DetailText(index);
  if (!detail.isEmpty()) {
    painter->save();
    const bool selected = (option.state & QStyle::State_Selected) != 0;
    painter->setPen(option.palette.color(selected ? QPalette::HighlightedText
                                                  : QPalette::PlaceholderText));
    painter->setFont(option.font);
    painter->drawText(item.rect.adjusted(kDetailMargin, 0, -kDetailMargin, 0),
                      Qt::AlignRight | Qt::AlignVCenter, detail);
    painter->restore();
  }
}

QSize PhraseListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const {
  QSize size = QStyledItemDelegate::sizeHint(option, index);
  const QFontMetrics metrics(option.font);
  size.setWidth(size.width() + kDetailMargin +
                metrics.horizontalAdvance(DetailText(index)));
  size.setHeight(size.height() + DividerHeight(option, index));
  return size;
}

}  // namespace speak5220::app
