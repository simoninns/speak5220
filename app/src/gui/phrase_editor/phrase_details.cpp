/*
 * File:        phrase_details.cpp
 * Module:      phrase_editor
 * Purpose:     The phrase's name and description fields, with the name
 *              rules applied as the user types
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/phrase_editor/phrase_details.h"

#include <QChar>
#include <QColor>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QString>
#include <QWidget>
#include <optional>
#include <string>

#include "core/project/phrase.h"
#include "core/project/phrase_name.h"
#include "core/util/status.h"
#include "gui/theme/colours.h"
#include "gui/undo/project_edits.h"

namespace speak5220::app {

namespace project = core::project;

namespace {

// How wide the name field is, in characters (names have at most 16).
constexpr int kNameFieldCharacters = 20;

}  // namespace

NameEdit FilterNameEdit(const QString& text) {
  QString result;
  result.reserve(text.size());
  for (const QChar c : text) {
    QChar upper = c;
    if (c >= QLatin1Char('a') && c <= QLatin1Char('z')) {
      upper = QChar(c.unicode() - 'a' + 'A');
    }
    if (upper.unicode() > 0x7F ||
        !project::IsPhraseNameCharacter(static_cast<char>(upper.unicode()))) {
      // The same words as NormalisePhraseName(), with the character shown
      // as typed.
      return {.text = std::nullopt,
              .refusal = QStringLiteral(
                             "a phrase name cannot contain \"%1\": use A-Z, "
                             "0-9, space and ' ( ) , - .")
                             .arg(c)};
    }
    result.append(upper);
  }
  return {.text = result, .refusal = QString()};
}

PhraseDetails::PhraseDetails(ProjectEdits& edits, QWidget* parent)
    : QWidget(parent),
      edits_(&edits),
      name_(new QLineEdit(this)),
      description_(new QLineEdit(this)),
      message_(new QLabel(this)) {
  name_->setMaxLength(project::kMaxPhraseNameLength);
  name_->setMinimumWidth(name_->fontMetrics().averageCharWidth() *
                         kNameFieldCharacters);
  name_->setPlaceholderText(QStringLiteral("NAME"));
  description_->setPlaceholderText(
      QStringLiteral("A note about the phrase, kept in the project only"));
  message_->setWordWrap(true);
  UpdateColours();

  auto* name_label = new QLabel(QStringLiteral("&Name"), this);
  name_label->setBuddy(name_);
  auto* description_label = new QLabel(QStringLiteral("&Description"), this);
  description_label->setBuddy(description_);

  auto* layout = new QGridLayout(this);
  layout->addWidget(name_label, 0, 0);
  layout->addWidget(name_, 0, 1);
  layout->addWidget(description_label, 0, 2);
  layout->addWidget(description_, 0, 3);
  layout->addWidget(message_, 1, 1, 1, 3);
  layout->setColumnStretch(3, 1);

  connect(name_, &QLineEdit::textEdited, this, &PhraseDetails::OnNameEdited);
  connect(name_, &QLineEdit::editingFinished, this, &PhraseDetails::CommitName);
  connect(description_, &QLineEdit::editingFinished, this,
          &PhraseDetails::CommitDescription);
  ShowPhrase(nullptr);
}

void PhraseDetails::ShowPhrase(const project::Phrase* phrase) {
  if (phrase == nullptr) {
    // Cleared first, so the editingFinished of a field losing focus as it
    // is disabled applies to no phrase.
    id_.clear();
    name_shown_.clear();
    description_shown_.clear();
    accepted_name_.clear();
    name_->clear();
    description_->clear();
    name_->setEnabled(false);
    description_->setEnabled(false);
    ShowMessage(QString());
    return;
  }
  const bool same_phrase = phrase->id == id_;
  if (!same_phrase) {
    ShowMessage(QString());
  }
  id_ = phrase->id;
  name_shown_ = QString::fromStdString(phrase->name);
  description_shown_ = QString::fromStdString(phrase->description);
  name_->setEnabled(true);
  description_->setEnabled(true);
  if (!same_phrase || !name_->hasFocus()) {
    name_->setText(name_shown_);
  }
  if (!same_phrase || !description_->hasFocus()) {
    description_->setText(description_shown_);
  }
  accepted_name_ = name_->text();
}

QString PhraseDetails::Message() const { return message_->text(); }

QColor PhraseDetails::MessageColour() const {
  return message_->palette().color(QPalette::WindowText);
}

void PhraseDetails::changeEvent(QEvent* event) {
  // Plan §5, "Colours": a change of scheme reaches the widget as a palette
  // change.
  if (event->type() == QEvent::PaletteChange) {
    UpdateColours();
  }
  QWidget::changeEvent(event);
}

void PhraseDetails::UpdateColours() {
  QPalette message_palette = message_->palette();
  message_palette.setColor(QPalette::WindowText, ColoursFor(palette()).warning);
  message_->setPalette(message_palette);
}

void PhraseDetails::OnNameEdited(const QString& text) {
  const NameEdit edit = FilterNameEdit(text);
  if (!edit.text.has_value()) {
    // Decision D2: the refused character is not taken; the name stays as
    // it was, with the cursor where it was before the keystroke.
    const int cursor = name_->cursorPosition() -
                       static_cast<int>(text.size() - accepted_name_.size());
    name_->setText(accepted_name_);
    name_->setCursorPosition(cursor);
    ShowMessage(edit.refusal);
    return;
  }
  if (*edit.text != text) {
    const int cursor = name_->cursorPosition();
    name_->setText(*edit.text);
    name_->setCursorPosition(cursor);
  }
  accepted_name_ = *edit.text;
  ShowMessage(QString());
}

void PhraseDetails::CommitName() {
  if (id_.empty() || name_->text() == name_shown_) {
    return;
  }
  const std::string typed = name_->text().toStdString();
  // A name that normalises to the current one ("HELLO " for "HELLO")
  // changes nothing, so it makes no undo step.
  const core::StatusOr<std::string> normalised =
      project::NormalisePhraseName(typed);
  if (normalised.IsOk() && QString::fromStdString(*normalised) == name_shown_) {
    name_->setText(name_shown_);
    accepted_name_ = name_shown_;
    return;
  }
  if (const core::Status status = edits_->RenamePhrase(id_, typed);
      !status.IsOk()) {
    ShowMessage(QString::fromStdString(status.Message()));
    name_->setText(name_shown_);
    accepted_name_ = name_shown_;
  }
}

void PhraseDetails::CommitDescription() {
  if (id_.empty() || description_->text() == description_shown_) {
    return;
  }
  if (const core::Status status =
          edits_->SetDescription(id_, description_->text().toStdString());
      !status.IsOk()) {
    ShowMessage(QString::fromStdString(status.Message()));
    description_->setText(description_shown_);
  }
}

void PhraseDetails::ShowMessage(const QString& message) {
  message_->setText(message);
}

}  // namespace speak5220::app
