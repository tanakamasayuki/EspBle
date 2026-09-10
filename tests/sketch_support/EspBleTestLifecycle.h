// The reserved stop command for the peer test sketches.
//
// The autouse fixture in tests/peer/conftest.py sends one reserved command to
// the primary DUT and every connected peer after each test, from fixture
// teardown so that a failed test, and Ctrl-C, are cleaned up as well:
//
//   0x04 (EOT)  STOP  nothing connected, advertising or scanning
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
// Byte-dispatch sketch (filter() turns the stop byte into '\0', which no
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
}

inline void begin(const Hooks &hooks)
{
  detail::hooks() = hooks;
}

// Consumes the stop byte and starts the transition. Any other byte returns false.
inline bool handle(char command)
{
  if (command != StopCommand)
  {
    return false;
  }
  detail::pending() = true;
  if (detail::hooks().stop != nullptr) detail::hooks().stop();
  return true;
}

// Serial.read() wrapper for byte-dispatch sketches: the stop byte is consumed
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
