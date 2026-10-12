/*
 * File:        encoder_settings_form_test.cpp
 * Module:      encoder panel tests
 * Purpose:     Unit tests of the settings form generated from the encoder
 *              options' metadata, and of its sliders
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/encoder_settings_form.h"

#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QString>
#include <QToolButton>

#include "core/encoder/encoder_options.h"
#include "gtest/gtest.h"
#include "gui/encoder_panel/option_slider.h"

namespace speak5220::app {
namespace {

using core::encoder::EncoderOptionInfo;
using core::encoder::EncoderOptions;
using core::encoder::EncoderOptionType;

class EncoderSettingsFormTest : public ::testing::Test {
 protected:
  EncoderSettingsFormTest() {
    QObject::connect(&form_, &EncoderSettingsForm::Edited,
                     [this] { ++edits_; });
  }

  EncoderSettingsForm form_;
  int edits_ = 0;
};

TEST_F(EncoderSettingsFormTest, HasAnEditorOfTheRightKindForEveryOption) {
  for (const EncoderOptionInfo& info : core::encoder::EncoderOptionInfos()) {
    QWidget* editor = form_.Editor(info.field);
    ASSERT_NE(editor, nullptr) << info.field;
    switch (info.type) {
      case EncoderOptionType::kReal:
      case EncoderOptionType::kInteger:
        EXPECT_EQ(editor, form_.Slider(info.field)) << info.field;
        break;
      case EncoderOptionType::kOptionalReal:
        EXPECT_NE(form_.Slider(info.field), nullptr) << info.field;
        EXPECT_NE(form_.OnBox(info.field), nullptr) << info.field;
        break;
      case EncoderOptionType::kPreEmphasis:
        EXPECT_NE(qobject_cast<QComboBox*>(editor), nullptr) << info.field;
        EXPECT_NE(form_.Slider(info.field), nullptr) << info.field;
        break;
    }
    EXPECT_TRUE(editor->toolTip().contains(QString::fromUtf8(
        info.help.data(), static_cast<qsizetype>(info.help.size()))))
        << info.field;
  }
}

TEST_F(EncoderSettingsFormTest, SlidersShowTheValueWithItsUnit) {
  EXPECT_EQ(form_.Slider("target_rms")->Text(), QStringLiteral("24 codes"));
  EXPECT_EQ(form_.Slider("gate_db")->Text(), QStringLiteral("45 dB"));
  EXPECT_EQ(form_.Slider("window")->Text(), QStringLiteral("32 ms"));
  EXPECT_EQ(form_.Slider("voicing_threshold")->Text(), QStringLiteral("0.3"));
  EXPECT_EQ(form_.Slider("level_agc")->Text(), QStringLiteral("1.25:1"));
  form_.Slider("level_agc")->MoveTo(1.0);
  EXPECT_EQ(form_.Slider("level_agc")->Text(), QStringLiteral("off"));
}

TEST_F(EncoderSettingsFormTest, SlidersCoverTheTypicalRange) {
  const OptionSlider* rms = form_.Slider("target_rms");
  EXPECT_DOUBLE_EQ(rms->ValueAt(rms->Slider()->minimum()), 5.0);
  EXPECT_DOUBLE_EQ(rms->ValueAt(rms->Slider()->maximum()), 50.0);
  EXPECT_DOUBLE_EQ(rms->ValueAt(1), 5.5);
}

TEST_F(EncoderSettingsFormTest, TickMarksTheBaselinesValue) {
  EncoderOptions baseline;
  baseline.target_rms = 25.0;
  EncoderOptions options = baseline;
  options.target_rms = 18.0;
  form_.SetOptions(options, baseline, QStringLiteral("the project default"));
  const OptionSlider* rms = form_.Slider("target_rms");
  ASSERT_TRUE(rms->Slider()->DefaultPosition().has_value());
  EXPECT_EQ(*rms->Slider()->DefaultPosition(), rms->PositionOf(25.0));
  EXPECT_EQ(rms->Slider()->value(), rms->PositionOf(18.0));
}

TEST_F(EncoderSettingsFormTest, ShowsOptionsWithoutSignalling) {
  EncoderOptions options;
  options.target_rms = 18.5;
  options.resonance_hold_db.reset();
  options.window = 200;
  form_.SetOptions(options, {}, QStringLiteral("the project default"));
  EXPECT_EQ(edits_, 0);
  EXPECT_DOUBLE_EQ(form_.Slider("target_rms")->Value(), 18.5);
  EXPECT_FALSE(form_.OnBox("resonance_hold_db")->isChecked());
  EXPECT_DOUBLE_EQ(form_.Slider("window")->Value(), 200.0);
  EXPECT_EQ(form_.Options(), options);
}

TEST_F(EncoderSettingsFormTest, MovingASliderSignalsAndChangesTheOptions) {
  form_.Slider("gate_db")->MoveTo(50.0);
  EXPECT_EQ(edits_, 1);
  EXPECT_DOUBLE_EQ(form_.Options().gate_db, 50.0);
  form_.Slider("window")->MoveTo(200.0);
  EXPECT_EQ(form_.Options().window, 200);
  form_.OnBox("resonance_hold_db")->setChecked(false);
  EXPECT_FALSE(form_.Options().resonance_hold_db.has_value());
  EXPECT_EQ(edits_, 3);
}

TEST_F(EncoderSettingsFormTest, MarksOptionsThatDifferFromTheBaseline) {
  EncoderOptions options;
  options.target_rms = 18.0;
  form_.SetOptions(options, {}, QStringLiteral("the project default"));
  EXPECT_TRUE(form_.IsMarked("target_rms"));
  EXPECT_TRUE(form_.ResetButton("target_rms")->isEnabled());
  EXPECT_FALSE(form_.IsMarked("gate_db"));
  EXPECT_FALSE(form_.ResetButton("gate_db")->isEnabled());
  EXPECT_TRUE(form_.ResetButton("target_rms")
                  ->toolTip()
                  .contains(QStringLiteral("the project default (24)")));
}

TEST_F(EncoderSettingsFormTest, ResetButtonPutsAnOptionBackToTheBaseline) {
  EncoderOptions baseline;
  baseline.target_rms = 25.0;
  EncoderOptions options = baseline;
  options.target_rms = 18.0;
  form_.SetOptions(options, baseline, QStringLiteral("the project default"));
  form_.ResetButton("target_rms")->click();
  EXPECT_EQ(edits_, 1);
  EXPECT_DOUBLE_EQ(form_.Options().target_rms, 25.0);
  EXPECT_FALSE(form_.IsMarked("target_rms"));
}

TEST_F(EncoderSettingsFormTest, ResonanceHoldIsTurnedOffByItsCheckBox) {
  EXPECT_TRUE(form_.OnBox("resonance_hold_db")->isChecked());
  EXPECT_TRUE(form_.Slider("resonance_hold_db")->isEnabled());
  form_.OnBox("resonance_hold_db")->setChecked(false);
  EXPECT_FALSE(form_.Options().resonance_hold_db.has_value());
  EXPECT_FALSE(form_.Slider("resonance_hold_db")->isEnabled());
  form_.OnBox("resonance_hold_db")->setChecked(true);
  ASSERT_TRUE(form_.Options().resonance_hold_db.has_value());
  EXPECT_DOUBLE_EQ(*form_.Options().resonance_hold_db, 14.0);
  form_.Slider("resonance_hold_db")->MoveTo(20.0);
  EXPECT_DOUBLE_EQ(*form_.Options().resonance_hold_db, 20.0);
}

TEST_F(EncoderSettingsFormTest, SquelchIsTurnedOnByItsCheckBox) {
  EXPECT_FALSE(form_.OnBox("squelch_db")->isChecked());
  EXPECT_FALSE(form_.Slider("squelch_db")->isEnabled());
  form_.OnBox("squelch_db")->setChecked(true);
  ASSERT_TRUE(form_.Options().squelch_db.has_value());
  EXPECT_DOUBLE_EQ(*form_.Options().squelch_db, 30.0);
  form_.Slider("squelch_db")->MoveTo(24.0);
  EXPECT_DOUBLE_EQ(*form_.Options().squelch_db, 24.0);
  form_.OnBox("squelch_db")->setChecked(false);
  EXPECT_FALSE(form_.Options().squelch_db.has_value());
  EXPECT_EQ(form_.OnBox("target_rms"), nullptr);
}

TEST_F(EncoderSettingsFormTest, PreEmphasisIsChirpOrAFirstOrderCoefficient) {
  auto* kind = qobject_cast<QComboBox*>(form_.Editor("pre_emphasis"));
  OptionSlider* mu = form_.Slider("pre_emphasis");
  EXPECT_FALSE(mu->isEnabled());
  kind->setCurrentIndex(1);
  EXPECT_TRUE(mu->isEnabled());
  mu->MoveTo(0.9);
  EXPECT_EQ(form_.Options().pre_emphasis,
            core::encoder::PreEmphasis::FirstOrder(0.9));
  kind->setCurrentIndex(0);
  EXPECT_TRUE(form_.Options().pre_emphasis.chirp);
}

TEST_F(EncoderSettingsFormTest, KeepsValuesOffTheStepsUntilEdited) {
  EncoderOptions options;
  options.gate_db = 51.123456;
  form_.SetOptions(options, {}, QStringLiteral("the project default"));
  EXPECT_EQ(form_.Slider("gate_db")->Text(), QStringLiteral("51.123456 dB"));
  form_.Slider("target_rms")->MoveTo(21.0);
  EXPECT_DOUBLE_EQ(form_.Options().gate_db, 51.123456);
}

TEST_F(EncoderSettingsFormTest, ValuesOutsideTheTypicalRangeWidenTheSlider) {
  EncoderOptions options;
  options.repeat_lambda = 800.0;
  form_.SetOptions(options, {}, QStringLiteral("the project default"));
  const OptionSlider* lambda = form_.Slider("repeat_lambda");
  EXPECT_DOUBLE_EQ(lambda->Value(), 800.0);
  EXPECT_EQ(lambda->Slider()->value(), lambda->Slider()->maximum());
  EXPECT_DOUBLE_EQ(lambda->ValueAt(lambda->Slider()->maximum()), 800.0);
  // A value back in the range narrows it again.
  form_.SetOptions({}, {}, QStringLiteral("the project default"));
  EXPECT_DOUBLE_EQ(lambda->ValueAt(lambda->Slider()->maximum()), 300.0);
}

TEST_F(EncoderSettingsFormTest, HidesTheAdvancedOptionsUntilAsked) {
  bool shown_signalled = false;
  QObject::connect(&form_, &EncoderSettingsForm::AdvancedShownChanged,
                   [&shown_signalled](bool shown) { shown_signalled = shown; });
  EXPECT_FALSE(form_.AdvancedShown());
  for (const EncoderOptionInfo& info : core::encoder::EncoderOptionInfos()) {
    EXPECT_EQ(form_.Editor(info.field)->isVisibleTo(&form_), !info.advanced)
        << info.field;
  }
  form_.AdvancedButton()->click();
  EXPECT_TRUE(form_.AdvancedShown());
  EXPECT_TRUE(shown_signalled);
  for (const EncoderOptionInfo& info : core::encoder::EncoderOptionInfos()) {
    EXPECT_TRUE(form_.Editor(info.field)->isVisibleTo(&form_)) << info.field;
  }
}

TEST_F(EncoderSettingsFormTest, SettingWhetherAdvancedIsShownDoesNotSignal) {
  int count = 0;
  QObject::connect(&form_, &EncoderSettingsForm::AdvancedShownChanged,
                   [&count](bool) { ++count; });
  form_.SetAdvancedShown(true);
  EXPECT_TRUE(form_.AdvancedShown());
  EXPECT_TRUE(form_.Editor("gate_db")->isVisibleTo(&form_));
  EXPECT_EQ(count, 0);
}

TEST_F(EncoderSettingsFormTest, AdvancedButtonCountsHiddenChanges) {
  EXPECT_EQ(form_.AdvancedButton()->text(),
            QStringLiteral("Advanced settings"));
  EncoderOptions options;
  options.target_rms = 25.0;  // basic, not counted
  options.gate_db = 50.0;
  options.lag_window_hz = 40.0;
  form_.SetOptions(options, {}, QStringLiteral("the project default"));
  EXPECT_EQ(form_.AdvancedButton()->text(),
            QStringLiteral("Advanced settings (2 changed)"));
  form_.SetAdvancedShown(true);
  form_.ResetButton("gate_db")->click();
  EXPECT_EQ(form_.AdvancedButton()->text(),
            QStringLiteral("Advanced settings (1 changed)"));
}

TEST_F(EncoderSettingsFormTest, NotEditableKeepsTheAdvancedButtonUsable) {
  EncoderOptions options;
  options.target_rms = 25.0;
  form_.SetOptions(options, {}, QStringLiteral("the project default"));
  form_.SetEditable(false);
  EXPECT_FALSE(form_.Editor("target_rms")->isEnabled());
  EXPECT_FALSE(form_.ResetButton("target_rms")->isEnabled());
  EXPECT_TRUE(form_.AdvancedButton()->isEnabled());
  form_.AdvancedButton()->click();
  EXPECT_TRUE(form_.AdvancedShown());
  form_.SetEditable(true);
  EXPECT_TRUE(form_.Editor("target_rms")->isEnabled());
  EXPECT_TRUE(form_.ResetButton("target_rms")->isEnabled());
}

TEST_F(EncoderSettingsFormTest, DrawsOffscreenWithTheTicks) {
  form_.resize(900, 300);
  QImage image(form_.size(), QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  form_.render(&image);
  EXPECT_FALSE(image.isNull());
}

}  // namespace
}  // namespace speak5220::app
