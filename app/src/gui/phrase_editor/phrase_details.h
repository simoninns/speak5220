/*
 * File:        phrase_details.h
 * Module:      phrase_editor
 * Purpose:     The phrase's name and description fields, with the name
 *              rules applied as the user types
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_PHRASE_EDITOR_PHRASE_DETAILS_H_
#define SPEAK5220_GUI_PHRASE_EDITOR_PHRASE_DETAILS_H_

#include <QString>
#include <QWidget>
#include <optional>

#include "core/project/phrase.h"
#include "gui/undo/project_edits.h"

class QLabel;
class QLineEdit;

namespace speak5220::app {

// What typing did to a name: the text to show, or why it was refused.
struct NameEdit {
  // `text` with a-z converted to A-Z; unset when refused.
  std::optional<QString> text;
  // Why the edit was refused: the first character a name cannot have.
  QString refusal;
};

// Decision D2 applied to a name as it is typed: lower case is converted
// to upper case, and a character outside A-Z, 0-9, space and ' ( ) , - .
// refuses the edit, with a message naming it. Spaces at the ends and the
// length are left to the rename (and the field's length limit). Pure.
NameEdit FilterNameEdit(const QString& text);

// Plan §5: the name and description of the selected phrase. Typing in
// the name is filtered by FilterNameEdit(): a refused character leaves the
// name as it was and shows the message below the fields. Each field is
// applied when editing finishes (Return or leaving it), as one undoable
// command through ProjectEdits; a rename the project refuses (a taken
// name, the budget) shows why and puts the name back.
// GUI thread only; the edits object must outlive the widget.
class PhraseDetails : public QWidget {
  Q_OBJECT

 public:
  explicit PhraseDetails(ProjectEdits& edits, QWidget* parent = nullptr);

  // Shows `phrase`, or clears and disables the fields when null (no
  // phrase, or several, selected). A field being edited for the same
  // phrase keeps its text.
  void ShowPhrase(const core::project::Phrase* phrase);

  QLineEdit* NameField() const { return name_; }
  QLineEdit* DescriptionField() const { return description_; }
  // The last message, or empty.
  QString Message() const;

 private:
  void OnNameEdited(const QString& text);
  void CommitName();
  void CommitDescription();
  void ShowMessage(const QString& message);

  ProjectEdits* edits_;
  QLineEdit* name_;
  QLineEdit* description_;
  QLabel* message_;
  core::project::PhraseId id_;
  QString name_shown_;
  QString description_shown_;
  // The name field's text before the latest keystroke.
  QString accepted_name_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_PHRASE_EDITOR_PHRASE_DETAILS_H_
