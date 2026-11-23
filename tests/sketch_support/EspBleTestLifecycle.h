// The reserved commands for the peer test sketches.
//
// The autouse fixture in tests/peer/conftest.py sends one reserved command to
// the primary DUT and every connected peer after each test, from fixture
// teardown so that a failed test, and Ctrl-C, are cleaned up as well:
//
//   0x04 (EOT)  STOP  nothing connected, advertising or scanning
//
// The other asks a sketch whether its setup has finished:
//
//   0x05 (ENQ)  READY?  prints the sketch's ready line again
//
// A ready line printed once at the end of setup() is lost whenever the serial
// reader attaches after the board has booted, which on a fast-booting core and a
// USB-UART bridge is most of the time. A sketch hands the code that prints it to
// announceReady() instead, which prints it now and again on every READY?, and a
// test asks with the `ready` fixture until the line comes. A sketch whose setup
// failed never calls announceReady(), so READY? stays unanswered and the test
// fails on the line it was waiting for.
//
// There is nothing to restore. Every module holds one test and every module
// begins with an upload, which resets the board, so no state is ever carried
// from one test to the next. The command exists only so that a board stops
// using the radio the moment its test is over, rather than at the next upload:
// the fixture is shared with other projects, and a board left advertising shows
// up in their scans.
//
// A command arrives as its byte followed by '\n'. The reply means "the target
// state holds", not "the byte arrived": a sketch registers an action that
// starts the transition and a predicate that reports when it is complete, and
// update() prints STOPPED once the predicate is true. A reply that never comes
// costs one timeout and a note on the pytest side, never a failure, which is
// what a hung or crashed sketch looks like.
//
// Byte-dispatch sketch (filter() turns a reserved byte into '\0', which no
// command branch matches):
//
//   const char command = EspBleTestLifecycle::filter(Serial.read());
//   if (command == 's') ...
//
// Line-dispatch sketch (the command is a one-byte line):
//
//   const String line = Serial.readStringUntil('\n');
//   if (EspBleTestLifecycle::handleLine(line)) return;
//
// Both call EspBleTestLifecycle::update() in loop() after the library's update.
// Sketches built on one EspBle or EspBleClassic instance use the companion
// headers, which supply the action and the predicate; others register their own
// with begin().
#pragma once

#include <Arduino.h>

namespace EspBleTestLifecycle
{
constexpr char StopCommand = '\x04';
constexpr char ReadyQuery = '\x05';

typedef void (*Action)();
typedef bool (*Predicate)();

struct Hooks
{
  Action stop = nullptr;       // starts stopping everything
  Predicate stopped = nullptr; // true once nothing is on the air (nullptr: at once)
};

namespace detail
{
inline Hooks &hooks()
{
  static Hooks value;
  return value;
}

inline bool &pending()
{
  static bool value = false;
  return value;
}

inline Action &ready()
{
  static Action value = nullptr;
  return value;
}
}

inline void begin(const Hooks &hooks)
{
  detail::hooks() = hooks;
}

// Prints the ready line now and on every later READY?. Call it where setup()
// has succeeded, in place of printing the line directly.
inline void announceReady(Action print)
{
  detail::ready() = print;
  if (print != nullptr) print();
}

// Consumes a reserved byte: STOP starts the transition, READY? repeats the
// ready line once there is one. Any other byte returns false.
inline bool handle(char command)
{
  if (command == ReadyQuery)
  {
    if (detail::ready() != nullptr) detail::ready()();
    return true;
  }
  if (command != StopCommand)
  {
    return false;
  }
  detail::pending() = true;
  if (detail::hooks().stop != nullptr) detail::hooks().stop();
  return true;
}

// Serial.read() wrapper for byte-dispatch sketches: a reserved byte is consumed
// and comes back as '\0'; every other value comes back unchanged.
inline char filter(int value)
{
  const char command = static_cast<char>(value);
  return handle(command) ? '\0' : command;
}

// The same for sketches that read whole lines.
inline bool handleLine(const String &line)
{
  return line.length() == 1 && handle(line[0]);
}

// Prints the reply once the target state holds. Call it from loop() after the
// library's update() so that asynchronous events have landed.
inline void update()
{
  if (!detail::pending())
  {
    return;
  }
  const Predicate done = detail::hooks().stopped;
  if (done != nullptr && !done())
  {
    return;
  }
  Serial.println("STOPPED");
  detail::pending() = false;
}
}
