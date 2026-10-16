/*
 * File:        project_settings_dialog.cpp
 * Module:      project_settings
 * Purpose:     The Project Settings dialog: the project's name, its PHROMs'
 *              header strings and its ASCII map
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/project_settings/project_settings_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/packaging/acorn_phrom.h"
#include "core/project/phrom_allocator.h"
#include "core/project/project_export.h"
#include "core/util/status.h"

namespace speak5220::app {

namespace project = core::project;

namespace {

// The "ASCII associated" entries: one per printable character
// (speak5220 datasheet §12.8).
constexpr char kFirstAscii = ' ';
constexpr char kLastAscii = '~';
constexpr int kAsciiCount = kLastAscii - kFirstAscii + 1;

// How the BBC Micro knows a PHROM: its first header string starts so
// (Acorn Speech System User Guide, Section 7).
constexpr std::string_view kCopyrightMark = "(C)";

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// The lines of `edit`, without the empty lines at the end.
std::vector<std::string> LinesOf(const QPlainTextEdit* edit) {
  QStringList lines = edit->toPlainText().split(QLatin1Char('\n'));
  while (!lines.isEmpty() && lines.last().isEmpty()) {
    lines.removeLast();
  }
  std::vector<std::string> strings;
  strings.reserve(static_cast<std::size_t>(lines.size()));
  for (const QString& line : lines) {
    strings.push_back(line.toStdString());
  }
  return strings;
}

QString LinesText(const std::vector<std::string>& strings) {
  QStringList lines;
  for (const std::string& line : strings) {
    lines << ToQString(line);
  }
  return lines.join(QLatin1Char('\n'));
}

// The header bytes `strings` take, with a null after each.
int HeaderBytes(const std::vector<std::string>& strings) {
  std::size_t bytes = 0;
  for (const std::string& line : strings) {
    bytes += line.size() + 1;
  }
  return static_cast<int>(bytes);
}

QString CharacterName(char character) {
  return character == ' ' ? QStringLiteral("Space")
                          : QString(QLatin1Char(character));
}

}  // namespace

ProjectSettingsDialog::ProjectSettingsDialog(const project::Project& project,
                                             QWidget* parent)
    : QDialog(parent),
      project_(&project),
      tabs_(new QTabWidget(this)),
      problem_label_(new QLabel(this)) {
  setWindowTitle(QStringLiteral("Project Settings"));
  for (const project::PhromHeader& phrom : project.Settings().phroms) {
    own_strings_[phrom.number] = phrom.header_strings;
    unknown_[phrom.number] = phrom.unknown;
  }

  tabs_->addTab(MakeGeneralTab(), QStringLiteral("&General"));
  tabs_->addTab(MakePhromsTab(), QStringLiteral("&PHROMs"));
  tabs_->addTab(MakeAsciiTab(), QStringLiteral("&ASCII Map"));

  problem_label_->setWordWrap(true);
  problem_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  ok_button_ = buttons->button(QDialogButtonBox::Ok);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(tabs_);
  layout->addWidget(problem_label_);
  layout->addWidget(buttons);

  ShowPhrom();
  Update();
}

QWidget* ProjectSettingsDialog::MakeGeneralTab() {
  auto* tab = new QWidget(tabs_);
  const project::ProjectSettings& settings = project_->Settings();
  name_edit_ = new QLineEdit(ToQString(settings.name), tab);
  name_edit_->setPlaceholderText(QStringLiteral("Named after its file"));
  name_edit_->setToolTip(QStringLiteral(
      "The project's name: the title of a BBC Micro disc, and in the C "
      "headers' comments"));
  header_edit_ = new QPlainTextEdit(LinesText(settings.header_strings), tab);
  header_edit_->setTabChangesFocus(true);
  header_edit_->setToolTip(QStringLiteral(
      "The strings of every PHROM's header, one per line. Each PHROM gets "
      "its number after the second"));
  header_note_ = new QLabel(tab);
  header_note_->setWordWrap(true);

  auto* form = new QFormLayout;
  form->addRow(QStringLiteral("&Name:"), name_edit_);
  form->addRow(QStringLiteral("&Header strings:"), header_edit_);
  form->addRow(QString(), header_note_);
  auto* layout = new QVBoxLayout(tab);
  layout->addLayout(form);
  layout->addStretch();

  connect(name_edit_, &QLineEdit::textChanged, this,
          &ProjectSettingsDialog::Update);
  connect(header_edit_, &QPlainTextEdit::textChanged, this, [this] {
    Update();
    // A PHROM without strings of its own shows the project's.
    ShowPhrom();
  });
  return tab;
}

QWidget* ProjectSettingsDialog::MakePhromsTab() {
  auto* tab = new QWidget(tabs_);
  auto* intro = new QLabel(
      QStringLiteral("A PHROM can have header strings of its own instead "
                     "of the project's."),
      tab);
  intro->setWordWrap(true);
  phrom_spin_ = new QSpinBox(tab);
  phrom_spin_->setRange(1, project::kPhromBudget);
  phrom_spin_->setToolTip(QStringLiteral("The PHROM to show, from 1"));
  own_box_ = new QCheckBox(QStringLiteral("&Own header strings"), tab);
  own_box_->setToolTip(QStringLiteral(
      "Give this PHROM header strings of its own, one per line"));
  phrom_edit_ = new QPlainTextEdit(tab);
  phrom_edit_->setTabChangesFocus(true);
  phrom_edit_->setAccessibleName(QStringLiteral("PHROM header strings"));
  phrom_note_ = new QLabel(tab);
  phrom_note_->setWordWrap(true);
  own_list_ = new QLabel(tab);
  own_list_->setWordWrap(true);

  auto* form = new QFormLayout;
  form->addRow(QStringLiteral("P&HROM:"), phrom_spin_);
  form->addRow(QString(), own_box_);
  form->addRow(QStringLiteral("Header strings:"), phrom_edit_);
  form->addRow(QString(), phrom_note_);
  auto* layout = new QVBoxLayout(tab);
  layout->addWidget(intro);
  layout->addLayout(form);
  layout->addWidget(own_list_);
  layout->addStretch();

  connect(phrom_spin_, &QSpinBox::valueChanged, this,
          &ProjectSettingsDialog::ShowPhrom);
  connect(own_box_, &QCheckBox::toggled, this, [this](bool own) {
    if (showing_phrom_) {
      return;
    }
    const int number = phrom_spin_->value();
    if (own) {
      // Start from what the PHROM has now.
      project::ProjectSettings settings = project_->Settings();
      settings.header_strings = LinesOf(header_edit_);
      settings.phroms.clear();
      own_strings_[number] = project::PhromHeaderStrings(settings, number);
    } else {
      own_strings_.erase(number);
    }
    ShowPhrom();
    Update();
  });
  connect(phrom_edit_, &QPlainTextEdit::textChanged, this,
          &ProjectSettingsDialog::OnPhromEdited);
  return tab;
}

QWidget* ProjectSettingsDialog::MakeAsciiTab() {
  auto* tab = new QWidget(tabs_);
  auto* intro = new QLabel(
      QStringLiteral("The phrase each character speaks: a program speaks "
                     "it by the character's code. Each PHROM includes the "
                     "entries whose phrase it holds."),
      tab);
  intro->setWordWrap(true);
  auto* area = new QScrollArea(tab);
  area->setWidgetResizable(true);
  auto* rows = new QWidget(area);
  auto* form = new QFormLayout(rows);

  const project::ProjectSettings& settings = project_->Settings();
  for (int row = 0; row < kAsciiCount; ++row) {
    const auto character = static_cast<char>(kFirstAscii + row);
    auto* box = new QComboBox(rows);
    box->addItem(QStringLiteral("None"), QString());
    for (const project::Phrase& phrase : project_->Phrases()) {
      box->addItem(ToQString(phrase.name), ToQString(phrase.name));
    }
    if (const auto entry = settings.ascii_map.find(character);
        entry != settings.ascii_map.end()) {
      const QString name = ToQString(entry->second);
      int index = box->findData(name);
      if (index < 0) {
        box->addItem(QStringLiteral("%1 (no phrase of this name)").arg(name),
                     name);
        index = box->count() - 1;
      }
      box->setCurrentIndex(index);
    }
    box->setAccessibleName(
        QStringLiteral("Phrase for %1").arg(CharacterName(character)));
    // A label's & marks its shortcut key, so a literal one is doubled.
    form->addRow(QStringLiteral("%1 (%2):")
                     .arg(CharacterName(character).replace(
                         QLatin1Char('&'), QStringLiteral("&&")))
                     .arg(static_cast<int>(character)),
                 box);
    ascii_boxes_.push_back(box);
    connect(box, &QComboBox::currentIndexChanged, this,
            &ProjectSettingsDialog::Update);
  }
  area->setWidget(rows);

  auto* layout = new QVBoxLayout(tab);
  layout->addWidget(intro);
  layout->addWidget(area, 1);
  return tab;
}

QComboBox* ProjectSettingsDialog::AsciiBox(char character) const {
  if (character < kFirstAscii || character > kLastAscii) {
    return nullptr;
  }
  return ascii_boxes_[static_cast<std::size_t>(character - kFirstAscii)];
}

void ProjectSettingsDialog::ShowPhrom() {
  showing_phrom_ = true;
  const int number = phrom_spin_->value();
  const auto own = own_strings_.find(number);
  own_box_->setChecked(own != own_strings_.end());
  phrom_edit_->setEnabled(own != own_strings_.end());
  if (own != own_strings_.end()) {
    if (LinesOf(phrom_edit_) != own->second) {
      phrom_edit_->setPlainText(LinesText(own->second));
    }
    phrom_note_->setText(QStringLiteral("%1 of %2 bytes.")
                             .arg(HeaderBytes(own->second))
                             .arg(core::packaging::kHeaderStringsCapacity));
  } else {
    project::ProjectSettings settings = project_->Settings();
    settings.header_strings = LinesOf(header_edit_);
    settings.phroms.clear();
    phrom_edit_->setPlainText(
        LinesText(project::PhromHeaderStrings(settings, number)));
    phrom_note_->setText(
        QStringLiteral("The project's header strings, with "
                       "the PHROM's number."));
  }
  QStringList numbers;
  for (const auto& [own_number, strings] : own_strings_) {
    numbers << QString::number(own_number);
  }
  own_list_->setText(
      numbers.isEmpty()
          ? QStringLiteral("No PHROM has header strings of its own.")
          : QStringLiteral("PHROMs with header strings of their own: %1")
                .arg(numbers.join(QStringLiteral(", "))));
  showing_phrom_ = false;
}

void ProjectSettingsDialog::OnPhromEdited() {
  if (showing_phrom_ || !own_box_->isChecked()) {
    return;
  }
  const int number = phrom_spin_->value();
  own_strings_[number] = LinesOf(phrom_edit_);
  phrom_note_->setText(QStringLiteral("%1 of %2 bytes.")
                           .arg(HeaderBytes(own_strings_[number]))
                           .arg(core::packaging::kHeaderStringsCapacity));
  Update();
}

ProjectSettingsChoice ProjectSettingsDialog::Chosen() const {
  ProjectSettingsChoice choice;
  choice.identity.name = name_edit_->text().trimmed().toStdString();
  choice.identity.header_strings = LinesOf(header_edit_);
  for (const auto& [number, strings] : own_strings_) {
    const auto unknown = unknown_.find(number);
    choice.phrom_settings.phroms.push_back(
        {.number = number,
         .header_strings = strings,
         .unknown = unknown == unknown_.end() ? project::UnknownKeys{}
                                              : unknown->second});
  }
  for (std::size_t i = 0; i < ascii_boxes_.size(); ++i) {
    const QString name = ascii_boxes_[i]->currentData().toString();
    if (!name.isEmpty()) {
      choice.phrom_settings
          .ascii_map[static_cast<char>(kFirstAscii + static_cast<int>(i))] =
          name.toStdString();
    }
  }
  return choice;
}

QString ProjectSettingsDialog::Problem() const {
  const ProjectSettingsChoice choice = Chosen();
  if (const core::Status status =
          project::ValidateProjectHeaderStrings(choice.identity.header_strings);
      !status.IsOk()) {
    return QStringLiteral("Header strings: %1")
        .arg(ToQString(status.Message()));
  }
  for (const project::PhromHeader& phrom : choice.phrom_settings.phroms) {
    if (const core::Status status =
            core::packaging::ValidateHeaderStrings(phrom.header_strings);
        !status.IsOk()) {
      return QStringLiteral("PHROM %1's header strings: %2")
          .arg(phrom.number)
          .arg(ToQString(status.Message()));
    }
  }
  return {};
}

void ProjectSettingsDialog::Update() {
  const std::vector<std::string> strings = LinesOf(header_edit_);
  // The longest the second string gets is with the last PHROM's number.
  const int bytes =
      HeaderBytes(strings) +
      (strings.size() > 1
           ? static_cast<int>(
                 (" " + std::to_string(project::kPhromBudget)).size())
           : 0);
  QString note = QStringLiteral("%1 of %2 bytes, with PHROM %3's number.")
                     .arg(bytes)
                     .arg(core::packaging::kHeaderStringsCapacity)
                     .arg(project::kPhromBudget);
  if (strings.empty() || !strings.front().starts_with(kCopyrightMark)) {
    note += QStringLiteral(
        " A BBC Micro finds a PHROM only if its first string starts "
        "with (C).");
  }
  header_note_->setText(note);

  const QString problem = Problem();
  problem_label_->setText(problem);
  problem_label_->setVisible(!problem.isEmpty());
  ok_button_->setEnabled(problem.isEmpty());
}

void ProjectSettingsDialog::accept() {
  if (!Problem().isEmpty()) {
    return;
  }
  // Tried on a copy, so a choice the project refuses (the ASCII map can
  // take it past its budget) keeps the dialog open.
  project::Project trial = *project_;
  ProjectSettingsChoice choice = Chosen();
  core::Status status = trial.SetIdentity(choice.identity).GetStatus();
  if (status.IsOk()) {
    status = trial.SetPhromSettings(choice.phrom_settings).GetStatus();
  }
  if (!status.IsOk()) {
    problem_label_->setText(ToQString(status.Message()));
    problem_label_->setVisible(true);
    return;
  }
  QDialog::accept();
}

}  // namespace speak5220::app
