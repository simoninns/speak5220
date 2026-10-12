/*
 * File:        encoder_settings_form.cpp
 * Module:      encoder_panel
 * Purpose:     The encoder's settings as a form, one editor per option,
 *              generated from the options' metadata
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/encoder_settings_form.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QString>
#include <QToolButton>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/util/number_text.h"
#include "core/util/status.h"
#include "gui/encoder_panel/option_slider.h"

namespace speak5220::app {

namespace {

namespace encoder = core::encoder;
using encoder::EncoderOptionInfo;
using encoder::EncoderOptionType;

// Columns of options, in each of the basic and advanced groups.
constexpr int kOptionColumns = 2;
// Grid columns per option: label, editor, reset.
constexpr int kColumnsPerOption = 3;
// The pre-emphasis kinds in their chooser.
constexpr int kChirpIndex = 0;
constexpr int kFirstOrderIndex = 1;
// The unit of a ratio, which its minimum turns off.
constexpr std::string_view kRatioUnit = ":1";
// The unit of the analysis window, shown in milliseconds (a step of 8
// samples is 1 ms).
constexpr std::string_view kSamplesUnit = "samples";
constexpr double kMillisecondsPerSample = 1000.0 / 8000.0;
// What the optional options' and the coefficient's sliders show before
// one is set: a squelch under most breaths, the default resonance hold,
// and the coefficient Optimise tries.
constexpr double kUnsetSquelch = 30.0;
constexpr double kUnsetResonanceHold = 14.0;
constexpr double kUnsetMu = 0.9;

// The slider's value for an optional option that is unset.
double UnsetValue(std::string_view field) {
  return field == "squelch_db" ? kUnsetSquelch : kUnsetResonanceHold;
}

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// A value of `info` as the slider shows it: "20 codes", "45 dB", "0.3",
// "30 ms" for 240 samples, "off" or "2.5:1" for a ratio.
QString FormatValue(const EncoderOptionInfo& info, double value) {
  const QString number = QString::fromStdString(core::FormatNumber(value));
  if (info.unit == kRatioUnit) {
    return value <= info.minimum ? QStringLiteral("off")
                                 : number + ToQString(kRatioUnit);
  }
  if (info.unit == kSamplesUnit) {
    return QStringLiteral("%1 ms").arg(QString::fromStdString(
        core::FormatNumber(value * kMillisecondsPerSample)));
  }
  return info.unit.empty()
             ? number
             : QStringLiteral("%1 %2").arg(number, ToQString(info.unit));
}

}  // namespace

EncoderSettingsForm::EncoderSettingsForm(QWidget* parent)
    : QWidget(parent), advanced_button_(new QToolButton(this)) {
  auto* grid = new QGridLayout(this);
  grid->setContentsMargins(0, 0, 0, 0);
  const auto infos = encoder::EncoderOptionInfos();
  // The basic options first, then the advanced, each in metadata order.
  std::vector<const EncoderOptionInfo*> ordered;
  for (const bool advanced : {false, true}) {
    for (const EncoderOptionInfo& info : infos) {
      if (info.advanced == advanced) {
        ordered.push_back(&info);
      }
    }
  }
  const int basic_count = static_cast<int>(std::ranges::count_if(
      infos, [](const EncoderOptionInfo& info) { return !info.advanced; }));
  const int basic_rows = (basic_count + kOptionColumns - 1) / kOptionColumns;
  const int advanced_rows =
      (static_cast<int>(infos.size()) - basic_count + kOptionColumns - 1) /
      kOptionColumns;
  // The button between the groups.
  advanced_button_->setCheckable(true);
  advanced_button_->setAutoRaise(true);
  advanced_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  advanced_button_->setToolTip(
      QStringLiteral("Show the settings that fine-tune the encoding"));
  grid->addWidget(advanced_button_, basic_rows, 0, 1,
                  kOptionColumns * kColumnsPerOption, Qt::AlignLeft);
  rows_.reserve(infos.size());
  for (std::size_t i = 0; i < ordered.size(); ++i) {
    const EncoderOptionInfo& info = *ordered[i];
    Row row;
    row.info = &info;
    const QString help = QStringLiteral("%1\nDefault: %2")
                             .arg(ToQString(info.help),
                                  info.default_text.empty()
                                      ? ToQString(encoder::kUnsetOptionText)
                                      : ToQString(info.default_text));
    row.label = new QLabel(ToQString(info.label), this);
    row.label->setToolTip(help);
    auto make_slider = [&info](QWidget* owner) {
      return new OptionSlider(
          info.typical_minimum, info.typical_maximum, info.step,
          [&info](double value) { return FormatValue(info, value); }, owner);
    };
    QWidget* editor = nullptr;
    QWidget* focus = nullptr;
    switch (info.type) {
      case EncoderOptionType::kReal:
      case EncoderOptionType::kInteger:
        row.slider = make_slider(this);
        editor = row.slider;
        focus = row.slider->Slider();
        break;
      case EncoderOptionType::kOptionalReal: {
        auto* box = new QWidget(this);
        auto* layout = new QHBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        row.check = new QCheckBox(box);
        row.check->setToolTip(
            QStringLiteral("Turn %1 on").arg(ToQString(info.label).toLower()));
        row.slider = make_slider(box);
        layout->addWidget(row.check);
        layout->addWidget(row.slider, 1);
        editor = box;
        focus = row.check;
        break;
      }
      case EncoderOptionType::kPreEmphasis: {
        auto* box = new QWidget(this);
        auto* layout = new QHBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        row.kind = new QComboBox(box);
        row.kind->addItem(QStringLiteral("Chirp"));
        row.kind->addItem(QStringLiteral("First order"));
        row.slider = make_slider(box);
        layout->addWidget(row.kind);
        layout->addWidget(row.slider, 1);
        editor = box;
        focus = row.kind;
        break;
      }
    }
    for (QWidget* widget : {editor, focus}) {
      widget->setToolTip(help);
      widget->setAccessibleName(ToQString(info.label));
    }
    if (row.slider != nullptr) {
      row.slider->Slider()->setAccessibleName(ToQString(info.label));
      row.slider->setToolTip(
          info.type == EncoderOptionType::kPreEmphasis
              ? help + QStringLiteral("\nThe slider sets the coefficient mu.")
              : help);
    }
    row.label->setBuddy(focus);
    row.editor = editor;
    row.reset = new QToolButton(this);
    row.reset->setText(QStringLiteral("↺"));
    row.reset->setAutoRaise(true);

    // Down the first column of its group, then the second.
    const int index = static_cast<int>(i) - (info.advanced ? basic_count : 0);
    // A row is in a group, so the group has at least one row.
    const int group_rows =
        std::max(1, info.advanced ? advanced_rows : basic_rows);
    const int r = (info.advanced ? basic_rows + 1 : 0) + index % group_rows;
    const int c = (index / group_rows) * kColumnsPerOption;
    grid->addWidget(row.label, r, c);
    grid->addWidget(editor, r, c + 1);
    grid->addWidget(row.reset, r, c + 2);
    rows_.push_back(row);
  }
  grid->setColumnStretch(1, 1);
  grid->setColumnStretch(1 + kColumnsPerOption, 1);
  // Every value's text the same width, so the sliders line up.
  int text_width = 0;
  for (const Row& row : rows_) {
    if (row.slider != nullptr) {
      text_width = std::max(text_width, row.slider->TextWidth());
    }
  }
  for (const Row& row : rows_) {
    if (row.slider != nullptr) {
      row.slider->SetTextWidth(text_width);
    }
  }

  // rows_ is complete, so its elements stay where they are.
  for (const Row& row : rows_) {
    const Row* at = &row;
    auto edited = [this, at] { OnEdited(*at); };
    if (row.slider != nullptr) {
      connect(row.slider, &OptionSlider::ValueChanged, this, edited);
    }
    if (row.check != nullptr) {
      connect(row.check, &QCheckBox::toggled, this, edited);
    }
    if (row.kind != nullptr) {
      connect(row.kind, &QComboBox::currentIndexChanged, this, edited);
    }
    connect(row.reset, &QToolButton::clicked, this, [this, at] {
      const std::string_view field = at->info->field;
      // The baseline's text is valid, so this cannot fail.
      (void)encoder::SetEncoderOption(
          options_, field, encoder::EncoderOptionText(baseline_, field));
      ShowValue(*at);
      UpdateMarks();
      emit Edited();
    });
  }
  connect(advanced_button_, &QToolButton::toggled, this, [this](bool shown) {
    UpdateAdvanced();
    emit AdvancedShownChanged(shown);
  });
  SetOptions({}, {}, QStringLiteral("the default"));
}

void EncoderSettingsForm::SetOptions(const encoder::EncoderOptions& options,
                                     const encoder::EncoderOptions& baseline,
                                     const QString& baseline_name) {
  options_ = options;
  baseline_ = baseline;
  baseline_name_ = baseline_name;
  for (const Row& row : rows_) {
    ShowValue(row);
  }
  UpdateMarks();
}

QWidget* EncoderSettingsForm::Editor(std::string_view field) const {
  const Row* row = FindRow(field);
  if (row == nullptr) {
    return nullptr;
  }
  if (row->kind != nullptr) {
    return row->kind;
  }
  return row->slider;
}

OptionSlider* EncoderSettingsForm::Slider(std::string_view field) const {
  const Row* row = FindRow(field);
  return row == nullptr ? nullptr : row->slider;
}

QCheckBox* EncoderSettingsForm::OnBox(std::string_view field) const {
  const Row* row = FindRow(field);
  return row == nullptr || row->info->type != EncoderOptionType::kOptionalReal
             ? nullptr
             : row->check;
}

QToolButton* EncoderSettingsForm::ResetButton(std::string_view field) const {
  const Row* row = FindRow(field);
  return row == nullptr ? nullptr : row->reset;
}

bool EncoderSettingsForm::IsMarked(std::string_view field) const {
  return encoder::EncoderOptionText(options_, field) !=
         encoder::EncoderOptionText(baseline_, field);
}

void EncoderSettingsForm::SetEditable(bool editable) {
  editable_ = editable;
  for (const Row& row : rows_) {
    row.label->setEnabled(editable);
    row.editor->setEnabled(editable);
  }
  // A reset button is enabled only where there is something to reset.
  UpdateMarks();
}

void EncoderSettingsForm::SetAdvancedShown(bool shown) {
  const QSignalBlocker blocker(advanced_button_);
  advanced_button_->setChecked(shown);
  UpdateAdvanced();
}

bool EncoderSettingsForm::AdvancedShown() const {
  return advanced_button_->isChecked();
}

const EncoderSettingsForm::Row* EncoderSettingsForm::FindRow(
    std::string_view field) const {
  for (const Row& row : rows_) {
    if (row.info->field == field) {
      return &row;
    }
  }
  return nullptr;
}

void EncoderSettingsForm::ShowValue(const Row& row) {
  loading_ = true;
  const std::string_view field = row.info->field;
  // The number in an option's text, if it is one.
  auto number_of = [field](const encoder::EncoderOptions& options) {
    return core::ParseNumber(encoder::EncoderOptionText(options, field));
  };
  switch (row.info->type) {
    case EncoderOptionType::kReal:
    case EncoderOptionType::kInteger:
      row.slider->SetDefault(number_of(baseline_));
      row.slider->SetValue(number_of(options_).value_or(0.0));
      break;
    case EncoderOptionType::kOptionalReal: {
      const std::optional<double> value = number_of(options_);
      const std::optional<double> base = number_of(baseline_);
      row.slider->SetDefault(base);
      row.check->setChecked(value.has_value());
      row.slider->setEnabled(value.has_value());
      row.slider->SetValue(value.value_or(base.value_or(UnsetValue(field))));
      break;
    }
    case EncoderOptionType::kPreEmphasis: {
      const bool chirp = options_.pre_emphasis.chirp;
      const bool base_chirp = baseline_.pre_emphasis.chirp;
      row.kind->setCurrentIndex(chirp ? kChirpIndex : kFirstOrderIndex);
      row.slider->setEnabled(!chirp);
      row.slider->SetDefault(
          base_chirp ? std::nullopt
                     : std::optional<double>(baseline_.pre_emphasis.mu));
      if (!chirp) {
        row.slider->SetValue(options_.pre_emphasis.mu);
      } else {
        row.slider->SetValue(base_chirp ? kUnsetMu : baseline_.pre_emphasis.mu);
      }
      break;
    }
  }
  loading_ = false;
}

void EncoderSettingsForm::OnEdited(const Row& row) {
  if (loading_) {
    return;
  }
  const std::string_view field = row.info->field;
  std::string text;
  switch (row.info->type) {
    case EncoderOptionType::kReal:
      text = core::FormatNumber(row.slider->Value());
      break;
    case EncoderOptionType::kInteger:
      text = std::to_string(std::lround(row.slider->Value()));
      break;
    case EncoderOptionType::kOptionalReal:
      row.slider->setEnabled(row.check->isChecked());
      text = row.check->isChecked() ? core::FormatNumber(row.slider->Value())
                                    : std::string(encoder::kUnsetOptionText);
      break;
    case EncoderOptionType::kPreEmphasis:
      row.slider->setEnabled(row.kind->currentIndex() == kFirstOrderIndex);
      text = row.kind->currentIndex() == kChirpIndex
                 ? std::string("chirp")
                 : core::FormatNumber(row.slider->Value());
      break;
  }
  // The sliders keep within the option's range, so this cannot fail.
  if (const core::Status status =
          encoder::SetEncoderOption(options_, field, text);
      !status.IsOk()) {
    return;
  }
  UpdateMarks();
  emit Edited();
}

void EncoderSettingsForm::UpdateMarks() {
  for (const Row& row : rows_) {
    const std::string_view field = row.info->field;
    const bool marked = IsMarked(field);
    QFont font = row.label->font();
    font.setBold(marked);
    row.label->setFont(font);
    row.reset->setEnabled(editable_ && marked);
    const std::string base = encoder::EncoderOptionText(baseline_, field);
    row.reset->setToolTip(
        QStringLiteral("Back to %1 (%2)").arg(baseline_name_, ToQString(base)));
    row.reset->setAccessibleName(
        QStringLiteral("Reset %1").arg(ToQString(row.info->label)));
  }
  UpdateAdvanced();
}

void EncoderSettingsForm::UpdateAdvanced() {
  const bool shown = advanced_button_->isChecked();
  int marked = 0;
  for (const Row& row : rows_) {
    if (!row.info->advanced) {
      continue;
    }
    for (QWidget* widget : {static_cast<QWidget*>(row.label), row.editor,
                            static_cast<QWidget*>(row.reset)}) {
      widget->setVisible(shown);
    }
    if (IsMarked(row.info->field)) {
      ++marked;
    }
  }
  advanced_button_->setArrowType(shown ? Qt::DownArrow : Qt::RightArrow);
  advanced_button_->setText(
      marked == 0
          ? QStringLiteral("Advanced settings")
          : QStringLiteral("Advanced settings (%1 changed)").arg(marked));
}

}  // namespace speak5220::app
