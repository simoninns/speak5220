/*
 * File:        third_party_notices.cpp
 * Module:      about
 * Purpose:     The libraries and other people's code in Speak5220, with
 *              their licences, for the About box
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/about/third_party_notices.h"

#include <QString>
#include <QStringList>
#include <array>
#include <span>
#include <string_view>

namespace speak5220::app {

namespace {

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// The third-party libraries, then the code the quality measures reproduce.
constexpr std::array kNotices = {
    ThirdPartyNotice{.name = "Qt 6",
                     .use = "The windows, threads and audio playback (Widgets, "
                            "Concurrent, Multimedia)",
                     .licence = "LGPL-3.0-only",
                     .copyright = "The Qt Company Ltd. and other contributors",
                     .url = "https://www.qt.io/"},
    ThirdPartyNotice{.name = "libsndfile",
                     .use = "Reading and writing WAV files",
                     .licence = "LGPL-2.1-or-later",
                     .copyright = "Erik de Castro Lopo and the libsndfile "
                                  "contributors",
                     .url = "https://libsndfile.github.io/libsndfile/"},
    ThirdPartyNotice{.name = "libsamplerate",
                     .use = "Resampling imported audio to 8 kHz",
                     .licence = "BSD-2-Clause",
                     .copyright = "Erik de Castro Lopo",
                     .url = "https://libsndfile.github.io/libsamplerate/"},
    ThirdPartyNotice{.name = "pocketfft (C++)",
                     .use = "The FFTs of the encoder and the quality measures",
                     .licence = "BSD-3-Clause",
                     .copyright = "Max-Planck-Society; author Martin Reinecke",
                     .url = "https://github.com/mreineck/pocketfft"},
    ThirdPartyNotice{.name = "libzip",
                     .use = "The .s5proj project file",
                     .licence = "BSD-3-Clause",
                     .copyright = "Dieter Baron and Thomas Klausner",
                     .url = "https://libzip.org/"},
    ThirdPartyNotice{.name = "yaml-cpp",
                     .use = "The project's YAML files",
                     .licence = "MIT",
                     .copyright = "Jesse Beder",
                     .url = "https://github.com/jbeder/yaml-cpp"},
    ThirdPartyNotice{.name = "libfvad",
                     .use = "Voice activity detection for the quality measures",
                     .licence = "BSD-3-Clause",
                     .copyright = "The WebRTC project authors; Daniel Pirch",
                     .url = "https://github.com/dpirch/libfvad"},
    ThirdPartyNotice{.name = "SPTK 4",
                     .use = "LPC and cepstral analysis for the spectral "
                            "envelope measure",
                     .licence = "Apache-2.0",
                     .copyright = "SPTK Working Group, Nagoya Institute of "
                                  "Technology",
                     .url = "https://github.com/sp-nitech/SPTK"},
    ThirdPartyNotice{.name = "REAPER (SPTK 4's copy)",
                     .use = "Pitch and voicing for the quality measures",
                     .licence = "Apache-2.0",
                     .copyright = "Google Inc.",
                     .url = "https://github.com/google/REAPER"},
    ThirdPartyNotice{.name = "WORLD's Harvest (SPTK 4's copy)",
                     .use = "Pitch and voicing, in place of REAPER",
                     .licence = "BSD-3-Clause",
                     .copyright = "Masanori Morise",
                     .url = "https://github.com/mmorise/World"},
    ThirdPartyNotice{.name = "spdlog",
                     .use = "Logging",
                     .licence = "MIT",
                     .copyright = "Gabi Melman",
                     .url = "https://github.com/gabime/spdlog"},
    ThirdPartyNotice{.name = "{fmt}",
                     .use = "Text formatting, through spdlog",
                     .licence = "MIT",
                     .copyright = "Victor Zverovich and the {fmt} "
                                  "contributors",
                     .url = "https://fmt.dev/"},
    ThirdPartyNotice{.name = "GoogleTest (with Google Mock)",
                     .use = "Testing Speak5220",
                     .licence = "BSD-3-Clause",
                     .copyright = "Google Inc.",
                     .url = "https://github.com/google/googletest",
                     .tests_only = true},
    ThirdPartyNotice{.name = "pystoi",
                     .use = "The intelligibility measure (ESTOI), ported "
                            "from its code",
                     .licence = "MIT",
                     .copyright = "Manuel Pariente",
                     .url = "https://github.com/mpariente/pystoi"},
    ThirdPartyNotice{.name = "NumPy",
                     .use = "The PCG64 generator and normal sampler of the "
                            "anchor vocoder's noise, reproduced from its code",
                     .licence = "BSD-3-Clause",
                     .copyright = "NumPy Developers; PCG64 by Melissa O'Neill",
                     .url = "https://numpy.org/"},
};

// The SPDX texts of the short licences, without their copyright lines.
constexpr std::string_view kBsd2 =
    "Redistribution and use in source and binary forms, with or without "
    "modification, are permitted provided that the following conditions "
    "are met:\n\n"
    "1. Redistributions of source code must retain the above copyright "
    "notice, this list of conditions and the following disclaimer.\n\n"
    "2. Redistributions in binary form must reproduce the above copyright "
    "notice, this list of conditions and the following disclaimer in the "
    "documentation and/or other materials provided with the "
    "distribution.\n\n"
    "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "
    "\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT "
    "LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR "
    "A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT "
    "HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, "
    "SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT "
    "LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, "
    "DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY "
    "THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT "
    "(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE "
    "OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.";

constexpr std::string_view kBsd3 =
    "Redistribution and use in source and binary forms, with or without "
    "modification, are permitted provided that the following conditions "
    "are met:\n\n"
    "1. Redistributions of source code must retain the above copyright "
    "notice, this list of conditions and the following disclaimer.\n\n"
    "2. Redistributions in binary form must reproduce the above copyright "
    "notice, this list of conditions and the following disclaimer in the "
    "documentation and/or other materials provided with the "
    "distribution.\n\n"
    "3. Neither the name of the copyright holder nor the names of its "
    "contributors may be used to endorse or promote products derived from "
    "this software without specific prior written permission.\n\n"
    "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "
    "\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT "
    "LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR "
    "A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT "
    "HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, "
    "SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT "
    "LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, "
    "DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY "
    "THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT "
    "(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE "
    "OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.";

constexpr std::string_view kMit =
    "Permission is hereby granted, free of charge, to any person obtaining "
    "a copy of this software and associated documentation files (the "
    "\"Software\"), to deal in the Software without restriction, including "
    "without limitation the rights to use, copy, modify, merge, publish, "
    "distribute, sublicense, and/or sell copies of the Software, and to "
    "permit persons to whom the Software is furnished to do so, subject to "
    "the following conditions:\n\n"
    "The above copyright notice and this permission notice shall be "
    "included in all copies or substantial portions of the Software.\n\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, "
    "EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF "
    "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND "
    "NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE "
    "LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION "
    "OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION "
    "WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.";

// The short licences, in the order their texts are shown.
constexpr std::array<std::string_view, 3> kShortLicences = {
    "BSD-2-Clause", "BSD-3-Clause", "MIT"};

// Paragraphs of plain text as HTML.
QString Paragraphs(std::string_view text) {
  QString html;
  for (const QString& paragraph :
       ToQString(text).split(QStringLiteral("\n\n"))) {
    html += QStringLiteral("<p>%1</p>").arg(paragraph.toHtmlEscaped());
  }
  return html;
}

}  // namespace

std::span<const ThirdPartyNotice> ThirdPartyNotices() { return kNotices; }

std::string_view LicenceText(std::string_view spdx) {
  if (spdx == "BSD-2-Clause") {
    return kBsd2;
  }
  if (spdx == "BSD-3-Clause") {
    return kBsd3;
  }
  if (spdx == "MIT") {
    return kMit;
  }
  return {};
}

QString LicenceUrl(std::string_view spdx) {
  return QStringLiteral("https://spdx.org/licenses/%1.html")
      .arg(ToQString(spdx));
}

QString ThirdPartyNoticesHtml() {
  QString html = QStringLiteral(
      "<p>Speak5220 is built with the following software, each under its "
      "own licence.</p>");
  for (const ThirdPartyNotice& notice : kNotices) {
    const QString url = ToQString(notice.url).toHtmlEscaped();
    const QString licence = ToQString(notice.licence).toHtmlEscaped();
    html += QStringLiteral(
                "<h3>%1</h3><p>%2%3<br>Copyright %4<br>Licence: "
                "<a href=\"%5\">%6</a><br><a href=\"%7\">%7</a></p>")
                .arg(ToQString(notice.name).toHtmlEscaped(),
                     ToQString(notice.use).toHtmlEscaped(),
                     notice.tests_only
                         ? QStringLiteral(" (not part of the application)")
                         : QString(),
                     ToQString(notice.copyright).toHtmlEscaped(),
                     LicenceUrl(notice.licence).toHtmlEscaped(), licence, url);
  }
  for (const std::string_view spdx : kShortLicences) {
    QStringList holders;
    for (const ThirdPartyNotice& notice : kNotices) {
      if (notice.licence == spdx) {
        holders << ToQString(notice.name).toHtmlEscaped();
      }
    }
    html += QStringLiteral("<hr><h3>%1</h3><p>The licence of %2.</p>")
                .arg(ToQString(spdx).toHtmlEscaped(),
                     holders.join(QStringLiteral(", ")));
    html += Paragraphs(LicenceText(spdx));
  }
  return html;
}

}  // namespace speak5220::app
