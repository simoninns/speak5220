/*
 * File:        accessibility_test.cpp
 * Module:      main_window tests
 * Purpose:     Checks every window and dialog for keyboard access and
 *              accessible names: each control can take the focus from the
 *              keyboard and names itself to a screen reader
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QWidget>
#include <set>
#include <string>
#include <vector>

#include "core/project/phrase.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/about/about_dialog.h"
#include "gui/encoder_panel/encoder_panel.h"
#include "gui/encoder_panel/encoder_settings_form.h"
#include "gui/export/export_dialog.h"
#include "gui/main_window/main_window.h"
#include "gui/optimise/optimise_dialog.h"
#include "gui/phrase_editor/phrase_details.h"
#include "gui/phrase_list/phrase_list_panel.h"
#include "gui/playback/player.h"
#include "gui/preferences/preferences_dialog.h"
#include "gui/support/gui_mocks.h"
#include "gui/waveform/phrase_tab.h"
#include "gui/waveform/phrase_waveforms.h"
#include "gui/waveform/waveform_pane.h"
#include "gui/waveform/waveform_view.h"
#include "support/project_builder.h"

namespace speak5220::app {
namespace {

using testing::AudioPhrase;
using testing::EncodedPhrase;
using testing::FakeRecentFilesStore;
using testing::FakeViewPreferences;
using ::testing::IsEmpty;
using testing::MockDialogs;
using ::testing::NiceMock;
using testing::TestId;
using testing::TestPlayback;

// `text` without its mnemonic marks.
QString Plain(QString text) { return text.remove(QLatin1Char('&')).trimmed(); }

// Whether `text` reads as a name: it holds a word of two letters or more,
// not only a symbol such as "+" or an arrow.
bool IsWords(const QString& text) {
  int letters = 0;
  for (const QChar c : text) {
    letters += c.isLetter() ? 1 : 0;
  }
  return letters >= 2;
}

// The text of the labels that are buddies of widgets under `root`.
std::set<const QWidget*> Labelled(const QWidget* root) {
  std::set<const QWidget*> labelled;
  for (const QLabel* label : root->findChildren<QLabel*>()) {
    if (label->buddy() != nullptr && IsWords(Plain(label->text()))) {
      labelled.insert(label->buddy());
    }
  }
  return labelled;
}

// Whether a screen reader can name `widget`: its accessible name, a
// button's own words, or a label naming it.
bool HasName(const QWidget* widget, const std::set<const QWidget*>& labelled) {
  if (IsWords(widget->accessibleName())) {
    return true;
  }
  if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
    return IsWords(Plain(button->text()));
  }
  if (labelled.contains(widget)) {
    return true;
  }
  // A spin box's or combo box's line edit is named by its owner.
  const QWidget* owner = widget->parentWidget();
  return qobject_cast<const QLineEdit*>(widget) != nullptr &&
         owner != nullptr &&
         (qobject_cast<const QAbstractSpinBox*>(owner) != nullptr ||
          qobject_cast<const QComboBox*>(owner) != nullptr);
}

// Whether `widget` is a control the user works with. Labels are read with
// the window; scroll areas and tab widgets only hold controls.
bool IsControl(const QWidget* widget) {
  if (qobject_cast<const QLabel*>(widget) != nullptr ||
      qobject_cast<const QScrollArea*>(widget) != nullptr ||
      qobject_cast<const QTabWidget*>(widget) != nullptr) {
    return false;
  }
  return qobject_cast<const QAbstractButton*>(widget) != nullptr ||
         qobject_cast<const QAbstractSlider*>(widget) != nullptr ||
         qobject_cast<const QAbstractSpinBox*>(widget) != nullptr ||
         qobject_cast<const QComboBox*>(widget) != nullptr ||
         qobject_cast<const QLineEdit*>(widget) != nullptr ||
         qobject_cast<const QTextEdit*>(widget) != nullptr ||
         qobject_cast<const QPlainTextEdit*>(widget) != nullptr ||
         qobject_cast<const QAbstractItemView*>(widget) != nullptr ||
         qobject_cast<const QTabBar*>(widget) != nullptr ||
         widget->focusPolicy() != Qt::NoFocus;
}

// How a failing widget is described: its class, and its tooltip or name.
QString Describe(const QWidget* widget) {
  QString what = QString::fromLatin1(widget->metaObject()->className());
  if (!widget->objectName().isEmpty()) {
    what += QStringLiteral(" \"%1\"").arg(widget->objectName());
  }
  if (!widget->toolTip().isEmpty()) {
    what += QStringLiteral(" (tip: %1)").arg(widget->toolTip());
  }
  if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
    what += QStringLiteral(" [%1]").arg(button->text());
  }
  return what;
}

// The controls under `root` shown with it that have no name, and those
// the keyboard cannot reach.
struct Audit {
  std::vector<std::string> unnamed;
  std::vector<std::string> unreachable;
};

Audit AuditWidgets(const QWidget* root) {
  Audit audit;
  const std::set<const QWidget*> labelled = Labelled(root);
  for (const QWidget* widget : root->findChildren<QWidget*>()) {
    if (!widget->isVisibleTo(root) || !IsControl(widget)) {
      continue;
    }
    // Scroll bars are reached through their views; a QTabWidget's bar is
    // named by its tabs.
    if (qobject_cast<const QAbstractSlider*>(widget) != nullptr &&
        QString::fromLatin1(widget->metaObject()->className()) ==
            QStringLiteral("QScrollBar")) {
      continue;
    }
    if (qobject_cast<const QTabBar*>(widget) == nullptr &&
        !HasName(widget, labelled)) {
      audit.unnamed.push_back(Describe(widget).toStdString());
    }
    if ((widget->focusPolicy() & Qt::TabFocus) == 0) {
      audit.unreachable.push_back(Describe(widget).toStdString());
    }
  }
  return audit;
}

class AccessibilityTest : public ::testing::Test {
 protected:
  AccessibilityTest() {
    std::vector<core::project::PlacedPhrase> placed;
    placed.push_back({.index = 0, .phrase = EncodedPhrase(1, "HELLO", 600)});
    placed.push_back({.index = 1, .phrase = AudioPhrase(2, "WORLD")});
    EXPECT_TRUE(window_.Document().Edits().InsertPhrases(placed).IsOk());
    window_.show();
    window_.Panel()->Select({TestId(1)});
    QApplication::processEvents();
  }

  NiceMock<MockDialogs> dialogs_;
  FakeRecentFilesStore recent_;
  FakeViewPreferences preferences_;
  core::InlineTaskRunner runner_;
  TestPlayback playback_;
  testing::TestEncoding encoding_;
  MainWindow window_{dialogs_,         recent_,         preferences_,
                     runner_,          playback_.synth, playback_.player,
                     encoding_.Tools()};
};

TEST_F(AccessibilityTest, MainWindowControlsAreNamedAndReachable) {
  for (const PhraseTab tab :
       {PhraseTab::kOriginal, PhraseTab::kPreprocess, PhraseTab::kEncode}) {
    window_.Waveforms()->SetCurrentTab(tab);
    QApplication::processEvents();
    const Audit audit = AuditWidgets(&window_);
    EXPECT_THAT(audit.unnamed, IsEmpty()) << "tab " << static_cast<int>(tab);
    EXPECT_THAT(audit.unreachable, IsEmpty())
        << "tab " << static_cast<int>(tab);
  }
}

// The getting-started walkthrough's commands, each with its key.
TEST_F(AccessibilityTest, WalkthroughCommandsHaveShortcuts) {
  const QStringList walkthrough = {QStringLiteral("&New"),
                                   QStringLiteral("&Add Phrases..."),
                                   QStringLiteral("Re-&encode"),
                                   QStringLiteral("&Optimise..."),
                                   QStringLiteral("&Save"),
                                   QStringLiteral("Save &As..."),
                                   QStringLiteral("&Export..."),
                                   QStringLiteral("&Open..."),
                                   QStringLiteral("&Preferences..."),
                                   QStringLiteral("&Import PHROM Image...")};
  for (const QString& text : walkthrough) {
    bool found = false;
    for (const QAction* action : window_.findChildren<QAction*>()) {
      if (action->text() == text) {
        found = true;
        EXPECT_FALSE(action->shortcut().isEmpty()) << text.toStdString();
      }
    }
    EXPECT_TRUE(found) << text.toStdString();
  }
}

TEST_F(AccessibilityTest, TabShortcutsShowTheTabAndFocusItsWaveform) {
  window_.activateWindow();
  QApplication::processEvents();
  for (const PhraseTab tab :
       {PhraseTab::kEncode, PhraseTab::kPreprocess, PhraseTab::kOriginal}) {
    QAction* action = window_.TabAction(tab);
    ASSERT_NE(action, nullptr);
    EXPECT_FALSE(action->shortcut().isEmpty());
    action->trigger();
    QApplication::processEvents();
    EXPECT_EQ(window_.Waveforms()->CurrentTab(), tab);
    EXPECT_TRUE(window_.Waveforms()->Pane(tab)->View()->hasFocus());
  }
}

TEST_F(AccessibilityTest, SpaceOnTheFocusedWaveformPlaysIt) {
  window_.TabAction(PhraseTab::kOriginal)->trigger();
  EXPECT_CALL(playback_.output, Start(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(core::OkStatus()));
  QKeyEvent space(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier,
                  QStringLiteral(" "));
  QApplication::sendEvent(
      window_.Waveforms()->Pane(PhraseTab::kOriginal)->View(), &space);
  EXPECT_EQ(playback_.player.State(), PlaybackState::kPlaying);
}

TEST_F(AccessibilityTest, TabKeyReachesTheListTheTabsAndTheControls) {
  // Following the focus chain from the phrase list, as Tab does.
  std::set<const QWidget*> reached;
  QWidget* widget = window_.Panel()->View();
  for (int step = 0; step < 1000 && !reached.contains(widget); ++step) {
    if (widget->isVisibleTo(&window_) && widget->isEnabled() &&
        (widget->focusPolicy() & Qt::TabFocus) != 0) {
      reached.insert(widget);
    }
    widget = widget->nextInFocusChain();
  }
  EXPECT_TRUE(reached.contains(window_.Waveforms()->Tabs()));
  EXPECT_TRUE(reached.contains(window_.Details()->NameField()));
  EXPECT_TRUE(reached.contains(
      window_.Waveforms()->PlayButton(PlaybackSource::kOriginal)));
  EXPECT_TRUE(reached.contains(
      window_.Waveforms()->Pane(PhraseTab::kOriginal)->View()));
}

TEST_F(AccessibilityTest, AdvancedEncoderSettingsAreNamedAndReachable) {
  window_.Waveforms()->SetCurrentTab(PhraseTab::kEncode);
  window_.Encoder()->Form()->SetAdvancedShown(true);
  QApplication::processEvents();
  const Audit audit = AuditWidgets(&window_);
  EXPECT_THAT(audit.unnamed, IsEmpty());
  EXPECT_THAT(audit.unreachable, IsEmpty());
}

TEST_F(AccessibilityTest, OptimiseWindowControlsAreNamedAndReachable) {
  window_.Optimise({TestId(1)});
  QApplication::processEvents();
  const Audit audit = AuditWidgets(window_.OptimiseWindow());
  EXPECT_THAT(audit.unnamed, IsEmpty());
  EXPECT_THAT(audit.unreachable, IsEmpty());
  window_.OptimiseWindow()->reject();
}

TEST_F(AccessibilityTest, ExportDialogControlsAreNamedAndReachable) {
  ExportDialog dialog({.project = &window_.Document().Project(),
                       .selected = {},
                       .base = QStringLiteral("speech"),
                       .directory = QString(),
                       .confirm_overwrite = {}});
  dialog.show();
  QApplication::processEvents();
  const Audit audit = AuditWidgets(&dialog);
  EXPECT_THAT(audit.unnamed, IsEmpty());
  EXPECT_THAT(audit.unreachable, IsEmpty());
}

TEST_F(AccessibilityTest, PreferencesControlsAreNamedAndReachable) {
  PreferencesDialog dialog(
      Preferences{}, {{.id = "usb", .description = QStringLiteral("USB")}});
  dialog.show();
  for (int tab = 0; tab < dialog.Tabs()->count(); ++tab) {
    dialog.Tabs()->setCurrentIndex(tab);
    QApplication::processEvents();
    const Audit audit = AuditWidgets(&dialog);
    EXPECT_THAT(audit.unnamed, IsEmpty()) << "tab " << tab;
    EXPECT_THAT(audit.unreachable, IsEmpty()) << "tab " << tab;
  }
}

TEST_F(AccessibilityTest, AboutBoxControlsAreNamedAndReachable) {
  AboutDialog dialog;
  dialog.show();
  for (int tab = 0; tab < dialog.Tabs()->count(); ++tab) {
    dialog.Tabs()->setCurrentIndex(tab);
    QApplication::processEvents();
    const Audit audit = AuditWidgets(&dialog);
    EXPECT_THAT(audit.unnamed, IsEmpty()) << "tab " << tab;
    EXPECT_THAT(audit.unreachable, IsEmpty()) << "tab " << tab;
  }
}

}  // namespace
}  // namespace speak5220::app
