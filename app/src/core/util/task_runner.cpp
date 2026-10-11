/*
 * File:        task_runner.cpp
 * Module:      util
 * Purpose:     Runs background jobs: the interface, an inline runner for
 *              tests and the command line, and a thread pool
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/task_runner.h"

#include <QThreadPool>
#include <functional>
#include <memory>
#include <utility>

namespace speak5220::core {

void InlineTaskRunner::Post(std::function<void()> job) { job(); }

void InlineTaskRunner::Wait() {}

ThreadPoolTaskRunner::ThreadPoolTaskRunner(int threads)
    : pool_(std::make_unique<QThreadPool>()) {
  if (threads > 0) {
    pool_->setMaxThreadCount(threads);
  }
}

ThreadPoolTaskRunner::~ThreadPoolTaskRunner() { pool_->waitForDone(); }

void ThreadPoolTaskRunner::Post(std::function<void()> job) {
  pool_->start(std::move(job));
}

void ThreadPoolTaskRunner::Wait() { pool_->waitForDone(); }

}  // namespace speak5220::core
