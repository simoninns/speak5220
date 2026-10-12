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
      scope_phrases_(start.selected),
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
  form->addRow(QStringLiteral("Format:"), format_box_);
  form->addRow(QStringLiteral("Phrases:"), scope_row);
  form->addRow(layout_label_, layout_box_);
  form->addRow(licence_label_, licence_edit_);
  form->addRow(audio_label_, audio_box_);
  form->addRow(QStringLiteral("Name:"), name_edit_);
  form->addRow(QStringLiteral("Folder:"), directory_row);
  auto* layout = new QVBoxLayout(this);
  layout->addLayout(form);
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
  selected_button_->setEnabled(!scope_phrases_.isEmpty());
  if (scope_phrases_.isEmpty() && selected_button_->isChecked()) {
    all_button_->setChecked(true);
  }

  const ExportRequest request = Request();
  QString summary;
  can_export_ = false;
  core::StatusOr<std::vector<std::string>> files =
      project::ExportFileNames(*project_, request.preset, request.preset.name);
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

}  // namespace speak5220::app
