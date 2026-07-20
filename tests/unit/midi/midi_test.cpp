// Host-side unit tests for the BLE MIDI packet codec (EspBleMidi.h): the
// timestamp header/low-byte encoding, running status, System Real-Time
// interleaving, and System Exclusive streams that span multiple BLE packets.
// The codec is Arduino-independent so it can be verified here with g++.

#include "EspBleMidi.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
int failures = 0;

void check(const char *name, bool condition)
{
  if (!condition)
  {
    std::printf("FAIL %s\n", name);
    ++failures;
  }
}

// A flattened copy of every message the parser emits, so tests can assert on
// them after parse() returns (the pointers in EspBleMidiMessage are only valid
// during the callback).
struct Captured
{
  uint16_t timestampMs = 0;
  uint8_t status = 0;
  uint8_t data1 = 0;
  uint8_t data2 = 0;
  uint8_t dataLength = 0;
  bool sysEx = false;
  bool sysExStart = false;
  bool sysExEnd = false;
  std::vector<uint8_t> raw;
  std::vector<uint8_t> sysExData;
};

std::vector<Captured> collect(EspBleMidiParser &parser, const uint8_t *data, size_t length, bool &ok)
{
  std::vector<Captured> out;
  ok = parser.parse(data, length, [&](const EspBleMidiMessage &m) {
    Captured c;
    c.timestampMs = m.timestampMs;
    c.status = m.status;
    c.data1 = m.data1;
    c.data2 = m.data2;
    c.dataLength = m.dataLength;
    c.sysEx = m.sysEx;
    c.sysExStart = m.sysExStart;
    c.sysExEnd = m.sysExEnd;
    for (size_t i = 0; i < m.length; ++i)
      c.raw.push_back(m.raw[i]);
    for (size_t i = 0; i < m.sysExLength; ++i)
      c.sysExData.push_back(m.sysExData[i]);
    out.push_back(c);
  });
  return out;
}
} // namespace

int main()
{
  // UUIDs match the MMA/Apple BLE-MIDI 1.0 specification.
  check("service uuid",
    std::strcmp(ESP_BLE_MIDI_SERVICE_UUID, "03B80E5A-EDE8-4B33-A751-6CE34EC4C700") == 0);
  check("io characteristic uuid",
    std::strcmp(ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID, "7772E5DB-3868-4112-A1A9-F2669D106BF3") == 0);

  // Single Note On. Header 0x81 -> timestampHigh=1; timestamp 0xA5 -> low=0x25.
  // Full timestamp = (1<<7)|0x25 = 165 ms.
  {
    const uint8_t packet[] = {0x81, 0xA5, 0x90, 0x3C, 0x40};
    EspBleMidiParser parser;
    bool ok = false;
    auto msgs = collect(parser, packet, sizeof(packet), ok);
    check("note on ok", ok);
    check("note on count", msgs.size() == 1);
    check("note on ts", msgs[0].timestampMs == 165);
    check("note on status", msgs[0].status == 0x90);
    check("note on data", msgs[0].dataLength == 2 && msgs[0].data1 == 0x3C && msgs[0].data2 == 0x40);
    check("note on not sysex", !msgs[0].sysEx);
  }

  // Two full messages in one packet, each with its own timestamp byte.
  {
    const uint8_t packet[] = {0x81, 0xA5, 0x90, 0x3C, 0x40, 0xA6, 0x80, 0x3C, 0x00};
    EspBleMidiParser parser;
    bool ok = false;
    auto msgs = collect(parser, packet, sizeof(packet), ok);
    check("pair ok", ok);
    check("pair count", msgs.size() == 2);
    check("pair note off ts", msgs[1].timestampMs == 166);
    check("pair note off status", msgs[1].status == 0x80);
    check("pair note off data", msgs[1].data1 == 0x3C && msgs[1].data2 == 0x00);
  }

  // Running status: the second message omits the status byte but keeps its own
  // timestamp byte. Status is inherited from the previous channel message.
  {
    const uint8_t packet[] = {0x81, 0xA5, 0x90, 0x3C, 0x40, 0xA6, 0x40, 0x40};
    EspBleMidiParser parser;
    bool ok = false;
    auto msgs = collect(parser, packet, sizeof(packet), ok);
    check("running ok", ok);
    check("running count", msgs.size() == 2);
    check("running inherits status", msgs[1].status == 0x90);
    check("running ts", msgs[1].timestampMs == 166);
    check("running data", msgs[1].data1 == 0x40 && msgs[1].data2 == 0x40);
  }

  // Running status with the timestamp byte also omitted: the trailing data
  // bytes reuse the previous status and the previous timestamp.
  {
    const uint8_t packet[] = {0x81, 0xA5, 0x90, 0x3C, 0x40, 0x43, 0x40};
    EspBleMidiParser parser;
    bool ok = false;
    auto msgs = collect(parser, packet, sizeof(packet), ok);
    check("running-no-ts ok", ok);
    check("running-no-ts count", msgs.size() == 2);
    check("running-no-ts status", msgs[1].status == 0x90);
    check("running-no-ts ts", msgs[1].timestampMs == 165);
    check("running-no-ts data", msgs[1].data1 == 0x43 && msgs[1].data2 == 0x40);
  }

  // Program Change carries a single data byte.
  {
    const uint8_t packet[] = {0x81, 0xA5, 0xC0, 0x05};
    EspBleMidiParser parser;
    bool ok = false;
    auto msgs = collect(parser, packet, sizeof(packet), ok);
    check("program ok", ok);
    check("program count", msgs.size() == 1);
    check("program data length", msgs[0].dataLength == 1 && msgs[0].data1 == 0x05);
  }

  // A System Real-Time (Clock, 0xF8) message is 0 data bytes and does NOT
  // cancel running status: the following data bytes still use 0x90.
  {
    const uint8_t packet[] = {0x81, 0xA5, 0x90, 0x3C, 0x40, 0xA6, 0xF8, 0xA7, 0x44, 0x40};
    EspBleMidiParser parser;
    bool ok = false;
    auto msgs = collect(parser, packet, sizeof(packet), ok);
    check("realtime ok", ok);
    check("realtime count", msgs.size() == 3);
    check("realtime clock", msgs[1].status == 0xF8 && msgs[1].dataLength == 0 && msgs[1].timestampMs == 166);
    check("realtime keeps running status", msgs[2].status == 0x90 && msgs[2].data1 == 0x44);
  }

  // System Exclusive contained in a single packet: 0xF0 .. data .. timestamp 0xF7.
  {
    const uint8_t packet[] = {0x81, 0xA5, 0xF0, 0x7D, 0x01, 0x02, 0xA6, 0xF7};
    EspBleMidiParser parser;
    bool ok = false;
    auto msgs = collect(parser, packet, sizeof(packet), ok);
    check("sysex ok", ok);
    check("sysex count", msgs.size() == 2);
    check("sysex start chunk", msgs[0].sysEx && msgs[0].sysExStart && !msgs[0].sysExEnd);
    check("sysex start ts", msgs[0].timestampMs == 165);
    check("sysex payload", msgs[0].sysExData.size() == 3 &&
      msgs[0].sysExData[0] == 0x7D && msgs[0].sysExData[1] == 0x01 && msgs[0].sysExData[2] == 0x02);
    check("sysex end chunk", msgs[1].sysEx && !msgs[1].sysExStart && msgs[1].sysExEnd);
    check("sysex end ts", msgs[1].timestampMs == 166);
    check("sysex end empty", msgs[1].sysExData.empty());
  }

  // System Exclusive spanning two BLE packets. The parser keeps sysex state
  // across parse() calls; the continuation packet has no 0xF0 and its data
  // bytes follow the header immediately.
  {
    EspBleMidiParser parser;
    const uint8_t p1[] = {0x81, 0xA5, 0xF0, 0x11, 0x22};
    const uint8_t p2[] = {0x82, 0x33, 0x44, 0xA7, 0xF7};
    bool ok1 = false, ok2 = false;
    auto m1 = collect(parser, p1, sizeof(p1), ok1);
    auto m2 = collect(parser, p2, sizeof(p2), ok2);
    check("sysex split ok1", ok1);
    check("sysex split ok2", ok2);
    check("sysex split p1 chunk", m1.size() == 1 && m1[0].sysExStart && !m1[0].sysExEnd &&
      m1[0].sysExData.size() == 2 && m1[0].sysExData[0] == 0x11 && m1[0].sysExData[1] == 0x22);
    check("sysex split p2 count", m2.size() == 2);
    check("sysex split p2 mid", !m2[0].sysExStart && !m2[0].sysExEnd &&
      m2[0].sysExData.size() == 2 && m2[0].sysExData[0] == 0x33 && m2[0].sysExData[1] == 0x44);
    check("sysex split p2 end", m2[1].sysExEnd && m2[1].timestampMs == (2 * 128 + 0x27));
  }

  // Malformed: a data byte at the start of a packet with no running status
  // established must be rejected rather than silently reinterpreted.
  {
    const uint8_t packet[] = {0x81, 0x3C, 0x40};
    EspBleMidiParser parser;
    bool ok = true;
    auto msgs = collect(parser, packet, sizeof(packet), ok);
    check("malformed rejected", !ok);
    check("malformed no messages", msgs.empty());
  }

  // Bad header (bit6 set is not a valid header byte).
  {
    const uint8_t packet[] = {0xC1, 0xA5, 0x90, 0x3C, 0x40};
    EspBleMidiParser parser;
    bool ok = true;
    collect(parser, packet, sizeof(packet), ok);
    check("bad header rejected", !ok);
  }

  // Empty characteristic value (a BLE MIDI read returns 0 bytes) is valid and
  // yields no messages.
  {
    EspBleMidiParser parser;
    bool ok = false;
    auto msgs = collect(parser, nullptr, 0, ok);
    check("empty ok", ok && msgs.empty());
  }

  // Packet builder: one Note On produces exactly header + timestamp + message.
  {
    uint8_t buf[16];
    EspBleMidiPacketBuilder builder(buf, sizeof(buf));
    check("build note on", builder.appendNoteOn(165, 0, 0x3C, 0x40));
    const uint8_t expected[] = {0x81, 0xA5, 0x90, 0x3C, 0x40};
    check("build note on size", builder.size() == sizeof(expected));
    check("build note on bytes", std::memcmp(builder.data(), expected, sizeof(expected)) == 0);
  }

  // Packet builder: two messages share the packet header; each gets a timestamp.
  {
    uint8_t buf[32];
    EspBleMidiPacketBuilder builder(buf, sizeof(buf));
    check("build multi 1", builder.appendNoteOn(165, 0, 0x3C, 0x40));
    check("build multi 2", builder.appendNoteOff(166, 0, 0x3C, 0x00));
    const uint8_t expected[] = {0x81, 0xA5, 0x90, 0x3C, 0x40, 0xA6, 0x80, 0x3C, 0x00};
    check("build multi size", builder.size() == sizeof(expected));
    check("build multi bytes", std::memcmp(builder.data(), expected, sizeof(expected)) == 0);
  }

  // Packet builder: a message whose timestamp falls outside the packet's
  // 7-bit window (different high 6 bits) is rejected; caller must flush.
  {
    uint8_t buf[32];
    EspBleMidiPacketBuilder builder(buf, sizeof(buf));
    check("build window 1", builder.appendNoteOn(165, 0, 0x3C, 0x40));
    check("build window reject", !builder.appendNoteOn(300, 0, 0x3E, 0x40));
    check("build window size unchanged", builder.size() == 5);
  }

  // Packet builder: append that does not fit the buffer is rejected and leaves
  // the builder untouched.
  {
    uint8_t buf[4];
    EspBleMidiPacketBuilder builder(buf, sizeof(buf));
    check("build overflow reject", !builder.appendNoteOn(165, 0, 0x3C, 0x40));
    check("build overflow empty", builder.size() == 0);
  }

  // Round trip: build several messages, parse them back, compare.
  {
    uint8_t buf[32];
    EspBleMidiPacketBuilder builder(buf, sizeof(buf));
    builder.appendNoteOn(200, 3, 0x40, 0x7F);
    builder.appendControlChange(201, 3, 0x07, 0x64);
    EspBleMidiParser parser;
    bool ok = false;
    auto msgs = collect(parser, builder.data(), builder.size(), ok);
    check("roundtrip ok", ok && msgs.size() == 2);
    check("roundtrip note on", msgs[0].status == 0x93 && msgs[0].data1 == 0x40 &&
      msgs[0].data2 == 0x7F && msgs[0].timestampMs == 200);
    check("roundtrip cc", msgs[1].status == 0xB3 && msgs[1].data1 == 0x07 &&
      msgs[1].data2 == 0x64 && msgs[1].timestampMs == 201);
  }

  if (failures != 0)
  {
    std::printf("%d check(s) failed\n", failures);
    return 1;
  }
  std::puts("OK");
  return 0;
}
