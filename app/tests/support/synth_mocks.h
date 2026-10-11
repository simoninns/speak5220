/*
 * File:        synth_mocks.h
 * Module:      test support
 * Purpose:     gMock mock of ISynth, for the code that renders encodings
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_SYNTH_MOCKS_H_
#define SPEAK5220_TESTS_SUPPORT_SYNTH_MOCKS_H_

#include <cstdint>
#include <span>

#include "core/synth/synth.h"
#include "core/util/status.h"
#include "gmock/gmock.h"

namespace speak5220::testing {

class MockSynth : public core::synth::ISynth {
 public:
  MOCK_METHOD(core::StatusOr<core::synth::SynthResult>, Synthesize,
              (std::span<const std::uint8_t> phrase,
               core::synth::SynthPath path),
              (override));
};

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_SYNTH_MOCKS_H_
