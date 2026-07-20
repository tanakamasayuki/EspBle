#ifndef ESP_BLE_MIDI_H
#define ESP_BLE_MIDI_H

// BLE MIDI packet codec used by the EspBle MIDI Device and MIDI Host profiles.
// It converts between raw BLE MIDI characteristic payloads (as defined by the
// MMA/Apple "Specification for MIDI over Bluetooth Low Energy 1.0") and decoded
// MIDI messages, handling the timestamp header/low-byte encoding, MIDI running
// status, System Real-Time interleaving, and System Exclusive streams that span
// multiple BLE packets.
//
// The codec is Arduino-independent so it can be verified by host-side unit
// tests (tests/unit/midi). Timestamps are the 13-bit millisecond values BLE
// MIDI carries (0..8191, wrapping); wall-clock reconstruction is left to the
// caller. Wire layout of a packet:
//
//   Header  : 1 0 h h h h h h   (bit7=1, bit6=0, bits5..0 = timestamp bits 12..7)
//   Timestamp: 1 l l l l l l l  (bit7=1,          bits6..0 = timestamp bits 6..0)
//   followed by the MIDI message bytes; running status and omitted timestamps
//   are permitted.

#include <stddef.h>
#include <stdint.h>

// 128-bit UUIDs assigned by the BLE MIDI specification.
static constexpr char ESP_BLE_MIDI_SERVICE_UUID[] = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
static constexpr char ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID[] = "7772E5DB-3868-4112-A1A9-F2669D106BF3";

// A decoded MIDI message delivered by EspBleMidiParser. For channel and system
// messages, `status`/`data1`/`data2` mirror EspUsbHostMidiMessage and `raw`
// points at the full message bytes (status + data). For System Exclusive, the
// message is delivered as one or more chunks: `sysEx` is true, `sysExData`
// points at the payload bytes of this chunk (framing 0xF0/0xF7 excluded), and
// `sysExStart`/`sysExEnd` mark the first and last chunk of the stream. All
// pointers are valid only during the parser callback.
struct EspBleMidiMessage
{
  uint16_t connectionId = 0; // set by the BLE profile; 0 from the raw codec
  uint16_t timestampMs = 0;  // 13-bit BLE MIDI timestamp (0..8191)
  uint8_t status = 0;        // full status byte (running status resolved)
  uint8_t data1 = 0;
  uint8_t data2 = 0;
  uint8_t dataLength = 0;    // number of valid data bytes (0..2)
  const uint8_t *raw = nullptr; // status + data bytes (channel/system messages)
  size_t length = 0;         // number of bytes in `raw`
  bool sysEx = false;
  bool sysExStart = false;
  bool sysExEnd = false;
  const uint8_t *sysExData = nullptr;
  size_t sysExLength = 0;

  bool isChannelVoice() const { return status >= 0x80 && status < 0xF0; }
  bool isSystemRealTime() const { return status >= 0xF8; }
  uint8_t channel() const { return isChannelVoice() ? (status & 0x0F) : 0; }
  uint8_t command() const { return isChannelVoice() ? (status & 0xF0) : status; }
};

// Number of data bytes a MIDI status byte carries: 0, 1 or 2 for fixed-length
// messages; -1 if `status` is not a status byte; -2 for SysEx start (0xF0);
// -3 for SysEx end (0xF7).
inline int espBleMidiDataLength(uint8_t status)
{
  if ((status & 0x80) == 0)
    return -1;
  if (status < 0xC0)
    return 2; // Note Off/On, Poly Pressure, Control Change
  if (status < 0xE0)
    return 1; // Program Change, Channel Pressure
  if (status < 0xF0)
    return 2; // Pitch Bend
  switch (status)
  {
  case 0xF0:
    return -2; // System Exclusive start
  case 0xF1:
    return 1; // MIDI Time Code quarter frame
  case 0xF2:
    return 2; // Song Position Pointer
  case 0xF3:
    return 1; // Song Select
  case 0xF7:
    return -3; // System Exclusive end
  default:
    return 0; // F4/F5/F6 (0 data) and F8..FF System Real-Time
  }
}

// Stateful decoder for one MIDI stream. Running status, the last timestamp, and
// an in-progress System Exclusive persist across parse() calls so a SysEx that
// spans multiple BLE notifications is reassembled correctly.
class EspBleMidiParser
{
public:
  void reset()
  {
    runningStatus_ = 0;
    lastTimestamp_ = 0;
    inSysEx_ = false;
    pendingSysExStart_ = false;
  }

  bool inSysEx() const { return inSysEx_; }

  // Parse one BLE MIDI packet, invoking onMessage(const EspBleMidiMessage&) for
  // each decoded message or SysEx chunk. Returns false if the packet is
  // malformed; messages decoded before the error are still delivered.
  template <typename Fn>
  bool parse(const uint8_t *data, size_t length, Fn &&onMessage)
  {
    if (data == nullptr || length == 0)
      return true;

    const uint8_t header = data[0];
    if ((header & 0x80) == 0 || (header & 0x40) != 0)
      return false; // header must be 0b10xxxxxx
    const uint16_t timestampHigh = header & 0x3F;

    const uint8_t *sysExChunk = nullptr;
    size_t sysExChunkLength = 0;

    auto emitSysEx = [&](bool end, uint16_t timestamp) {
      EspBleMidiMessage message;
      message.timestampMs = timestamp;
      message.sysEx = true;
      message.sysExStart = pendingSysExStart_;
      message.sysExEnd = end;
      message.sysExData = sysExChunk;
      message.sysExLength = sysExChunkLength;
      onMessage(message);
      pendingSysExStart_ = false;
      sysExChunk = nullptr;
      sysExChunkLength = 0;
    };

    size_t index = 1;
    while (index < length)
    {
      const uint8_t byte = data[index];

      if (inSysEx_)
      {
        if ((byte & 0x80) == 0)
        {
          if (sysExChunkLength == 0)
            sysExChunk = &data[index];
          ++sysExChunkLength;
          ++index;
          continue;
        }
        // A high-bit byte inside SysEx is a timestamp preceding either the
        // terminating 0xF7 or an interleaved System Real-Time message. Flush
        // any accumulated payload first.
        if (sysExChunkLength > 0)
          emitSysEx(false, lastTimestamp_);
        if (index + 1 >= length)
          return false; // timestamp with no following byte
        const uint16_t timestamp = static_cast<uint16_t>((timestampHigh << 7) | (byte & 0x7F));
        lastTimestamp_ = timestamp;
        const uint8_t st = data[index + 1];
        index += 2;
        if ((st & 0x80) == 0)
          return false;
        if (st == 0xF7)
        {
          emitSysEx(true, timestamp);
          inSysEx_ = false;
        }
        else if (st >= 0xF8)
        {
          emitFixed(timestamp, st, nullptr, 0, onMessage);
        }
        else
        {
          return false; // only real-time may interrupt a SysEx payload
        }
        continue;
      }

      uint16_t timestamp;
      uint8_t status;
      size_t dataStart;
      if (byte & 0x80)
      {
        timestamp = static_cast<uint16_t>((timestampHigh << 7) | (byte & 0x7F));
        lastTimestamp_ = timestamp;
        if (index + 1 >= length)
          return false; // dangling timestamp
        const uint8_t next = data[index + 1];
        if (next & 0x80)
        {
          status = next;
          dataStart = index + 2;
        }
        else
        {
          if (runningStatus_ == 0)
            return false;
          status = runningStatus_;
          dataStart = index + 1;
        }
      }
      else
      {
        // No timestamp byte: running status data reusing the last timestamp.
        if (runningStatus_ == 0)
          return false;
        status = runningStatus_;
        timestamp = lastTimestamp_;
        dataStart = index;
      }

      if (status == 0xF0)
      {
        inSysEx_ = true;
        pendingSysExStart_ = true;
        runningStatus_ = 0;
        lastTimestamp_ = timestamp;
        index = dataStart;
        continue;
      }

      const int dataLength = espBleMidiDataLength(status);
      if (dataLength < 0)
        return false; // 0xF7 or malformed status where a message was expected
      if (dataStart + static_cast<size_t>(dataLength) > length)
        return false;

      uint8_t staged[3];
      staged[0] = status;
      for (int k = 0; k < dataLength; ++k)
        staged[1 + k] = data[dataStart + k];
      emitFixed(timestamp, status, staged, static_cast<size_t>(dataLength) + 1, onMessage);

      index = dataStart + static_cast<size_t>(dataLength);

      if (status < 0xF0)
        runningStatus_ = status; // channel voice sets running status
      else if (status < 0xF8)
        runningStatus_ = 0; // system common clears it (real-time leaves it)
    }

    if (inSysEx_ && sysExChunkLength > 0)
      emitSysEx(false, lastTimestamp_);
    return true;
  }

private:
  template <typename Fn>
  void emitFixed(uint16_t timestamp, uint8_t status, const uint8_t *raw, size_t length, Fn &&onMessage)
  {
    EspBleMidiMessage message;
    message.timestampMs = timestamp;
    message.status = status;
    message.raw = raw;
    message.length = length;
    if (length >= 2)
    {
      message.data1 = raw[1];
      message.dataLength = 1;
    }
    if (length >= 3)
    {
      message.data2 = raw[2];
      message.dataLength = 2;
    }
    onMessage(message);
  }

  uint8_t runningStatus_ = 0;
  uint16_t lastTimestamp_ = 0;
  bool inSysEx_ = false;
  bool pendingSysExStart_ = false;
};

// Incremental encoder for a single BLE MIDI packet. The caller provides the
// output buffer (typically sized to the connection's ATT_MTU - 3). Each append
// writes a timestamp byte plus the message; the packet header is written from
// the first appended message. Messages whose timestamp falls in a different
// 7-bit high window than the header are rejected so the caller flushes and
// starts a new packet. No running-status compression is applied: every message
// carries its full status byte, which every conformant receiver accepts.
class EspBleMidiPacketBuilder
{
public:
  EspBleMidiPacketBuilder(uint8_t *buffer, size_t capacity) : buffer_(buffer), capacity_(capacity) {}

  void reset()
  {
    length_ = 0;
    hasHeader_ = false;
    headerHigh_ = 0;
  }

  size_t size() const { return length_; }
  bool empty() const { return length_ == 0; }
  const uint8_t *data() const { return buffer_; }

  // Append a raw channel/system message (status byte + up to 2 data bytes).
  bool appendMessage(uint16_t timestampMs, const uint8_t *message, size_t messageLength)
  {
    if (message == nullptr || messageLength == 0 || messageLength > 3)
      return false;
    if ((message[0] & 0x80) == 0)
      return false; // must begin with a status byte

    timestampMs &= 0x1FFF;
    const uint8_t high = static_cast<uint8_t>((timestampMs >> 7) & 0x3F);
    const uint8_t low = static_cast<uint8_t>(timestampMs & 0x7F);

    if (!hasHeader_)
    {
      if (2 + messageLength > capacity_)
        return false;
      buffer_[0] = static_cast<uint8_t>(0x80 | high);
      length_ = 1;
      hasHeader_ = true;
      headerHigh_ = high;
    }
    else
    {
      if (high != headerHigh_)
        return false; // outside this packet's timestamp window
      if (length_ + 1 + messageLength > capacity_)
        return false;
    }

    buffer_[length_++] = static_cast<uint8_t>(0x80 | low);
    for (size_t k = 0; k < messageLength; ++k)
      buffer_[length_++] = message[k];
    return true;
  }

  bool appendNoteOn(uint16_t ts, uint8_t channel, uint8_t note, uint8_t velocity)
  {
    const uint8_t message[3] = {static_cast<uint8_t>(0x90 | (channel & 0x0F)),
                                static_cast<uint8_t>(note & 0x7F),
                                static_cast<uint8_t>(velocity & 0x7F)};
    return appendMessage(ts, message, 3);
  }

  bool appendNoteOff(uint16_t ts, uint8_t channel, uint8_t note, uint8_t velocity)
  {
    const uint8_t message[3] = {static_cast<uint8_t>(0x80 | (channel & 0x0F)),
                                static_cast<uint8_t>(note & 0x7F),
                                static_cast<uint8_t>(velocity & 0x7F)};
    return appendMessage(ts, message, 3);
  }

  bool appendControlChange(uint16_t ts, uint8_t channel, uint8_t control, uint8_t value)
  {
    const uint8_t message[3] = {static_cast<uint8_t>(0xB0 | (channel & 0x0F)),
                                static_cast<uint8_t>(control & 0x7F),
                                static_cast<uint8_t>(value & 0x7F)};
    return appendMessage(ts, message, 3);
  }

  bool appendProgramChange(uint16_t ts, uint8_t channel, uint8_t program)
  {
    const uint8_t message[2] = {static_cast<uint8_t>(0xC0 | (channel & 0x0F)),
                                static_cast<uint8_t>(program & 0x7F)};
    return appendMessage(ts, message, 2);
  }

private:
  uint8_t *buffer_ = nullptr;
  size_t capacity_ = 0;
  size_t length_ = 0;
  bool hasHeader_ = false;
  uint8_t headerHigh_ = 0;
};

#endif // ESP_BLE_MIDI_H
