/*
 * File:        emulator_mocks.h
 * Module:      test support
 * Purpose:     gMock mocks of the emulator's interfaces: the VSM bus, the
 *              sample sink and the event observer
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_EMULATOR_MOCKS_H_
#define SPEAK5220_TESTS_SUPPORT_EMULATOR_MOCKS_H_

#include <cstdint>

#include "core/emulator/sample_sink.h"
#include "core/emulator/tms5220_event.h"
#include "core/emulator/vsm_bus.h"
#include "gmock/gmock.h"

namespace speak5220::testing {

class MockVsmBus : public core::emulator::IVsmBus {
 public:
  MOCK_METHOD(void, Load, (int nibble), (override));
  MOCK_METHOD(int, Read, (), (override));
  MOCK_METHOD(void, Dummy, (), (override));
  MOCK_METHOD(void, Branch, (), (override));
};

class MockSampleSink : public core::emulator::ISampleSink {
 public:
  MOCK_METHOD(void, OnSample, (std::int64_t index, std::int64_t slot, int code),
              (override));
};

class MockTms5220Observer : public core::emulator::ITms5220Observer {
 public:
  MOCK_METHOD(void, OnEvent, (const core::emulator::Tms5220Event& event),
              (override));
};

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_EMULATOR_MOCKS_H_
