/*
 * File:        export_dialog.cpp
 * Module:      export
 * Purpose:     The Export dialog: scope, format and its options, and the
 *              files it will write
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/export/export_dialog.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/project/project_export.h"
#include "core/util/status.h"

namespace speak5220::app {

namespace project = core::project;

namespace {

// The most file names the summary lists before "and N more".
constexpr qsizetype kMaxListedFiles = 6;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

ExportDialog::ExportDialog(const ExportStart& start, QWidget* parent)
    : QDialog(parent),
      project_(start.project),
      confirm_overwrite_(start.confirm_overwrite),
      set_presets_(start.set_presets),
      scope_phrases_(start.selected),
      preset_box_(new QComboBox(this)),
      save_preset_button_(new QPushButton(this)),
      delete_preset_button_(
          new QPushButton(QStringLiteral("&Delete Preset"), this)),
      preset_note_(new QLabel(this)),
      format_box_(new QComboBox(this)),
      all_button_(new QRadioButton(QStringLiteral("All phrases"), this)),
      selected_button_(new QRadioButton(this)),
      layout_label_(new QLabel(QStringLiteral("Layout:"), this)),
      layout_box_(new QComboBox(this)),
      licence_label_(new QLabel(QStringLiteral("Licence:"), this)),
      licence_edit_(new QLineEdit(this)),
      audio_label_(new QLabel(QStringLiteral("Audio:"), this)),
      audio_box_(new QComboBox(this)),
      name_edit_(new QLineEdit(start.base, this)),
      directory_edit_(new QLineEdit(start.directory, this)),
      summary_label_(new QLabel(this)) {
  setWindowTitle(QStringLiteral("Export"));

  preset_box_->setAccessibleName(QStringLiteral("Preset"));
  preset_box_->setToolTip(
      QStringLiteral("Fill the fields from an export saved in the project"));
  save_preset_button_->setToolTip(QStringLiteral(
      "Save these fields in the project as an export preset, named after "
      "the Name field"));
  delete_preset_button_->setToolTip(
      QStringLiteral("Remove the chosen preset from the project"));
  auto* preset_row = new QHBoxLayout;
  preset_row->addWidget(preset_box_, 1);
  preset_row->addWidget(save_preset_button_);
  preset_row->addWidget(delete_preset_button_);
  preset_note_->setWordWrap(true);
  save_preset_button_->setVisible(static_cast<bool>(set_presets_));
  delete_preset_button_->setVisible(static_cast<bool>(set_presets_));
  FillPresets({});

  for (const project::ExportFormatInfo& format : project::ExportFormats()) {
    format_box_->addItem(ToQString(format.label), ToQString(format.key));
  }

  auto* scope_group = new QButtonGroup(this);
  scope_group->addButton(all_button_);
  scope_group->addButton(selected_button_);
  all_button_->setChecked(true);
  auto* scope_row = new QHBoxLayout;
  scope_row->addWidget(all_button_);
  scope_row->addWidget(selected_button_);
  scope_row->addStretch();

  layout_box_->addItem(QStringLiteral("One file per PHROM"),
                       ToQString(project::kLayoutSeparate));
  layout_box_->addItem(QStringLiteral("All PHROMs in one file"),
                       ToQString(project::kLayoutConcatenated));

  licence_edit_->setPlaceholderText(ToQString(project::kDefaultCHeaderLicence));
  licence_edit_->setToolTip(
      QStringLiteral("The SPDX licence identifier written in the header"));

  audio_box_->addItem(QStringLiteral("Encoded (the chip's output)"),
                      ToQString(project::kAudioEncoded));
  audio_box_->addItem(QStringLiteral("Original (the 8 kHz source)"),
                      ToQString(project::kAudioOriginal));
  audio_box_->addItem(QStringLiteral("Preprocessed (what the encoder aims at)"),
                      ToQString(project::kAudioPreprocessed));

  name_edit_->setToolTip(
      QStringLiteral("The exported files are named from this"));
  auto* browse = new QPushButton(QStringLiteral("Browse..."), this);
  auto* directory_row = new QHBoxLayout;
  directory_row->addWidget(directory_edit_, 1);
  directory_row->addWidget(browse);

  summary_label_->setWordWrap(true);
  summary_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
  export_button_ = buttons->addButton(QStringLiteral("Export"),
                                      QDialogButtonBox::AcceptRole);

  auto* form = new QFormLayout;
  // QFormLayout gives a label a buddy only when it makes the label for a
  // widget, not for a row of them.
  auto* preset_label = new QLabel(QStringLiteral("&Preset:"), this);
  preset_label->setBuddy(preset_box_);
  form->addRow(preset_label, preset_row);
  form->addRow(QStringLiteral("Format:"), format_box_);
  form->addRow(QStringLiteral("Phrases:"), scope_row);
  form->addRow(layout_label_, layout_box_);
  form->addRow(licence_label_, licence_edit_);
  form->addRow(audio_label_, audio_box_);
  form->addRow(QStringLiteral("Name:"), name_edit_);
  form->addRow(QStringLiteral("Folder:"), directory_row);
  // QFormLayout names a field by its label only when it makes the label
  // itself from text.
  layout_label_->setBuddy(layout_box_);
  licence_label_->setBuddy(licence_edit_);
  audio_label_->setBuddy(audio_box_);
  directory_edit_->setAccessibleName(QStringLiteral("Folder"));
  browse->setToolTip(QStringLiteral("Choose the folder"));
  auto* layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(preset_note_);
  layout->addWidget(summary_label_);
  layout->addWidget(buttons);

  connect(format_box_, &QComboBox::currentIndexChanged, this,
          &ExportDialog::Update);
  connect(layout_box_, &QComboBox::currentIndexChanged, this,
          &ExportDialog::Update);
  connect(audio_box_, &QComboBox::currentIndexChanged, this,
          &ExportDialog::Update);
  connect(all_button_, &QRadioButton::toggled, this, &ExportDialog::Update);
  connect(licence_edit_, &QLineEdit::textChanged, this, &ExportDialog::Update);
  connect(name_edit_, &QLineEdit::textChanged, this, &ExportDialog::Update);
  connect(directory_edit_, &QLineEdit::textChanged, this,
          &ExportDialog::Update);
  connect(browse, &QPushButton::clicked, this, &ExportDialog::Browse);
  connect(preset_box_, &QComboBox::activated, this, &ExportDialog::LoadPreset);
  connect(save_preset_button_, &QPushButton::clicked, this,
          &ExportDialog::SavePreset);
  connect(delete_preset_button_, &QPushButton::clicked, this,
          &ExportDialog::DeletePreset);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Update();
}

ExportRequest ExportDialog::Request() const {
  ExportRequest request;
  project::ExportPreset& preset = request.preset;
  preset.name = name_edit_->text().trimmed().toStdString();
  preset.format = format_box_->currentData().toString().toStdString();
  if (selected_button_->isChecked()) {
    preset.scope = std::string(project::kScopeSelected);
    for (const QString& name : scope_phrases_) {
      preset.phrases.push_back(name.toStdString());
    }
  } else {
    preset.scope = std::string(project::kScopeAll);
  }
  if (const project::ExportFormatInfo* format =
          project::FindExportFormat(preset.format);
      format != nullptr) {
    if (format->takes_layout) {
      preset.layout = layout_box_->currentData().toString().toStdString();
    }
    if (format->takes_licence) {
      preset.licence = licence_edit_->text().trimmed().toStdString();
    }
    if (format->takes_audio) {
      preset.audio = audio_box_->currentData().toString().toStdString();
    }
  }
  request.directory = directory_edit_->text().trimmed();
  return request;
}

QString ExportDialog::Summary() const { return summary_label_->text(); }

QStringList ExportDialog::ExistingFiles() const {
  const ExportRequest request = Request();
  const core::StatusOr<std::vector<std::string>> names =
      project::ExportFileNames(*project_, request.preset, request.preset.name);
  QStringList existing;
  if (!names.IsOk() || request.directory.isEmpty()) {
    return existing;
  }
  const QDir directory(request.directory);
  for (const std::string& name : *names) {
    if (QFileInfo::exists(directory.filePath(ToQString(name)))) {
      existing << ToQString(name);
    }
  }
  return existing;
}

void ExportDialog::accept() {
  if (!can_export_) {
    return;
  }
  // Never replace files without asking; declining returns to the dialog.
  if (const QStringList existing = ExistingFiles();
      !existing.isEmpty() &&
      (!confirm_overwrite_ ||
       !confirm_overwrite_(Request().directory, existing))) {
    return;
  }
  QDialog::accept();
}

bool ExportDialog::CanExport() const { return can_export_; }

void ExportDialog::Update() {
  const QString format_key = format_box_->currentData().toString();
  const project::ExportFormatInfo* format =
      project::FindExportFormat(format_key.toStdString());
  const bool layout = format != nullptr && format->takes_layout;
  const bool licence = format != nullptr && format->takes_licence;
  const bool audio = format != nullptr && format->takes_audio;
  layout_label_->setVisible(layout);
  layout_box_->setVisible(layout);
  licence_label_->setVisible(licence);
  licence_edit_->setVisible(licence);
  audio_label_->setVisible(audio);
  audio_box_->setVisible(audio);

  selected_button_->setText(
      QStringLiteral("Selected phrases (%1)").arg(scope_phrases_.size()));
  selected_button_->setToolTip(scope_phrases_.join(QStringLiteral(", ")));
  selected_button_->setEnabled(!scope_phrases_.isEmpty());
  if (scope_phrases_.isEmpty() && selected_button_->isChecked()) {
    all_button_->setChecked(true);
  }

  const ExportRequest request = Request();
  QString summary;
  can_export_ = false;
  core::StatusOr<std::vector<std::string>> files =
      project::ExportFileNames(*project_, request.preset, request.preset.name);
  // A preset is saved under the Name field, and must be one that can run.
  can_save_ = files.IsOk();
  const bool replaces = PresetIndex(request.preset.name) >= 0;
  save_preset_button_->setText(replaces ? QStringLiteral("&Update Preset")
                                        : QStringLiteral("&Save as Preset"));
  save_preset_button_->setEnabled(can_save_);
  delete_preset_button_->setEnabled(
      !preset_box_->currentData().toString().isEmpty());
  preset_note_->setVisible(!preset_note_->text().isEmpty());
  if (!files.IsOk()) {
    summary = QStringLiteral("Cannot export: %1")
                  .arg(ToQString(files.GetStatus().Message()));
  } else if (const core::Status current =
                 project::CheckEncodingsCurrent(*project_, request.preset);
             !current.IsOk()) {
    summary = QStringLiteral("Cannot export: %1: re-encode it first")
                  .arg(ToQString(current.Message()));
  } else if (files->empty()) {
    summary = QStringLiteral("There is nothing to export.");
  } else {
    QStringList names;
    for (std::size_t i = 0;
         i < files->size() && std::cmp_less(i, kMaxListedFiles); ++i) {
      names << ToQString((*files)[i]);
    }
    if (std::cmp_greater(files->size(), kMaxListedFiles)) {
      names << QStringLiteral("and %1 more")
                   .arg(files->size() -
                        static_cast<std::size_t>(kMaxListedFiles));
    }
    summary = QStringLiteral("Writes %1 file%2: %3")
                  .arg(files->size())
                  .arg(files->size() == 1 ? QString() : QStringLiteral("s"))
                  .arg(names.join(QStringLiteral(", ")));
    if (request.directory.isEmpty()) {
      summary += QStringLiteral("\nChoose a folder to export into.");
    } else {
      can_export_ = true;
    }
  }
  summary_label_->setText(summary);
  export_button_->setEnabled(can_export_);
}

void ExportDialog::Browse() {
  const QString directory = QFileDialog::getExistingDirectory(
      this, QStringLiteral("Export Into"), directory_edit_->text());
  if (!directory.isEmpty()) {
    directory_edit_->setText(directory);
  }
}

int ExportDialog::PresetIndex(const std::string& name) const {
  const std::vector<project::ExportPreset>& presets =
      project_->Contents().exports;
  for (std::size_t i = 0; i < presets.size(); ++i) {
    if (presets[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void ExportDialog::FillPresets(const QString& chosen) {
  preset_box_->clear();
  preset_box_->addItem(QStringLiteral("None"), QString());
  for (const project::ExportPreset& preset : project_->Contents().exports) {
    preset_box_->addItem(ToQString(preset.name), ToQString(preset.name));
  }
  preset_box_->setCurrentIndex(std::max(0, preset_box_->findData(chosen)));
  preset_box_->setEnabled(preset_box_->count() > 1);
}

void ExportDialog::LoadPreset() {
  const int index =
      PresetIndex(preset_box_->currentData().toString().toStdString());
  if (index < 0) {
    preset_note_->clear();
    Update();
    return;
  }
  const project::ExportPreset& preset =
      project_->Contents().exports[static_cast<std::size_t>(index)];
  if (const int format = format_box_->findData(ToQString(preset.format));
      format >= 0) {
    format_box_->setCurrentIndex(format);
    preset_note_->clear();
  } else {
    preset_note_->setText(
        QStringLiteral("The preset's format, \"%1\", is not one Speak5220 "
                       "has.")
            .arg(ToQString(preset.format)));
  }
  if (preset.scope == project::kScopeSelected) {
    scope_phrases_.clear();
    for (const std::string& name : preset.phrases) {
      scope_phrases_ << ToQString(name);
    }
    selected_button_->setEnabled(!scope_phrases_.isEmpty());
    selected_button_->setChecked(true);
  } else {
    all_button_->setChecked(true);
  }
  layout_box_->setCurrentIndex(std::max(
      0,
      layout_box_->findData(ToQString(
          preset.layout.empty() ? project::kLayoutSeparate : preset.layout))));
  licence_edit_->setText(ToQString(preset.licence));
  audio_box_->setCurrentIndex(std::max(
      0, audio_box_->findData(ToQString(
             preset.audio.empty() ? project::kAudioEncoded : preset.audio))));
  name_edit_->setText(ToQString(preset.name));
  Update();
}

void ExportDialog::SavePreset() {
  if (!set_presets_ || !can_save_) {
    return;
  }
  project::ExportPreset preset = Request().preset;
  std::vector<project::ExportPreset> presets = project_->Contents().exports;
  const int index = PresetIndex(preset.name);
  QString text;
  if (index >= 0) {
    // Keys this version does not know are kept.
    project::ExportPreset& replaced = presets[static_cast<std::size_t>(index)];
    preset.unknown = replaced.unknown;
    replaced = preset;
    text = QStringLiteral("Update Export Preset %1");
  } else {
    presets.push_back(preset);
    text = QStringLiteral("Save Export Preset %1");
  }
  const QString name = ToQString(preset.name);
  if (const core::Status status =
          set_presets_(std::move(presets), text.arg(name));
      !status.IsOk()) {
    preset_note_->setText(QStringLiteral("Cannot save the preset: %1")
                              .arg(ToQString(status.Message())));
    return;
  }
  FillPresets(name);
  preset_note_->setText(QStringLiteral("Saved as the preset %1.").arg(name));
  Update();
}

void ExportDialog::DeletePreset() {
  const QString name = preset_box_->currentData().toString();
  const int index = PresetIndex(name.toStdString());
  if (!set_presets_ || index < 0) {
    return;
  }
  std::vector<project::ExportPreset> presets = project_->Contents().exports;
  presets.erase(presets.begin() + index);
  if (const core::Status status =
          set_presets_(std::move(presets),
                       QStringLiteral("Delete Export Preset %1").arg(name));
      !status.IsOk()) {
    preset_note_->setText(QStringLiteral("Cannot delete the preset: %1")
                              .arg(ToQString(status.Message())));
    return;
  }
  FillPresets({});
  preset_note_->setText(QStringLiteral("Deleted the preset %1.").arg(name));
  Update();
}

}  // namespace speak5220::app
