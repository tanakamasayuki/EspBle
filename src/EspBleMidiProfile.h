#ifndef ESP_BLE_MIDI_PROFILE_H
#define ESP_BLE_MIDI_PROFILE_H

// BLE MIDI Device (Peripheral) and Host (Central) profile helpers built on the
// public EspBle GATT API plus the Arduino-independent packet codec in
// EspBleMidi.h. The API mirrors the USB siblings so code ports across
// transports: EspBleMidiDevice follows EspUsbDeviceMidi (constructed with a
// reference; noteOn/noteOff/controlChange/...), and EspBleMidiHost follows
// EspUsbHost's MIDI surface (onMidiMessage + send helpers, EspBleMidiMessage
// mirrors EspUsbHostMidiMessage).
//
// Both helpers install the single generic GATT callbacks they need (the device
// takes gattServer().onWritten/onSubscriptionChanged; the host takes
// onNotification/onCharacteristicDiscovered/onSubscribed), so a sketch that
// uses a MIDI helper should not also drive those callbacks itself. Timestamps
// are derived from millis() (the BLE MIDI 13-bit millisecond clock).

#include <Arduino.h>

#include "EspBle.h"
#include "EspBleMidi.h"

// Peripheral-side BLE MIDI. Register the service before EspBle::begin(), then
// send with the note/control helpers and receive host writes via onMessage().
class EspBleMidiDevice
{
public:
  explicit EspBleMidiDevice(EspBle &ble) : ble_(ble) {}

  using MessageCallback = std::function<void(const EspBleMidiMessage &message)>;

  // Register the MIDI GATT service, its I/O characteristic and the advertised
  // service UUID. Must be called before EspBle::begin() (GATT services are
  // registered before the server starts).
  bool begin()
  {
    EspBleGattCharacteristicConfig config;
    config.readable = true;              // spec: readable, returns empty
    config.writableWithoutResponse = true;
    config.writable = true;              // accept both, some hosts use write
    config.notifiable = true;
    if (!ble_.gattServer().addService(ESP_BLE_MIDI_SERVICE_UUID))
      return false;
    if (!ble_.gattServer().addCharacteristic(
          ESP_BLE_MIDI_SERVICE_UUID, ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID, config))
      return false;
    ble_.advertising().addServiceUuid(ESP_BLE_MIDI_SERVICE_UUID);

    ble_.gattServer().onWritten([this](const EspBleGattWrite &write) {
      if (!uuidEquals(write.characteristicUuid, ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID))
        return;
      deliverWrite(write.connectionId,
                   reinterpret_cast<const uint8_t *>(write.value.c_str()),
                   write.value.length());
    });
    ble_.gattServer().onSubscriptionChanged([this](const EspBleGattSubscription &subscription) {
      if (!uuidEquals(subscription.characteristicUuid, ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID))
        return;
      setSubscribed(subscription.connectionId, subscription.notifications);
    });
    return true;
  }

  // Callback for MIDI received from a connected host (host -> device).
  void onMessage(MessageCallback callback) { messageCallback_ = callback; }

  // True while at least one host is subscribed to notifications.
  bool ready() const { return subscriberCount_ > 0; }

  bool noteOn(uint8_t channel, uint8_t note, uint8_t velocity)
  {
    const uint8_t message[3] = {static_cast<uint8_t>(0x90 | (channel & 0x0F)),
                                static_cast<uint8_t>(note & 0x7F),
                                static_cast<uint8_t>(velocity & 0x7F)};
    return sendMessage(message, 3);
  }

  bool noteOff(uint8_t channel, uint8_t note, uint8_t velocity)
  {
    const uint8_t message[3] = {static_cast<uint8_t>(0x80 | (channel & 0x0F)),
                                static_cast<uint8_t>(note & 0x7F),
                                static_cast<uint8_t>(velocity & 0x7F)};
    return sendMessage(message, 3);
  }

  bool controlChange(uint8_t channel, uint8_t control, uint8_t value)
  {
    const uint8_t message[3] = {static_cast<uint8_t>(0xB0 | (channel & 0x0F)),
                                static_cast<uint8_t>(control & 0x7F),
                                static_cast<uint8_t>(value & 0x7F)};
    return sendMessage(message, 3);
  }

  bool programChange(uint8_t channel, uint8_t program)
  {
    const uint8_t message[2] = {static_cast<uint8_t>(0xC0 | (channel & 0x0F)),
                                static_cast<uint8_t>(program & 0x7F)};
    return sendMessage(message, 2);
  }

  bool polyPressure(uint8_t channel, uint8_t note, uint8_t pressure)
  {
    const uint8_t message[3] = {static_cast<uint8_t>(0xA0 | (channel & 0x0F)),
                                static_cast<uint8_t>(note & 0x7F),
                                static_cast<uint8_t>(pressure & 0x7F)};
    return sendMessage(message, 3);
  }

  bool channelPressure(uint8_t channel, uint8_t pressure)
  {
    const uint8_t message[2] = {static_cast<uint8_t>(0xD0 | (channel & 0x0F)),
                                static_cast<uint8_t>(pressure & 0x7F)};
    return sendMessage(message, 2);
  }

  bool pitchBend(uint8_t channel, uint16_t value)
  {
    const uint8_t message[3] = {static_cast<uint8_t>(0xE0 | (channel & 0x0F)),
                                static_cast<uint8_t>(value & 0x7F),
                                static_cast<uint8_t>((value >> 7) & 0x7F)};
    return sendMessage(message, 3);
  }

  // Send one raw channel/system MIDI message (status byte + up to 2 data bytes)
  // as a single BLE MIDI notification carrying the current timestamp.
  bool sendMessage(const uint8_t *message, size_t length)
  {
    uint8_t buffer[8];
    EspBleMidiPacketBuilder builder(buffer, sizeof(buffer));
    if (!builder.appendMessage(nowTimestamp(), message, length))
      return false;
    return ble_.gattServer().notify(
      ESP_BLE_MIDI_SERVICE_UUID, ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID,
      builder.data(), builder.size());
  }

private:
  static constexpr size_t MaxSubscribers = 4;

  static uint16_t nowTimestamp() { return static_cast<uint16_t>(millis() & 0x1FFF); }

  static bool uuidEquals(const String &value, const char *uuid)
  {
    return value.equalsIgnoreCase(uuid);
  }

  void deliverWrite(EspBleConnectionId connectionId, const uint8_t *data, size_t length)
  {
    if (!messageCallback_)
      return;
    parser_.parse(data, length, [&](const EspBleMidiMessage &decoded) {
      EspBleMidiMessage message = decoded;
      message.connectionId = connectionId;
      messageCallback_(message);
    });
  }

  void setSubscribed(EspBleConnectionId connectionId, bool subscribed)
  {
    for (size_t i = 0; i < MaxSubscribers; ++i)
    {
      if (subscribers_[i].used && subscribers_[i].connectionId == connectionId)
      {
        if (!subscribed)
        {
          subscribers_[i].used = false;
          if (subscriberCount_ > 0)
            --subscriberCount_;
        }
        return;
      }
    }
    if (!subscribed)
      return;
    for (size_t i = 0; i < MaxSubscribers; ++i)
    {
      if (!subscribers_[i].used)
      {
        subscribers_[i].used = true;
        subscribers_[i].connectionId = connectionId;
        ++subscriberCount_;
        return;
      }
    }
  }

  struct Subscriber
  {
    bool used = false;
    EspBleConnectionId connectionId = 0;
  };

  EspBle &ble_;
  MessageCallback messageCallback_;
  EspBleMidiParser parser_;
  Subscriber subscribers_[MaxSubscribers];
  size_t subscriberCount_ = 0;
};

// Central-side BLE MIDI. After connecting (and completing security if enabled),
// call discover(connectionId) to find the MIDI service and subscribe; decoded
// messages arrive via onMidiMessage().
class EspBleMidiHost
{
public:
  explicit EspBleMidiHost(EspBle &ble) : ble_(ble) {}

  using MessageCallback = std::function<void(const EspBleMidiMessage &message)>;

  // Install the GATT client callbacks the host needs. Call once after
  // EspBle::begin().
  bool begin()
  {
    ble_.onNotification([this](const EspBleGattNotification &notification) {
      if (!uuidEquals(notification.characteristicUuid, ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID))
        return;
      deliverNotification(notification.connectionId,
                          reinterpret_cast<const uint8_t *>(notification.value.c_str()),
                          notification.value.length());
    });
    ble_.onCharacteristicDiscovered([this](const EspBleGattResult &result) {
      if (!uuidEquals(result.characteristicUuid, ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID))
        return;
      if (!result.success)
        return;
      ble_.subscribe(result.connectionId, ESP_BLE_MIDI_SERVICE_UUID,
                     ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID);
    });
    ble_.onSubscribed([this](const EspBleGattResult &result) {
      if (!uuidEquals(result.characteristicUuid, ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID))
        return;
      if (result.success)
        markReady(result.connectionId);
    });
    return true;
  }

  // Discover the MIDI I/O characteristic on a connection and subscribe to it.
  bool discover(EspBleConnectionId connectionId, uint32_t timeoutMilliseconds = 10000)
  {
    slotFor(connectionId); // reserve a parser/state slot
    return ble_.discoverCharacteristic(
      connectionId, ESP_BLE_MIDI_SERVICE_UUID, ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID,
      timeoutMilliseconds);
  }

  // True once discovery and subscription have completed for a connection.
  bool ready(EspBleConnectionId connectionId) const
  {
    for (size_t i = 0; i < MaxConnections; ++i)
      if (slots_[i].used && slots_[i].connectionId == connectionId)
        return slots_[i].ready;
    return false;
  }

  void onMidiMessage(MessageCallback callback) { messageCallback_ = callback; }

  bool sendNoteOn(EspBleConnectionId connectionId, uint8_t channel, uint8_t note, uint8_t velocity)
  {
    const uint8_t message[3] = {static_cast<uint8_t>(0x90 | (channel & 0x0F)),
                                static_cast<uint8_t>(note & 0x7F),
                                static_cast<uint8_t>(velocity & 0x7F)};
    return sendMessage(connectionId, message, 3);
  }

  bool sendNoteOff(EspBleConnectionId connectionId, uint8_t channel, uint8_t note, uint8_t velocity)
  {
    const uint8_t message[3] = {static_cast<uint8_t>(0x80 | (channel & 0x0F)),
                                static_cast<uint8_t>(note & 0x7F),
                                static_cast<uint8_t>(velocity & 0x7F)};
    return sendMessage(connectionId, message, 3);
  }

  bool sendControlChange(EspBleConnectionId connectionId, uint8_t channel, uint8_t control, uint8_t value)
  {
    const uint8_t message[3] = {static_cast<uint8_t>(0xB0 | (channel & 0x0F)),
                                static_cast<uint8_t>(control & 0x7F),
                                static_cast<uint8_t>(value & 0x7F)};
    return sendMessage(connectionId, message, 3);
  }

  bool sendProgramChange(EspBleConnectionId connectionId, uint8_t channel, uint8_t program)
  {
    const uint8_t message[2] = {static_cast<uint8_t>(0xC0 | (channel & 0x0F)),
                                static_cast<uint8_t>(program & 0x7F)};
    return sendMessage(connectionId, message, 2);
  }

  // Send one raw channel/system MIDI message to a connected device using a
  // Write Without Response.
  bool sendMessage(EspBleConnectionId connectionId, const uint8_t *message, size_t length)
  {
    uint8_t buffer[8];
    EspBleMidiPacketBuilder builder(buffer, sizeof(buffer));
    if (!builder.appendMessage(nowTimestamp(), message, length))
      return false;
    return ble_.writeCharacteristic(
      connectionId, ESP_BLE_MIDI_SERVICE_UUID, ESP_BLE_MIDI_IO_CHARACTERISTIC_UUID,
      builder.data(), builder.size(), false);
  }

private:
  static constexpr size_t MaxConnections = 4;

  static uint16_t nowTimestamp() { return static_cast<uint16_t>(millis() & 0x1FFF); }

  static bool uuidEquals(const String &value, const char *uuid)
  {
    return value.equalsIgnoreCase(uuid);
  }

  struct Slot
  {
    bool used = false;
    bool ready = false;
    EspBleConnectionId connectionId = 0;
    EspBleMidiParser parser;
  };

  Slot *slotFor(EspBleConnectionId connectionId)
  {
    for (size_t i = 0; i < MaxConnections; ++i)
      if (slots_[i].used && slots_[i].connectionId == connectionId)
        return &slots_[i];
    for (size_t i = 0; i < MaxConnections; ++i)
    {
      if (!slots_[i].used)
      {
        slots_[i] = Slot();
        slots_[i].used = true;
        slots_[i].connectionId = connectionId;
        return &slots_[i];
      }
    }
    return nullptr;
  }

  void markReady(EspBleConnectionId connectionId)
  {
    Slot *slot = slotFor(connectionId);
    if (slot)
      slot->ready = true;
  }

  void deliverNotification(EspBleConnectionId connectionId, const uint8_t *data, size_t length)
  {
    Slot *slot = slotFor(connectionId);
    if (slot == nullptr || !messageCallback_)
      return;
    slot->parser.parse(data, length, [&](const EspBleMidiMessage &decoded) {
      EspBleMidiMessage message = decoded;
      message.connectionId = connectionId;
      messageCallback_(message);
    });
  }

  EspBle &ble_;
  MessageCallback messageCallback_;
  Slot slots_[MaxConnections];
};

#endif // ESP_BLE_MIDI_PROFILE_H
