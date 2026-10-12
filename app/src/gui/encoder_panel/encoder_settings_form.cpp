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
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/preprocess/preprocess.h"
#include "core/util/number_text.h"
#include "core/util/status.h"
#include "gui/encoder_panel/option_slider.h"

namespace speak5220::app {

namespace {

namespace encoder = core::encoder;
namespace preprocess = core::preprocess;
using encoder::EncoderOptionInfo;
using encoder::EncoderOptionType;
using encoder::OptionStage;

// Columns of options, in each group.
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
// one is set: a squelch under most breaths, a bass cut like Acorn's own
// phrases (HLD §9), the default resonance hold, and the coefficient
// Optimise tries.
constexpr double kUnsetSquelch = 30.0;
constexpr double kUnsetBassCut = 200.0;
constexpr double kUnsetResonanceHold = 14.0;
constexpr double kUnsetMu = 0.9;

// The slider's value for an optional option that is unset.
double UnsetValue(std::string_view field) {
  if (field == "squelch_db") {
    return kUnsetSquelch;
  }
  if (field == "bass_cut_hz") {
    return kUnsetBassCut;
  }
  return kUnsetResonanceHold;
}

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// A value of `info` as the slider shows it: "20 codes", "45 dB", "0.3",
// "30 ms" for 240 samples, "off" or "2.5:1" for a ratio, and "off" for an
// option at its off value ("2 frames", "off" for 1).
QString FormatValue(const EncoderOptionInfo& info, double value) {
  const QString number = QString::fromStdString(core::FormatNumber(value));
  if (!info.off_text.empty() && core::FormatNumber(value) == info.off_text) {
    return QStringLiteral("off");
  }
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

// The groups of the form, in order: the preprocessing options, the
// encoder's basic options, and its advanced ones.
struct Group {
  OptionStage stage;
  bool advanced;
};
constexpr std::array<Group, 3> kGroups = {{
    {.stage = OptionStage::kPreprocess, .advanced = false},
    {.stage = OptionStage::kEncoder, .advanced = false},
    {.stage = OptionStage::kEncoder, .advanced = true},
}};

bool InGroup(const EncoderOptionInfo& info, const Group& group) {
  return info.stage == group.stage &&
         (group.stage == OptionStage::kPreprocess ||
          info.advanced == group.advanced);
}

// A section's heading.
QLabel* Heading(const QString& text, QWidget* parent) {
  auto* label = new QLabel(text, parent);
  QFont font = label->font();
  font.setBold(true);
  label->setFont(font);
  return label;
}

}  // namespace

EncoderSettingsForm::EncoderSettingsForm(QWidget* parent)
    : QWidget(parent),
      preset_box_(new QComboBox(this)),
      advanced_button_(new QToolButton(this)) {
  auto* grid = new QGridLayout(this);
  grid->setContentsMargins(0, 0, 0, 0);
  const auto infos = encoder::EncoderOptionInfos();
  constexpr int kAllColumns = kOptionColumns * kColumnsPerOption;
  // Each group's options in metadata order, down the first column of the
  // group and then the second, under the group's heading row.
  std::vector<const EncoderOptionInfo*> ordered;
  std::vector<int> first_row;
  std::vector<int> group_rows;
  std::vector<std::size_t> group_of;
  int next_row = 0;
  for (std::size_t g = 0; g < kGroups.size(); ++g) {
    int count = 0;
    for (const EncoderOptionInfo& info : infos) {
      if (InGroup(info, kGroups[g])) {
        ordered.push_back(&info);
        group_of.push_back(g);
        ++count;
      }
    }
    // The heading: the preprocessing's with its presets, the encoder's,
    // or the button that shows the advanced options.
    QWidget* heading = nullptr;
    if (kGroups[g].stage == OptionStage::kPreprocess) {
      auto* box = new QWidget(this);
      auto* layout = new QHBoxLayout(box);
      layout->setContentsMargins(0, 0, 0, 0);
      layout->addWidget(Heading(QStringLiteral("Preprocessing"), box));
      auto* preset_label = new QLabel(QStringLiteral("Preset"), box);
      preset_label->setBuddy(preset_box_);
      layout->addSpacing(preset_label->fontMetrics().averageCharWidth() * 2);
      layout->addWidget(preset_label);
      layout->addWidget(preset_box_);
      layout->addStretch();
      heading = box;
    } else if (!kGroups[g].advanced) {
      heading = Heading(QStringLiteral("Encoder"), this);
    } else {
      heading = advanced_button_;
    }
    grid->addWidget(heading, next_row, 0, 1, kAllColumns, Qt::AlignLeft);
    first_row.push_back(next_row + 1);
    // A group has at least one row.
    group_rows.push_back(
        std::max(1, (count + kOptionColumns - 1) / kOptionColumns));
    next_row += 1 + group_rows.back();
  }
  for (const preprocess::PreprocessPreset& preset :
       preprocess::PreprocessPresets()) {
    preset_box_->addItem(ToQString(preset.name));
  }
  preset_box_->addItem(QStringLiteral("Custom"));
  preset_box_->setToolTip(
      QStringLiteral("A set of preprocessing settings; Custom when they "
                     "match none. The encoding is measured against the "
                     "preprocessed recording."));
  preset_box_->setAccessibleName(QStringLiteral("Preprocessing preset"));
  advanced_button_->setCheckable(true);
  advanced_button_->setAutoRaise(true);
  advanced_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  advanced_button_->setToolTip(
      QStringLiteral("Show the settings that fine-tune the encoding"));
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
    const std::size_t g = group_of[i];
    const int index = static_cast<int>(std::ranges::count(
        group_of.begin(), group_of.begin() + static_cast<std::ptrdiff_t>(i),
        g));
    const int r = first_row[g] + index % group_rows[g];
    const int c = (index / group_rows[g]) * kColumnsPerOption;
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
  connect(preset_box_, &QComboBox::activated, this,
          [this](int index) { OnPresetChosen(index); });
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
  preset_box_->setEnabled(editable);
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

void EncoderSettingsForm::OnPresetChosen(int index) {
  const auto presets = preprocess::PreprocessPresets();
  if (index < 0 || static_cast<std::size_t>(index) >= presets.size()) {
    // Custom keeps the settings as they are.
    UpdatePreset();
    return;
  }
  const preprocess::PreprocessOptions& chosen =
      presets[static_cast<std::size_t>(index)].options;
  if (chosen == options_.preprocess) {
    return;
  }
  options_.preprocess = chosen;
  for (const Row& row : rows_) {
    if (row.info->stage == OptionStage::kPreprocess) {
      ShowValue(row);
    }
  }
  UpdateMarks();
  emit Edited();
}

void EncoderSettingsForm::UpdatePreset() {
  const auto presets = preprocess::PreprocessPresets();
  const preprocess::PreprocessPreset* match =
      preprocess::MatchingPreset(options_.preprocess);
  const QSignalBlocker blocker(preset_box_);
  preset_box_->setCurrentIndex(match == nullptr
                                   ? static_cast<int>(presets.size())
                                   : static_cast<int>(match - presets.data()));
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
  UpdatePreset();
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
