/*
 * File:        task_runner.h
 * Module:      util
 * Purpose:     Runs background jobs: the interface, an inline runner for
 *              tests and the command line, and a thread pool
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_UTIL_TASK_RUNNER_H_
#define SPEAK5220_CORE_UTIL_TASK_RUNNER_H_

#include <functional>
#include <memory>

class QThreadPool;

namespace speak5220::core {

// Runs jobs, such as an import or an encoding, away from the caller.
// A job reports its result itself (through a callback it captures); the
// callback runs on whichever thread ran the job, so a GUI passes results
// back to its own thread with a queued signal (AGENTS.md §4.3.3).
class ITaskRunner {
 public:
  virtual ~ITaskRunner() = default;

  // Starts `job`. Thread-safe.
  virtual void Post(std::function<void()> job) = 0;
  // Blocks until every job posted so far has finished. Thread-safe, but
  // must not be called from a job.
  virtual void Wait() = 0;
};

// Runs each job at once, on the caller's thread, inside Post(). Used by
// unit tests and by the command-line tool. Thread-compatible.
class InlineTaskRunner : public ITaskRunner {
 public:
  void Post(std::function<void()> job) override;
  void Wait() override;
};

// Runs jobs on a QThreadPool of its own (QtCore), so Wait() waits only for
// this runner's jobs. Thread-safe.
class ThreadPoolTaskRunner : public ITaskRunner {
 public:
  // `threads` <= 0 uses one thread per core.
  explicit ThreadPoolTaskRunner(int threads = 0);
  ~ThreadPoolTaskRunner() override;
  ThreadPoolTaskRunner(const ThreadPoolTaskRunner&) = delete;
  ThreadPoolTaskRunner& operator=(const ThreadPoolTaskRunner&) = delete;

  void Post(std::function<void()> job) override;
  void Wait() override;

  // The most jobs that run at once.
  int ThreadCount() const;

 private:
  std::unique_ptr<QThreadPool> pool_;
};

}  // namespace speak5220::core

#endif  // SPEAK5220_CORE_UTIL_TASK_RUNNER_H_
