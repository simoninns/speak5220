/*
 * File:        third_party_notices.h
 * Module:      about
 * Purpose:     The libraries and other people's code in Speak5220, with
 *              their licences, for the About box
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_ABOUT_THIRD_PARTY_NOTICES_H_
#define SPEAK5220_GUI_ABOUT_THIRD_PARTY_NOTICES_H_

#include <QString>
#include <span>
#include <string_view>

namespace speak5220::app {

// One library, or other code Speak5220 reproduces. A plain
// value type.
struct ThirdPartyNotice {
  std::string_view name;
  // What Speak5220 uses it for.
  std::string_view use;
  // The licence's SPDX identifier.
  std::string_view licence;
  std::string_view copyright;
  // The project's home page.
  std::string_view url;
  // Used to build or test Speak5220, but not part of the application.
  bool tests_only = false;
};

// Every entry, the third-party libraries first, then code ported from
// other projects. Pure.
std::span<const ThirdPartyNotice> ThirdPartyNotices();

// The full text of a short licence (BSD-2-Clause, BSD-3-Clause, MIT) by
// its SPDX identifier, with its copyright line left out; empty for
// others, which are linked instead (LicenceUrl()). Pure.
std::string_view LicenceText(std::string_view spdx);
// Where the licence's text is published (spdx.org). Pure.
QString LicenceUrl(std::string_view spdx);

// The notices as HTML for the About box: each entry with its use,
// licence and copyright, and then each short licence's full text. Pure.
QString ThirdPartyNoticesHtml();

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ABOUT_THIRD_PARTY_NOTICES_H_
