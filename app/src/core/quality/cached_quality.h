/*
 * File:        cached_quality.h
 * Module:      quality
 * Purpose:     Converts between an encoding's quality assessment and the
 *              figures a project caches with the encoding
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_CACHED_QUALITY_H_
#define SPEAK5220_CORE_QUALITY_CACHED_QUALITY_H_

#include "core/project/phrase.h"
#include "core/quality/quality_assessor.h"

namespace speak5220::core::quality {

// The figures of `assessment` to cache with its encoding, under
// kMeasuresVersion. Pure.
project::CachedQuality ToCachedQuality(const QualityAssessment& assessment);

// The assessment the cached figures give: the measures' figures (their
// frame counts unknown, so 0) and the score made of them again. Pure.
QualityAssessment FromCachedQuality(const project::CachedQuality& cached);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_CACHED_QUALITY_H_
