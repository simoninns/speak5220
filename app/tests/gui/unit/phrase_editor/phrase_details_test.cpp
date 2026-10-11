/*
 * File:        phrase_details_test.cpp
 * Module:      phrase_editor tests
 * Purpose:     Unit tests for the phrase name and description fields over
 *              a mocked IProjectEditor
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/phrase_editor/phrase_details.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QString>
#include <QUndoStack>
#include <string>

#include "core/project/phrase.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/support/test_palettes.h"
#include "gui/theme/colours.h"
#include "gui/undo/project_edits.h"
#include "support/project_builder.h"
#include "support/project_mocks.h"

namespace speak5220::app {
namespace {

using core::project::Phrase;
using testing::AudioPhrase;
using testing::DarkPalette;
using ::testing::Eq;
using testing::LightPalette;
using testing::MockProjectEditor;
using ::testing::Return;
using ::testing::StrictMock;
using testing::TestId;

// Sends one key press, as typing it would.
void Type(QLineEdit* field, Qt::Key key, const QString& text) {
  QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text);
  QCoreApplication::sendEvent(field, &press);
}

void TypeText(QLineEdit* field, const QString& text) {
  for (const QChar c : text) {
    Type(field, Qt::Key_unknown, QString(c));
  }
}

void PressReturn(QLineEdit* field) {
  Type(field, Qt::Key_Return, QStringLiteral("\r"));
}

TEST(FilterNameEditTest, ConvertsLowerCase) {
  const NameEdit edit = FilterNameEdit(QStringLiteral("o'clock 2"));
  ASSERT_TRUE(edit.text.has_value());
  EXPECT_EQ(*edit.text, QStringLiteral("O'CLOCK 2"));
  EXPECT_TRUE(edit.refusal.isEmpty());
}

TEST(FilterNameEditTest, AcceptsTheAcornPunctuation) {
  const NameEdit edit = FilterNameEdit(QStringLiteral("2- (TWEN-),X."));
  ASSERT_TRUE(edit.text.has_value());
  EXPECT_EQ(*edit.text, QStringLiteral("2- (TWEN-),X."));
}

TEST(FilterNameEditTest, RefusesOtherCharactersNamingThem) {
  const NameEdit edit = FilterNameEdit(QStringLiteral("HELLO!"));
  EXPECT_FALSE(edit.text.has_value());
  EXPECT_EQ(edit.refusal,
            QStringLiteral("a phrase name cannot contain \"!\": use A-Z, "
                           "0-9, space and ' ( ) , - ."));
}

TEST(FilterNameEditTest, RefusesCharactersBeyondAscii) {
  const NameEdit edit = FilterNameEdit(QStringLiteral("CAFé"));
  EXPECT_FALSE(edit.text.has_value());
  EXPECT_TRUE(edit.refusal.contains(QStringLiteral("\"é\"")));
}

class PhraseDetailsTest : public ::testing::Test {
 protected:
  PhraseDetailsTest() { details_.ShowPhrase(&phrase_); }

  StrictMock<MockProjectEditor> editor_;
  QUndoStack stack_;
  ProjectEdits edits_{editor_, stack_};
  PhraseDetails details_{edits_};
  Phrase phrase_ = AudioPhrase(1, "HELLO");
};

TEST_F(PhraseDetailsTest, ShowsThePhrase) {
  phrase_.description = "A greeting";
  details_.ShowPhrase(&phrase_);
  EXPECT_EQ(details_.NameField()->text(), QStringLiteral("HELLO"));
  EXPECT_EQ(details_.DescriptionField()->text(), QStringLiteral("A greeting"));
  EXPECT_TRUE(details_.NameField()->isEnabled());
  EXPECT_EQ(details_.NameField()->maxLength(), 16);
}

TEST_F(PhraseDetailsTest, MessageColourFollowsThePalette) {
  details_.setPalette(LightPalette());
  EXPECT_EQ(details_.MessageColour(), ColoursFor(LightPalette()).warning);
  // A change of colour scheme reaches the widget as a palette change.
  details_.setPalette(DarkPalette());
  EXPECT_EQ(details_.MessageColour(), ColoursFor(DarkPalette()).warning);
}

TEST_F(PhraseDetailsTest, NoPhraseClearsAndDisablesTheFields) {
  details_.ShowPhrase(nullptr);
  EXPECT_TRUE(details_.NameField()->text().isEmpty());
  EXPECT_FALSE(details_.NameField()->isEnabled());
  EXPECT_FALSE(details_.DescriptionField()->isEnabled());
}

TEST_F(PhraseDetailsTest, LowerCaseIsConvertedAsItIsTyped) {
  details_.NameField()->clear();
  TypeText(details_.NameField(), QStringLiteral("good day"));
  EXPECT_EQ(details_.NameField()->text(), QStringLiteral("GOOD DAY"));
  EXPECT_TRUE(details_.Message().isEmpty());
}

TEST_F(PhraseDetailsTest, RefusedCharacterShowsAMessageAndLeavesTheName) {
  details_.NameField()->end(false);
  TypeText(details_.NameField(), QStringLiteral("!"));
  EXPECT_EQ(details_.NameField()->text(), QStringLiteral("HELLO"));
  EXPECT_EQ(details_.NameField()->cursorPosition(), 5);
  EXPECT_TRUE(details_.Message().contains(QStringLiteral("\"!\"")));
  // The next accepted keystroke clears the message.
  TypeText(details_.NameField(), QStringLiteral("s"));
  EXPECT_EQ(details_.NameField()->text(), QStringLiteral("HELLOS"));
  EXPECT_TRUE(details_.Message().isEmpty());
}

TEST_F(PhraseDetailsTest, ReturnRenamesThePhrase) {
  EXPECT_CALL(editor_, RenamePhrase(TestId(1), Eq("HELLO 2")))
      .WillOnce(Return(std::string("HELLO")));
  details_.NameField()->end(false);
  TypeText(details_.NameField(), QStringLiteral(" 2"));
  PressReturn(details_.NameField());
  EXPECT_EQ(stack_.count(), 1);
}

TEST_F(PhraseDetailsTest, RenameTheProjectRefusesShowsWhyAndRestoresTheName) {
  EXPECT_CALL(editor_, RenamePhrase(TestId(1), Eq("BYE")))
      .WillOnce(Return(
          core::InvalidArgumentError("there is already a phrase called BYE")));
  details_.NameField()->clear();
  TypeText(details_.NameField(), QStringLiteral("bye"));
  PressReturn(details_.NameField());
  EXPECT_EQ(details_.NameField()->text(), QStringLiteral("HELLO"));
  EXPECT_EQ(details_.Message(),
            QStringLiteral("there is already a phrase called BYE"));
  EXPECT_EQ(stack_.count(), 0);
}

TEST_F(PhraseDetailsTest, UnchangedNameMakesNoEdit) {
  // StrictMock: any call to the editor fails the test.
  PressReturn(details_.NameField());
  details_.NameField()->end(false);
  TypeText(details_.NameField(), QStringLiteral(" "));
  PressReturn(details_.NameField());
  EXPECT_EQ(details_.NameField()->text(), QStringLiteral("HELLO"));
  EXPECT_EQ(stack_.count(), 0);
}

TEST_F(PhraseDetailsTest, ReturnSetsTheDescription) {
  EXPECT_CALL(editor_, SetDescription(TestId(1), "Said at noon"))
      .WillOnce(Return(std::string()));
  TypeText(details_.DescriptionField(), QStringLiteral("Said at noon"));
  PressReturn(details_.DescriptionField());
  EXPECT_EQ(stack_.count(), 1);
}

TEST_F(PhraseDetailsTest, AnotherPhraseClearsTheMessage) {
  TypeText(details_.NameField(), QStringLiteral("?"));
  ASSERT_FALSE(details_.Message().isEmpty());
  const Phrase other = AudioPhrase(2, "OTHER");
  details_.ShowPhrase(&other);
  EXPECT_TRUE(details_.Message().isEmpty());
  EXPECT_EQ(details_.NameField()->text(), QStringLiteral("OTHER"));
}

}  // namespace
}  // namespace speak5220::app
