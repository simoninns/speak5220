/*
 * File:        quantiser.cc
 * Module:      encoder
 * Purpose:     Quantises reflection coefficients, pitch periods and energies
 *              to the TMS5220's coding tables
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/quantiser.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/lpc_analysis.h"
#include "core/lpc/coding_tables.h"

namespace speak5220::core::encoder {
namespace {

// The K tables hold 10-bit values scaled by 512 (§8.1, Table 8-6).
constexpr double kKScale = 512.0;
// Coordinate-search passes, and the relative gain a move must make.
constexpr int kSearchPasses = 3;
constexpr double kSearchGain = 1e-9;
// The moves tried for each index, in order.
constexpr std::array<int, 4> kMoves = {-1, 1, -2, 2};
// The highest energy index that codes a sound (15 is the stop).
constexpr int kLastEnergyIndex = 14;

// The log-area ratio of every table entry, per K.
struct LarTables {
  std::array<std::vector<double>, lpc::kKCount> lar;

  LarTables() {
    for (int i = 0; i < lpc::kKCount; ++i) {
      for (const int v : lpc::KTable(i)) {
        lar.at(static_cast<std::size_t>(i))
            .push_back(dsp::LogAreaRatio(static_cast<double>(v) / kKScale));
      }
    }
  }
};

const LarTables& Lars() {
  static const LarTables kTables;
  return kTables;
}

int NearestByLar(int i, double k) {
  const std::vector<double>& lar = Lars().lar.at(static_cast<std::size_t>(i));
  const double target = dsp::LogAreaRatio(k);
  int best = 0;
  double best_distance = std::abs(lar[0] - target);
  for (std::size_t j = 1; j < lar.size(); ++j) {
    const double distance = std::abs(lar[j] - target);
    if (distance < best_distance) {
      best_distance = distance;
      best = static_cast<int>(j);
    }
  }
  return best;
}

}  // namespace

std::vector<double> KValues(std::span<const int> indices) {
  std::vector<double> values;
  values.reserve(indices.size());
  for (std::size_t i = 0; i < indices.size(); ++i) {
    const std::span<const int> table = lpc::KTable(static_cast<int>(i));
    values.push_back(
        static_cast<double>(table[static_cast<std::size_t>(indices[i])]) /
        kKScale);
  }
  return values;
}

double QuantisedPredictionError(std::span<const int> indices,
                                std::span<const double> r) {
  const std::vector<double> k = KValues(indices);
  return dsp::PredictionError(dsp::StepUp(k), r);
}

QuantisedK QuantiseK(std::span<const double> k, std::span<const double> r) {
  const std::size_t order = k.size();
  std::vector<int> indices(order);
  for (std::size_t i = 0; i < order; ++i) {
    indices[i] = NearestByLar(static_cast<int>(i), k[i]);
  }
  double best = QuantisedPredictionError(indices, r);
  std::vector<int> trial;
  for (int pass = 0; pass < kSearchPasses; ++pass) {
    bool changed = false;
    for (std::size_t i = 0; i < order; ++i) {
      const int top =
          static_cast<int>(lpc::KTable(static_cast<int>(i)).size()) - 1;
      // The moves are taken from the index as it was before this K's
      // moves, as tms5220enc.py builds its candidate tuple once.
      const int from = indices[i];
      for (const int move : kMoves) {
        const int candidate = from + move;
        if (candidate < 0 || candidate > top) {
          continue;
        }
        trial = indices;
        trial[i] = candidate;
        const double value = QuantisedPredictionError(trial, r);
        if (value < best * (1.0 - kSearchGain)) {
          best = value;
          indices = trial;
          changed = true;
        }
      }
    }
    if (!changed) {
      break;
    }
  }
  return {.indices = std::move(indices), .error = best};
}

int PitchIndex(double period) {
  const double target = std::log(period);
  int best = 1;
  double best_distance = 0.0;
  for (std::size_t i = 1; i < lpc::kPitchTable.size(); ++i) {
    const double distance = std::abs(
        std::log(static_cast<double>(lpc::kPitchTable[i]) + 1.0) - target);
    if (i == 1 || distance < best_distance) {
      best_distance = distance;
      best = static_cast<int>(i);
    }
  }
  return best;
}

int EnergyIndex(double value) {
  if (value <= 0.0) {
    return 1;
  }
  int best = 1;
  double best_distance = 0.0;
  for (int i = 1; i <= kLastEnergyIndex; ++i) {
    const double distance = std::abs(std::log(
        static_cast<double>(lpc::kEnergyTable[static_cast<std::size_t>(i)]) /
        value));
    if (i == 1 || distance < best_distance) {
      best_distance = distance;
      best = i;
    }
  }
  return best;
}

}  // namespace speak5220::core::encoder
