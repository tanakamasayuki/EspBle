#ifndef ESP_BLE_H
#define ESP_BLE_H

#include <Arduino.h>
#include <functional>
#include <memory>
#include <mutex>
#include <sdkconfig.h>

#if !defined(CONFIG_NIMBLE_ENABLED) && !defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"
#endif

#include "EspBleKeymap.h"
#include "espble_version.h"

enum class EspBleError : uint8_t
{
  None = 0,
  InvalidState,
  InvalidArgument,
  BackendFailure,
  ResourceExhausted,
  NotFound,
  Timeout,
};

enum class EspBleSecurityIoCapability : uint8_t
{
  None = 0,
  DisplayOnly,
  KeyboardOnly,
  DisplayYesNo, // display + yes/no, required for Numeric Comparison
};

// Which address this device presents to peers.
// - Public: the factory public address (default).
// - RandomStatic: a random static address generated at begin() (a fixed random
//   identity that hides the public address but does not rotate).
// - ResolvablePrivate: a Resolvable Private Address (RPA) that the controller
//   rotates periodically (CONFIG_BT_NIMBLE_RPA_TIMEOUT, 900 s on the bundled
//   build). A bonded peer resolves it via the IRK exchanged at bonding; an
//   unbonded observer sees only a changing random address. Requires security
//   (bonding) to be usable across the rotation by a peer.
enum class EspBleOwnAddressType : uint8_t
{
  Public = 0,
  RandomStatic,
  ResolvablePrivate,
};

struct EspBleSecurityConfig
{
  bool enabled = false;
  bool bonding = true;
  bool pairOnConnect = true;
  bool mitm = false;
  EspBleSecurityIoCapability ioCapability = EspBleSecurityIoCapability::None;
  bool staticPasskeyEnabled = false;
  uint32_t staticPasskey = 0;
};

struct EspBleConfig
{
  const char *deviceName = "EspBle";
  // Preferred ATT MTU negotiated with the peer (23..517; the smaller of the two
  // sides wins). The default of 247 puts a single notification payload at 244
  // bytes instead of the 20 bytes the 23-byte spec minimum allows, which is what
  // almost every application wants. Lower it only to save RAM per connection.
  uint16_t preferredMtu = 247;
  EspBleSecurityConfig security;
  // When true (the default), a successful GATT client subscribe() is remembered
  // per peer address, and the subscription is restored automatically the next
  // time this central connects to the same peer — the application does not have
  // to re-subscribe after a reconnect. A successful unsubscribe() forgets it.
  // Relies on a stable peer address (a bonded/identity address, or a public or
  // static random address); set false to manage subscriptions manually.
  bool persistentSubscriptions = true;
  // Address privacy: the address type this device presents (default Public).
  // See EspBleOwnAddressType. RandomStatic / ResolvablePrivate are applied at
  // begin(); ResolvablePrivate is only useful together with security/bonding.
  EspBleOwnAddressType ownAddressType = EspBleOwnAddressType::Public;
};

struct EspBleScanConfig
{
  // Active scanning (the default) answers each advertisement with a Scan
  // Request and also receives the peer's Scan Response, which is where a device
  // name usually lives. Set it to false for a passive scan: quieter and lower
  // power because this device never transmits, but it only sees the
  // advertising payload.
  bool active = true;
  // When false (the default) each device is reported once per scan, which keeps
  // a scan for "is this device around?" quiet. Set it to true to receive every
  // advertisement, which is what a broadcaster whose payload changes over time
  // (a sensor beacon) requires -- with the filter on, only its first value
  // arrives and later ones look like they were never sent.
  bool wantDuplicates = false;
  uint16_t intervalMilliseconds = 100;
  uint16_t windowMilliseconds = 50;
  uint32_t durationSeconds = 0;
};

enum class EspBleAddressType : uint8_t
{
  Public = 0,
  Random,
  PublicIdentity,
  RandomIdentity,
};

// One Service Data block from an advertisement (AD type 0x16/0x20/0x21): a
// payload together with the service UUID it belongs to. uuid is reported in
// full 128-bit form even when advertised as a 16-bit value.
struct EspBleServiceData
{
  String uuid;
  String data;
};

struct EspBleScanResult
{
  static constexpr size_t MaxServiceUuids = 8;
  // An advertisement plus its scan response give 62 bytes, and each block costs
  // at least 5 (length + type + 16-bit UUID + one payload byte), so four covers
  // any realistic advertiser.
  static constexpr size_t MaxServiceData = 4;

  String address;
  EspBleAddressType addressType = EspBleAddressType::Public;
  String name;
  int rssi = 0;
  bool connectable = false;
  bool scannable = false;
  String manufacturerData;
  // Every Service Data block the advertisement carries, in the order received.
  // Most beacon formats use exactly one; look a specific one up by UUID with
  // serviceDataFor() rather than assuming an index.
  EspBleServiceData serviceData[MaxServiceData];
  size_t serviceDataCount = 0;
  String serviceUuids[MaxServiceUuids];
  size_t serviceUuidCount = 0;
  // Appearance (AD type 0x19): the device category a peer advertises, which
  // hosts use to pick an icon. 0 when the advertisement carries none.
  uint16_t appearance = 0;
  // Tx Power Level (AD type 0x0A) in dBm, as declared by the advertiser. Combine
  // it with rssi to estimate distance: the larger the gap, the further away.
  // Valid only when hasTxPowerLevel() is true, since 0 dBm is a legal value.
  int8_t txPowerLevel = 0;
  bool txPowerLevelPresent = false;

  bool hasName() const;
  bool hasManufacturerData() const;
  bool hasServiceData() const;
  bool hasAppearance() const;
  bool hasTxPowerLevel() const;
  // Copy the payload of the Service Data block whose UUID matches, comparing
  // UUIDs by value so a 16-bit shorthand matches its 128-bit form. Returns false
  // when no block carries that UUID.
  bool serviceDataFor(const char *uuid, String &data) const;
  bool advertisesService(const char *uuid) const;
};

enum class EspBleRole : uint8_t
{
  Central = 0,
  Peripheral,
};

using EspBleConnectionId = uint32_t;
using EspBleListenerId = uint32_t;
constexpr EspBleListenerId EspBleInvalidListenerId = 0;

// Multi-observer slot for one event: a single "primary" callback (set via the
// on*() setters, kept for the common single-observer case and backward
// compatibility) plus up to MaxListeners additional listeners (add*Listener()).
// The owner serializes every call with its own mutex; this type does no locking
// itself. Dispatch takes a snapshot under the lock and invokes it unlocked, so a
// callback may add/remove listeners without deadlocking or being invoked in the
// same dispatch. Removal shifts later slots down; listener ids are owner-unique.
template <typename Callback, size_t MaxListeners = 4>
class EspBleCallbackList
{
public:
  static constexpr size_t Capacity = MaxListeners + 1; // + primary

  void setPrimary(Callback callback)
  {
    primary_ = callback ? std::make_shared<Callback>(std::move(callback)) : nullptr;
  }

  // Store callback under listenerId (allocated by the owner). Returns listenerId
  // on success or EspBleInvalidListenerId if the list is full / callback empty.
  EspBleListenerId add(Callback callback, EspBleListenerId listenerId)
  {
    if (!callback || listenerId == EspBleInvalidListenerId) return EspBleInvalidListenerId;
    for (size_t i = 0; i < MaxListeners; ++i)
    {
      if (listeners_[i].id == EspBleInvalidListenerId)
      {
        listeners_[i].id = listenerId;
        listeners_[i].callback = std::make_shared<Callback>(std::move(callback));
        return listenerId;
      }
    }
    return EspBleInvalidListenerId;
  }

  bool remove(EspBleListenerId listenerId)
  {
    for (size_t i = 0; i < MaxListeners; ++i)
    {
      if (listeners_[i].id == listenerId)
      {
        for (size_t next = i + 1; next < MaxListeners; ++next)
        {
          listeners_[next - 1] = std::move(listeners_[next]);
        }
        listeners_[MaxListeners - 1] = Slot();
        return true;
      }
    }
    return false;
  }

  bool contains(EspBleListenerId listenerId) const
  {
    for (const Slot &slot : listeners_)
    {
      if (slot.id == listenerId) return true;
    }
    return false;
  }

  // Copy the primary (first) then each listener into out[], returning the count.
  // out must hold at least Capacity entries.
  size_t snapshot(std::shared_ptr<Callback> *out) const
  {
    size_t count = 0;
    if (primary_) out[count++] = primary_;
    for (const Slot &slot : listeners_)
    {
      if (slot.callback) out[count++] = slot.callback;
    }
    return count;
  }

private:
  struct Slot
  {
    EspBleListenerId id = EspBleInvalidListenerId;
    std::shared_ptr<Callback> callback;
  };
  std::shared_ptr<Callback> primary_;
  Slot listeners_[MaxListeners];
};

static constexpr uint8_t ESP_BLE_HID_REPORT_ID_KEYBOARD = 0x01;
static constexpr uint8_t ESP_BLE_HID_REPORT_ID_MOUSE = 0x02;
static constexpr uint8_t ESP_BLE_HID_REPORT_ID_GAMEPAD = 0x03;
static constexpr uint8_t ESP_BLE_HID_REPORT_ID_CONSUMER_CONTROL = 0x04;
static constexpr uint8_t ESP_BLE_HID_REPORT_ID_SYSTEM_CONTROL = 0x05;
static constexpr uint8_t ESP_BLE_HID_REPORT_ID_VENDOR = 0x06;

enum EspBleHidReportType : uint8_t
{
  ESP_BLE_HID_REPORT_TYPE_INPUT = 0x01,
  ESP_BLE_HID_REPORT_TYPE_OUTPUT = 0x02,
  ESP_BLE_HID_REPORT_TYPE_FEATURE = 0x03,
};

static constexpr uint8_t ESP_BLE_HID_MOUSE_LEFT = 0x01;
static constexpr uint8_t ESP_BLE_HID_MOUSE_RIGHT = 0x02;
static constexpr uint8_t ESP_BLE_HID_MOUSE_MIDDLE = 0x04;
static constexpr uint8_t ESP_BLE_HID_MOUSE_BACK = 0x08;
static constexpr uint8_t ESP_BLE_HID_MOUSE_FORWARD = 0x10;

static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_CENTER = 0x00;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_UP = 0x01;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_UP_RIGHT = 0x02;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_RIGHT = 0x03;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_DOWN_RIGHT = 0x04;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_DOWN = 0x05;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_DOWN_LEFT = 0x06;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_LEFT = 0x07;
static constexpr uint8_t ESP_BLE_HID_GAMEPAD_HAT_UP_LEFT = 0x08;

static constexpr uint16_t ESP_BLE_HID_CONSUMER_CONTROL_NEXT_TRACK = 0x00b5;
static constexpr uint16_t ESP_BLE_HID_CONSUMER_CONTROL_PREVIOUS_TRACK = 0x00b6;
static constexpr uint16_t ESP_BLE_HID_CONSUMER_CONTROL_PLAY_PAUSE = 0x00cd;
static constexpr uint16_t ESP_BLE_HID_CONSUMER_CONTROL_MUTE = 0x00e2;
static constexpr uint16_t ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_UP = 0x00e9;
static constexpr uint16_t ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_DOWN = 0x00ea;

static constexpr uint8_t ESP_BLE_HID_SYSTEM_CONTROL_POWER_OFF = 0x01;
static constexpr uint8_t ESP_BLE_HID_SYSTEM_CONTROL_STANDBY = 0x02;
static constexpr uint8_t ESP_BLE_HID_SYSTEM_CONTROL_WAKE_HOST = 0x03;

struct EspBleConnection
{
  EspBleConnectionId id = 0;
  uint16_t handle = 0xffff;
  String peerAddress;
  EspBleAddressType peerAddressType = EspBleAddressType::Public;
  EspBleRole localRole = EspBleRole::Central;
  uint16_t mtu = 23;
  bool encrypted = false;
  bool authenticated = false;
  bool bonded = false;
  uint8_t encryptionKeySize = 0;
  // Only meaningful in the onDisconnected() callback: the HCI reason code for
  // the disconnection, or 0 when the reason is unknown. A locally requested
  // disconnect, a remote termination, and a supervision timeout report distinct
  // codes, and the code a peer passed to disconnect() arrives here unchanged.
  // A reason that did not originate from HCI (a host-level failure) is reported
  // as the backend's own error value instead. 0 in onConnected()/onMtuChanged().
  int disconnectReason = 0;
  // Current connection parameters, populated at connect and refreshed on each
  // update delivered to onConnectionParametersUpdated(). connectionInterval is
  // in units of 1.25 ms, supervisionTimeout in units of 10 ms, and
  // peripheralLatency counts skipped connection events.
  uint16_t connectionInterval = 0;
  uint16_t peripheralLatency = 0;
  uint16_t supervisionTimeout = 0;
  // Current LE PHY per direction: 1 = 1M, 2 = 2M, 3 = Coded (0 if unknown).
  // Populated at connect and refreshed on each update delivered to onPhyUpdated().
  uint8_t txPhy = 0;
  uint8_t rxPhy = 0;

  size_t maximumNotificationPayload() const;
};

struct EspBleConnectionFailure
{
  String peerAddress;
  EspBleError error = EspBleError::BackendFailure;
  String detail;
};

struct EspBleMtuChanged
{
  EspBleConnection connection;
  uint16_t previousMtu = 23;
};

struct EspBleSecurityChanged
{
  EspBleConnection connection;
  bool success = false;
  EspBleError error = EspBleError::None;
  String detail;
};

struct EspBlePasskeyDisplayed
{
  EspBleConnection connection;
  uint32_t passkey = 0;
};

struct EspBleBond
{
  String peerAddress;
  EspBleAddressType peerAddressType = EspBleAddressType::Public;
};

struct EspBleGattCharacteristicConfig
{
  bool readable = false;
  bool writable = false;
  bool writableWithoutResponse = false;
  bool notifiable = false;
  bool indicatable = false;
  bool encryptedRead = false;
  bool encryptedWrite = false;
  bool authenticatedRead = false;
  bool authenticatedWrite = false;
};

struct EspBleGattDescriptorConfig
{
  bool readable = true;
  bool writable = false;
  bool encryptedRead = false;
  bool encryptedWrite = false;
  bool authenticatedRead = false;
  bool authenticatedWrite = false;
  uint16_t maximumLength = 100;
};

enum class EspBleGattOperation : uint8_t
{
  Discover = 0,
  Read,
  Write,
  Subscribe,
  Unsubscribe,
  DiscoverServices,
  ReadDescriptor,
  WriteDescriptor,
  // HID Host discovery runs as a queued operation on the shared GATT engine, so
  // it serializes with the generic operations instead of racing them.
  HidDiscover,
};

struct EspBleGattResult
{
  EspBleGattOperation operation = EspBleGattOperation::Discover;
  EspBleConnectionId connectionId = 0;
  String serviceUuid;
  String characteristicUuid;
  String descriptorUuid;
  // Attribute handle of the target characteristic. Populated for every
  // characteristic operation; the way to tell apart characteristics that share
  // a UUID (e.g. several HID Report characteristics).
  uint16_t handle = 0;
  bool success = false;
  EspBleError error = EspBleError::None;
  String detail;
  String value;
  bool readable = false;
  bool writable = false;
  bool writableWithoutResponse = false;
  bool notifiable = false;
  bool indicatable = false;
  bool subscribedToNotifications = false;
  bool subscribedToIndications = false;
  bool response = true;
};

struct EspBleGattServiceInfo
{
  String serviceUuid;
  uint16_t handle = 0;
};

struct EspBleGattCharacteristicInfo
{
  String serviceUuid;
  String characteristicUuid;
  uint16_t handle = 0;
  bool readable = false;
  bool writable = false;
  bool writableWithoutResponse = false;
  bool notifiable = false;
  bool indicatable = false;
};

struct EspBleGattDescriptorInfo
{
  String serviceUuid;
  String characteristicUuid;
  String descriptorUuid;
  uint16_t handle = 0;
  // Value handle of the characteristic this descriptor belongs to. The UUID pair
  // above cannot identify it when a peer repeats a UUID, so the handle is what
  // ties a CCCD to one specific characteristic.
  uint16_t characteristicHandle = 0;
};

// Opaque references to entries registered on the local GATT server. The add*
// calls return one and every later operation takes it, because the Bluetooth
// spec allows several Services or Characteristics to share a UUID: a UUID alone
// cannot say which one is meant. A default-constructed handle is invalid, and
// every call that takes an invalid handle fails with InvalidArgument.
struct EspBleGattService
{
  uint16_t id = 0;
  bool valid() const { return id != 0; }
  explicit operator bool() const { return valid(); }
};

struct EspBleGattCharacteristic
{
  uint16_t id = 0;
  bool valid() const { return id != 0; }
  explicit operator bool() const { return valid(); }
  bool operator==(const EspBleGattCharacteristic &other) const { return id == other.id; }
  bool operator!=(const EspBleGattCharacteristic &other) const { return id != other.id; }
};

struct EspBleGattDescriptor
{
  uint16_t id = 0;
  bool valid() const { return id != 0; }
  explicit operator bool() const { return valid(); }
  bool operator==(const EspBleGattDescriptor &other) const { return id == other.id; }
  bool operator!=(const EspBleGattDescriptor &other) const { return id != other.id; }
};

struct EspBleGattWrite
{
  EspBleConnectionId connectionId = 0;
  // Which characteristic was written. Compare against the handle kept from
  // addCharacteristic(); the UUIDs below are for logging and cannot tell apart
  // characteristics that share one.
  EspBleGattCharacteristic characteristic;
  String serviceUuid;
  String characteristicUuid;
  String value;
};

struct EspBleGattDescriptorWrite
{
  EspBleGattDescriptor descriptor;
  String serviceUuid;
  String characteristicUuid;
  String descriptorUuid;
  String value;
};

struct EspBleGattNotification
{
  EspBleConnectionId connectionId = 0;
  String serviceUuid;
  String characteristicUuid;
  // Attribute handle of the notifying characteristic; disambiguates characteristics
  // that share a UUID.
  uint16_t handle = 0;
  String value;
  bool indication = false;
};

struct EspBleGattSubscription
{
  EspBleConnectionId connectionId = 0;
  EspBleGattCharacteristic characteristic;
  String serviceUuid;
  String characteristicUuid;
  bool notifications = false;
  bool indications = false;
};

struct EspBleGattSendResult
{
  // The connection this send targeted, or 0 for a broadcast to all subscribers.
  EspBleConnectionId connectionId = 0;
  EspBleGattCharacteristic characteristic;
  String serviceUuid;
  String characteristicUuid;
  String value;
  bool indication = false;
  bool success = false;
  EspBleError error = EspBleError::None;
  String detail;
};

struct EspBleHidDeviceConfig
{
  const char *manufacturer = "EspBle";
  uint16_t vendorId = 0xffff;
  uint16_t productId = 0x0001;
  uint16_t productVersion = 0x0001;
  uint8_t countryCode = 0;
  uint8_t initialBatteryLevel = 100;
};

struct EspBleHidKeyboardConfig : EspBleHidDeviceConfig
{
  EspBleKeyboardLayout layout = EspBleKeyboardLayout::EnUs;
  // Expose HID over GATT Boot Protocol (Protocol Mode 0x2A4E + Boot Keyboard
  // Input/Output Reports 0x2A22/0x2A32). Off by default: most HOGP hosts use
  // Report Protocol Mode, and the extra characteristics enlarge every host's
  // discovery. Enable only for hosts that need Boot Protocol (e.g. a BIOS).
  bool bootProtocol = false;
};

struct EspBleHidMouseConfig : EspBleHidDeviceConfig
{
  uint8_t buttons = 5;
};

struct EspBleHidConsumerControlConfig : EspBleHidDeviceConfig {};
struct EspBleHidSystemControlConfig : EspBleHidDeviceConfig {};
struct EspBleHidGamepadConfig : EspBleHidDeviceConfig {};

struct EspBleHidVendorConfig : EspBleHidDeviceConfig
{
  uint8_t reportSize = 63;
};

struct EspBleHidKeyboardInputReport
{
  static constexpr uint8_t LeftControl = 0x01;
  static constexpr uint8_t LeftShift = 0x02;
  static constexpr uint8_t LeftAlt = 0x04;
  static constexpr uint8_t LeftGui = 0x08;
  static constexpr uint8_t RightControl = 0x10;
  static constexpr uint8_t RightShift = 0x20;
  static constexpr uint8_t RightAlt = 0x40;
  static constexpr uint8_t RightGui = 0x80;

  uint8_t modifiers = 0;
  uint8_t keys[6] = {};
};

struct EspBleHidMouseReport
{
  uint8_t buttons = 0;
  int8_t x = 0;
  int8_t y = 0;
  int8_t wheel = 0;
};

struct EspBleHidGamepadReport
{
  int8_t x = 0;
  int8_t y = 0;
  int8_t z = 0;
  int8_t rz = 0;
  int8_t rx = 0;
  int8_t ry = 0;
  uint8_t hat = ESP_BLE_HID_GAMEPAD_HAT_CENTER;
  uint32_t buttons = 0;
};

using EspBleHidKeyboardReport = EspBleHidKeyboardInputReport;

struct EspBleHidKeyboardOutputReport
{
  EspBleConnectionId connectionId = 0;
  uint8_t leds = 0;

  bool numLock() const { return (leds & 0x01) != 0; }
  bool capsLock() const { return (leds & 0x02) != 0; }
  bool scrollLock() const { return (leds & 0x04) != 0; }
  bool compose() const { return (leds & 0x08) != 0; }
  bool kana() const { return (leds & 0x10) != 0; }
};

struct EspBleHidKeyboardHostDiscovery
{
  EspBleConnectionId connectionId = 0;
  uint8_t reportId = 0;
  bool hasCountryCode = false;
  uint8_t countryCode = 0;
  bool hasOutputReport = false;
  bool hasBatteryLevel = false;
  uint8_t batteryLevel = 0;
  bool success = false;
  EspBleError error = EspBleError::None;
  String detail;
};

// Format-independent snapshot of the HID Keyboard/Keypad usage page.
// Modifier usages 0xe0-0xe7 are included in keys as well as modifiers.
struct EspBleHidKeyboardState
{
  static constexpr size_t BitmapSize = 32;

  EspBleConnectionId connectionId = 0;
  uint8_t reportId = 0;
  uint8_t keys[BitmapSize] = {};
  uint8_t changedKeys[BitmapSize] = {};
  uint8_t modifiers = 0;
  bool numLock = false;
  bool capsLock = false;
  bool scrollLock = false;
  bool compose = false;
  bool kana = false;

  bool isDown(uint8_t usage) const
  {
    return (keys[usage >> 3] & static_cast<uint8_t>(1u << (usage & 7))) != 0;
  }
  bool wasPressed(uint8_t usage) const
  {
    return isDown(usage) &&
      (changedKeys[usage >> 3] & static_cast<uint8_t>(1u << (usage & 7))) != 0;
  }
  bool wasReleased(uint8_t usage) const
  {
    return !isDown(usage) &&
      (changedKeys[usage >> 3] & static_cast<uint8_t>(1u << (usage & 7))) != 0;
  }
};

// EspBleKeyboardLayout is defined in EspBleKeymap.h.

struct EspBleHidReport
{
  EspBleConnectionId connectionId = 0;
  uint8_t reportId = 0;
  const uint8_t *rawData = nullptr;
  size_t rawLength = 0;
};

struct EspBleHidKeyboardEvent : EspBleHidReport
{
  uint8_t usage = 0;
  // Unicode code point produced by the selected layout (0 when the usage
  // produces no character). `ascii` is its ISO-8859-1 subset: the low byte
  // when the code point fits in 8 bits, otherwise 0.
  uint16_t unicode = 0;
  uint8_t ascii = 0;
  uint8_t modifiers = 0;
  bool pressed = false;
  bool released = false;
  bool numLock = false;
  bool capsLock = false;
  bool scrollLock = false;
  bool compose = false;
  bool kana = false;
};

struct EspBleHidMouseEvent : EspBleHidReport
{
  int16_t x = 0;
  int16_t y = 0;
  int16_t wheel = 0;
  uint8_t buttons = 0;
  uint8_t previousButtons = 0;
  bool moved = false;
  bool buttonsChanged = false;
};

struct EspBleHidConsumerControlEvent : EspBleHidReport
{
  uint16_t usage = 0;
  bool pressed = false;
  bool released = false;
};

struct EspBleHidSystemControlEvent : EspBleHidReport
{
  uint8_t usage = 0;
  bool pressed = false;
  bool released = false;
};

struct EspBleHidFieldValue
{
  uint8_t reportId = 0;
  uint16_t usagePage = 0;
  uint16_t usage = 0;
  int32_t value = 0;
  int32_t logicalMin = 0;
  int32_t logicalMax = 0;
  uint16_t bitOffset = 0;
  uint8_t bitSize = 0;
  uint8_t flags = 0;
};

struct EspBleHidGamepadEvent : EspBleHidReport
{
  const EspBleHidFieldValue *fields = nullptr;
  size_t fieldCount = 0;
  bool changed = false;
};

struct EspBleHidVendorReport : EspBleHidReport
{
  uint8_t reportType = 0;
  const uint8_t *data = nullptr;
  size_t length = 0;
};

struct EspBleHidVendorInputEvent : EspBleHidReport {};

// Backend type, forward declared so the public header stays free of NimBLE
// includes (only EspBleAdvertising's private renderer references it).
class BLEAdvertisementData;

class EspBle;
class EspBleAdvertising;
class EspBleScanner;
class EspBleGattServer;
class EspBleHidKeyboard;
class EspBleHidVendor;
class EspBleHidCustom;
class EspBleHidHost;
struct EspBleScannerImpl;
struct EspBleImpl;
struct EspBleGattServerImpl;
struct EspBleHidDeviceManagerImpl;
struct EspBleHidKeyboardHostImpl;

// Which peers are allowed to scan-request and connect while advertising. Any
// policy other than Any consults the accept list managed through
// EspBle::addToAcceptList(); with an empty accept list, a restricted policy
// rejects everyone. Enforced by the controller, so a rejected peer never
// reaches the application.
enum class EspBleAdvertisingFilterPolicy : uint8_t
{
  Any = 0,                  // anyone may scan-request and connect (default)
  ScanRequestFromAcceptList,
  ConnectionFromAcceptList,
  Both,
};

// One legacy advertising payload (31 bytes). Two of these exist per advertiser:
// the advertising payload itself (EspBleAdvertising::data()) and the scan
// response (EspBleAdvertising::scanResponse()), which a scanner only receives
// when it performs an active scan. Splitting fields across the two doubles the
// space available. Flags are emitted automatically into the advertising payload
// and are not permitted in a scan response.
class EspBleAdvertisingData
{
public:
  static constexpr size_t MaxServiceUuids = 4;

  static constexpr size_t MaxServiceData = 4;

  void clear();
  void setName(const char *name);
  bool addServiceUuid(const char *uuid);
  void setManufacturerData(const uint8_t *data, size_t length);
  // Add a Service Data block (AD type 0x16 / 0x20 / 0x21 by UUID size). Several
  // blocks may be added, each with its own UUID; adding the same UUID twice
  // replaces the earlier block. Add the same UUID with addServiceUuid() too if
  // scanners should also discover it via the service-UUID list. Passing no data
  // removes the block for that UUID.
  bool addServiceData(const char *uuid, const uint8_t *data, size_t length);
  void setAppearance(uint16_t appearance);
  // Include the Tx Power Level AD type (0x0A). The controller fills in the
  // actual power, which lets a scanner estimate distance from it and the RSSI.
  void setTxPowerIncluded(bool included);
  bool isEmpty() const;

private:
  friend class EspBleAdvertising;

  String name_;
  String manufacturerData_;
  EspBleServiceData serviceData_[MaxServiceData];
  size_t serviceDataCount_ = 0;
  String serviceUuids_[MaxServiceUuids];
  size_t serviceUuidCount_ = 0;
  uint16_t appearance_ = 0;
  bool txPowerIncluded_ = false;
};

class EspBleAdvertising
{
public:
  static constexpr size_t MaxServiceUuids = EspBleAdvertisingData::MaxServiceUuids;

  void clear();
  // The advertising payload. The setters below forward to it, so
  // setName("x") and data().setName("x") are the same call.
  EspBleAdvertisingData &data();
  // The scan response payload, sent only to active scanners. Empty by default;
  // while it is empty and scan response is enabled, the device name is placed
  // here automatically so it does not consume the advertising payload budget.
  // Putting anything in it takes over that placement entirely.
  EspBleAdvertisingData &scanResponse();

  void setName(const char *name);
  bool addServiceUuid(const char *uuid);
  void setManufacturerData(const uint8_t *data, size_t length);
  bool addServiceData(const char *uuid, const uint8_t *data, size_t length);
  void setAppearance(uint16_t appearance);
  void setScanResponseEnabled(bool enabled);
  // Restrict scan requests and/or connections to peers on the accept list.
  void setFilterPolicy(EspBleAdvertisingFilterPolicy policy);
  EspBleAdvertisingFilterPolicy filterPolicy() const;
  // Beacon support. setConnectable(false) advertises in a non-connectable mode
  // (a pure broadcaster / beacon); pair it with setScanResponseEnabled(false)
  // for non-connectable non-scannable advertising. setInterval() sets the
  // advertising interval in milliseconds (20..10240 ms; 0 restores the backend
  // default). The BLE spec requires >= 100 ms for non-connectable advertising.
  void setConnectable(bool connectable);
  bool setInterval(uint16_t minMilliseconds, uint16_t maxMilliseconds);
  bool start(uint32_t durationSeconds = 0);
  bool stop();
  bool isAdvertising() const;

private:
  friend class EspBle;

  explicit EspBleAdvertising(EspBle *owner);

  // Render one payload into the backend container, reporting an error through
  // owner_ when a field does not fit in the 31-byte legacy budget.
  bool buildPayload(
    const EspBleAdvertisingData &source,
    BLEAdvertisementData &destination,
    bool includeFlags,
    const char *payloadName) const;

  EspBle *owner_;
  EspBleAdvertisingData data_;
  EspBleAdvertisingData scanResponseData_;
  EspBleAdvertisingFilterPolicy filterPolicy_ = EspBleAdvertisingFilterPolicy::Any;
  bool scanResponseEnabled_ = true;
  bool connectable_ = true;
  uint16_t intervalMinMs_ = 0;
  uint16_t intervalMaxMs_ = 0;
};

class EspBleScanner
{
public:
  using ResultCallback = std::function<void(const EspBleScanResult &result)>;

  void onResult(ResultCallback callback);
  bool start(const EspBleScanConfig &config = EspBleScanConfig());
  bool stop();
  bool isScanning() const;
  size_t droppedResultCount() const;

private:
  friend class EspBle;
  friend struct EspBleScannerImpl;

  explicit EspBleScanner(EspBle *owner);
  ~EspBleScanner();
  void dispatchPendingResults();
  void flushPendingResults();

  EspBle *owner_;
  ResultCallback resultCallback_;
  EspBleScannerImpl *impl_ = nullptr;
};

class EspBleGattServer
{
public:
  static constexpr size_t MaxServices = 8;
  static constexpr size_t MaxCharacteristics = 32;
  static constexpr size_t MaxDescriptors = 16;
  using WriteCallback = std::function<void(const EspBleGattWrite &write)>;
  using DescriptorWriteCallback =
    std::function<void(const EspBleGattDescriptorWrite &write)>;
  using SubscriptionCallback = std::function<void(const EspBleGattSubscription &subscription)>;
  using SendCallback = std::function<void(const EspBleGattSendResult &result)>;

  // Register a Service. Two calls with the same UUID create two independent
  // instances, as the spec allows. Returns an invalid handle on failure.
  EspBleGattService addService(const char *serviceUuid);
  // Register a Characteristic inside a Service and return its handle; every
  // later operation takes that handle. Two characteristics in one service may
  // not share a UUID: the spec permits it, but the bundled backend reuses the
  // first entry instead of adding a second, so this is rejected rather than
  // silently dropped. Two services may share a UUID (see addService).
  EspBleGattCharacteristic addCharacteristic(
    EspBleGattService service,
    const char *characteristicUuid,
    const EspBleGattCharacteristicConfig &config);
  EspBleGattDescriptor addDescriptor(
    EspBleGattCharacteristic characteristic,
    const char *descriptorUuid,
    const EspBleGattDescriptorConfig &config = EspBleGattDescriptorConfig());

  bool setValue(
    EspBleGattCharacteristic characteristic, const uint8_t *data, size_t length);
  bool setValue(EspBleGattCharacteristic characteristic, const String &value);
  bool value(EspBleGattCharacteristic characteristic, String &value) const;
  bool setDescriptorValue(
    EspBleGattDescriptor descriptor, const uint8_t *data, size_t length);
  bool setDescriptorValue(EspBleGattDescriptor descriptor, const String &value);
  bool descriptorValue(EspBleGattDescriptor descriptor, String &value) const;

  // Broadcast to every subscriber of the characteristic.
  bool notify(EspBleGattCharacteristic characteristic, const uint8_t *data, size_t length);
  bool notify(EspBleGattCharacteristic characteristic, const String &value);
  bool indicate(EspBleGattCharacteristic characteristic, const uint8_t *data, size_t length);
  bool indicate(EspBleGattCharacteristic characteristic, const String &value);
  // Connection-scoped send: both target exactly one connection, and the
  // per-connection MTU applies. indicate(connectionId, …) waits for that
  // peer's confirmation before reporting to onSent(), so with several
  // subscribers each connection is confirmed on its own.
  bool notify(
    EspBleConnectionId connectionId,
    EspBleGattCharacteristic characteristic,
    const uint8_t *data,
    size_t length);
  bool notify(
    EspBleConnectionId connectionId,
    EspBleGattCharacteristic characteristic,
    const String &value);
  bool indicate(
    EspBleConnectionId connectionId,
    EspBleGattCharacteristic characteristic,
    const uint8_t *data,
    size_t length);
  bool indicate(
    EspBleConnectionId connectionId,
    EspBleGattCharacteristic characteristic,
    const String &value);
  // Primary observer (one per event; a second call replaces it).
  void onWritten(WriteCallback callback);
  void onDescriptorWritten(DescriptorWriteCallback callback);
  void onSubscriptionChanged(SubscriptionCallback callback);
  void onSent(SendCallback callback);
  // Additional observers that coexist with the primary and with each other, so
  // a profile helper and application code can both watch the same event. Returns
  // a listener id (EspBleInvalidListenerId if the list is full or callback is
  // empty); removeListener() drops one by id.
  EspBleListenerId addWrittenListener(WriteCallback callback);
  EspBleListenerId addDescriptorWrittenListener(DescriptorWriteCallback callback);
  EspBleListenerId addSubscriptionChangedListener(SubscriptionCallback callback);
  EspBleListenerId addSentListener(SendCallback callback);
  bool removeListener(EspBleListenerId listenerId);

private:
  friend class EspBle;
  friend struct EspBleImpl;
  friend struct EspBleGattServerImpl;

  explicit EspBleGattServer(EspBle *owner);
  ~EspBleGattServer();
  bool realize();
  void resetBackend();
  void dispatchWrite(const EspBleGattWrite &write);
  void dispatchDescriptorWrite(const EspBleGattDescriptorWrite &write);
  void dispatchSubscription(const EspBleGattSubscription &subscription);
  void dispatchSendResult(const EspBleGattSendResult &result);
  bool send(
    EspBleConnectionId connectionId,
    EspBleGattCharacteristic characteristic,
    const uint8_t *data,
    size_t length,
    bool indication);

  EspBleListenerId allocateListenerIdLocked();

  EspBle *owner_;
  EspBleGattServerImpl *impl_ = nullptr;
  mutable std::mutex listenerMutex_;
  EspBleListenerId nextListenerId_ = 1;
  EspBleCallbackList<WriteCallback> writtenListeners_;
  EspBleCallbackList<DescriptorWriteCallback> descriptorWrittenListeners_;
  EspBleCallbackList<SubscriptionCallback> subscriptionListeners_;
  EspBleCallbackList<SendCallback> sentListeners_;
};

class EspBleHidKeyboard
{
public:
  using OutputReportCallback =
    std::function<void(const EspBleHidKeyboardOutputReport &report)>;
  using ProtocolModeCallback =
    std::function<void(uint8_t mode, EspBleConnectionId connectionId)>;

  // HID over GATT Protocol Mode values (Protocol Mode characteristic 0x2A4E).
  static constexpr uint8_t BootProtocolMode = 0;
  static constexpr uint8_t ReportProtocolMode = 1;

  bool configure(
    const EspBleHidKeyboardConfig &config = EspBleHidKeyboardConfig());
  void enableNkro(bool enable = true);
  bool nkroEnabled() const;
  bool sendReport(const EspBleHidKeyboardReport &report);
  bool pressUsage(uint8_t usage, uint8_t modifiers = 0, uint32_t holdMs = 10);
  bool releaseUsage(uint8_t usage);
  bool tapUsage(uint8_t usage, uint8_t modifiers = 0, uint32_t holdMs = 10);
  bool pressKey(char key, uint32_t holdMs = 10);
  bool tapKey(char key, uint32_t holdMs = 10);
  bool write(const char *text, uint32_t interKeyDelayMs = 5);
  bool releaseAll();
  void setLayout(EspBleKeyboardLayout layout);
  EspBleKeyboardLayout layout() const;
  bool setBatteryLevel(uint8_t level);
  void onOutputReport(OutputReportCallback callback);
  // Current HID Protocol Mode (BootProtocolMode / ReportProtocolMode). The Host
  // selects it by writing the Protocol Mode characteristic; the default after a
  // connection is ReportProtocolMode.
  uint8_t protocolMode() const;
  void onProtocolMode(ProtocolModeCallback callback);
  bool configured() const;

private:
  friend class EspBle;
  friend class EspBleHidMouse;
  friend class EspBleHidConsumerControl;
  friend class EspBleHidSystemControl;
  friend class EspBleHidGamepad;
  friend class EspBleHidVendor;
  friend class EspBleHidCustom;
  friend struct EspBleHidDeviceManagerImpl;

  explicit EspBleHidKeyboard(EspBle *owner);
  ~EspBleHidKeyboard();
  bool configureProfile(uint8_t reportId, const EspBleHidDeviceConfig &config);
  bool configureCustom(const EspBleHidDeviceConfig &config);
  bool realize();
  bool sendRawReport(uint8_t reportId, const uint8_t *data, size_t length);
  bool sendCustomInput(uint8_t reportId, const uint8_t *data, size_t length);
  void resetBackend();
  void dispatchPendingOutputReports();
  void dispatchPendingProtocolMode();

  EspBle *owner_;
  EspBleHidDeviceManagerImpl *impl_ = nullptr;
  OutputReportCallback outputReportCallback_;
  ProtocolModeCallback protocolModeCallback_;
  EspBleKeyboardLayout layout_ = EspBleKeyboardLayout::EnUs;
  bool nkroEnabled_ = false;
  uint8_t nkroModifiers_ = 0;
  uint8_t nkroBitmap_[28] = {};
};

class EspBleHidMouse
{
public:
  bool configure(const EspBleHidMouseConfig &config = EspBleHidMouseConfig());
  bool configured() const;
  bool sendReport(const EspBleHidMouseReport &report);
  bool move(int8_t x, int8_t y, int8_t wheel = 0, uint8_t buttons = 0);
  bool wheel(int8_t amount);
  bool press(uint8_t buttons);
  bool release(uint8_t buttons);
  bool click(uint8_t button, uint32_t holdMs = 10);
  bool releaseAll();
  uint8_t buttons() const;

private:
  friend class EspBle;
  explicit EspBleHidMouse(EspBle *owner) : owner_(owner) {}
  EspBle *owner_;
  bool configured_ = false;
  uint8_t buttons_ = 0;
};

class EspBleHidConsumerControl
{
public:
  bool configure(const EspBleHidConsumerControlConfig &config = EspBleHidConsumerControlConfig());
  bool configured() const;
  bool sendReport(uint16_t usage);
  bool sendUsage(uint16_t usage);
  bool press(uint16_t usage);
  bool release();
  bool click(uint16_t usage, uint32_t holdMs = 10);
  bool releaseAll();
  uint16_t usage() const;

private:
  friend class EspBle;
  explicit EspBleHidConsumerControl(EspBle *owner) : owner_(owner) {}
  EspBle *owner_;
  bool configured_ = false;
  uint16_t usage_ = 0;
};

class EspBleHidSystemControl
{
public:
  bool configure(const EspBleHidSystemControlConfig &config = EspBleHidSystemControlConfig());
  bool configured() const;
  bool sendReport(uint8_t usage);
  bool sendUsage(uint8_t usage);
  bool press(uint8_t usage);
  bool release();
  bool click(uint8_t usage, uint32_t holdMs = 10);
  bool releaseAll();
  uint8_t usage() const;

private:
  friend class EspBle;
  explicit EspBleHidSystemControl(EspBle *owner) : owner_(owner) {}
  EspBle *owner_;
  bool configured_ = false;
  uint8_t usage_ = 0;
};

class EspBleHidGamepad
{
public:
  bool configure(const EspBleHidGamepadConfig &config = EspBleHidGamepadConfig());
  bool configured() const;
  bool sendReport(const EspBleHidGamepadReport &report);
  bool send(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry,
            uint8_t hat, uint32_t buttons);
  bool releaseAll();

private:
  friend class EspBle;
  explicit EspBleHidGamepad(EspBle *owner) : owner_(owner) {}
  EspBle *owner_;
  bool configured_ = false;
};

class EspBleHidVendor
{
public:
  using ReportCallback = std::function<void(const EspBleHidVendorReport &report)>;

  bool configure(const EspBleHidVendorConfig &config = EspBleHidVendorConfig());
  bool configured() const;
  bool sendInput(const void *data, size_t length);
  void onOutputReport(ReportCallback callback);
  void onFeatureReport(ReportCallback callback);

private:
  friend class EspBle;
  friend struct EspBleHidDeviceManagerImpl;
  explicit EspBleHidVendor(EspBle *owner) : owner_(owner) {}
  void dispatchPendingReports();

  EspBle *owner_;
  bool configured_ = false;
  ReportCallback outputCallback_;
  ReportCallback featureCallback_;
};

// Custom HID with an arbitrary Report Descriptor. Reports are composed into the
// same HID service as the fixed profiles (keyboard/mouse/...), so a custom
// report can coexist with them. Report IDs must be unique and, when a fixed
// profile is also enabled, must not use its reserved report ID (1..6).
class EspBleHidCustom
{
public:
  static constexpr size_t MaxReports = 4;
  using ReportCallback = std::function<void(const EspBleHidVendorReport &report)>;

  bool configure(const EspBleHidDeviceConfig &config = EspBleHidDeviceConfig());
  // Set the raw HID Report Descriptor bytes exposed as the Report Map (0x2A4B).
  bool setReportMap(const uint8_t *descriptor, size_t length);
  bool addInputReport(uint8_t reportId, uint16_t sizeBytes);
  bool addOutputReport(uint8_t reportId, uint16_t sizeBytes);
  bool addFeatureReport(uint8_t reportId, uint16_t sizeBytes);
  bool configured() const;
  bool sendInput(uint8_t reportId, const uint8_t *data, size_t length);
  void onOutputReport(ReportCallback callback);
  void onFeatureReport(ReportCallback callback);

private:
  friend class EspBle;
  friend struct EspBleHidDeviceManagerImpl;
  explicit EspBleHidCustom(EspBle *owner) : owner_(owner) {}
  bool addReport(uint8_t reportId, uint8_t reportType, uint16_t sizeBytes);
  void dispatchPendingReports();

  EspBle *owner_;
  bool configured_ = false;
  ReportCallback outputCallback_;
  ReportCallback featureCallback_;
};

class EspBleHidHost
{
public:
  static constexpr size_t MaxListenersPerEvent = 4;
  using DiscoveryCallback =
    std::function<void(const EspBleHidKeyboardHostDiscovery &result)>;
  using StateCallback = std::function<void(const EspBleHidKeyboardState &state)>;
  using KeyboardCallback = std::function<void(const EspBleHidKeyboardEvent &event)>;
  using MouseCallback = std::function<void(const EspBleHidMouseEvent &event)>;
  using ConsumerControlCallback = std::function<void(const EspBleHidConsumerControlEvent &event)>;
  using SystemControlCallback = std::function<void(const EspBleHidSystemControlEvent &event)>;
  using GamepadCallback = std::function<void(const EspBleHidGamepadEvent &event)>;
  using VendorInputCallback = std::function<void(const EspBleHidVendorInputEvent &event)>;

  bool discover(EspBleConnectionId connectionId);
  bool setKeyboardLeds(
    EspBleConnectionId connectionId,
    bool numLock,
    bool capsLock,
    bool scrollLock,
    bool compose = false,
    bool kana = false);
  bool sendVendorOutput(
    EspBleConnectionId connectionId, const uint8_t *data, size_t length);
  bool sendVendorFeature(
    EspBleConnectionId connectionId, const uint8_t *data, size_t length);
  void onDiscovered(DiscoveryCallback callback);
  void onKeyboardState(StateCallback callback);
  void onKeyboard(KeyboardCallback callback);
  void onMouse(MouseCallback callback);
  void onConsumerControl(ConsumerControlCallback callback);
  void onSystemControl(SystemControlCallback callback);
  void onGamepad(GamepadCallback callback);
  void onVendorInput(VendorInputCallback callback);
  EspBleListenerId addDiscoveredListener(DiscoveryCallback callback);
  EspBleListenerId addKeyboardStateListener(StateCallback callback);
  EspBleListenerId addKeyboardListener(KeyboardCallback callback);
  EspBleListenerId addMouseListener(MouseCallback callback);
  EspBleListenerId addConsumerControlListener(ConsumerControlCallback callback);
  EspBleListenerId addSystemControlListener(SystemControlCallback callback);
  EspBleListenerId addGamepadListener(GamepadCallback callback);
  EspBleListenerId addVendorInputListener(VendorInputCallback callback);
  bool removeListener(EspBleListenerId listenerId);
  void setKeyboardLayout(EspBleKeyboardLayout layout);
  EspBleKeyboardLayout keyboardLayout() const;
  bool ready(EspBleConnectionId connectionId) const;
  size_t droppedEventCount() const;
  size_t invalidInputReportCount() const;
  // Opt-in: after a HID peer that was discovered once reconnects and re-encrypts,
  // re-run discover() automatically (the HID Host does not use the generic
  // subscription registry, so it is not covered by persistentSubscriptions). Off
  // by default. Composes with a manual discover(): if the app still calls
  // discover() from onSecurityChanged, the automatic one is skipped for that
  // connection (no double discovery). Pair with setAutoReconnect() +
  // persistentSubscriptions for hands-off HID reconnection.
  void setAutoRediscover(bool enable);
  bool autoRediscover() const;

private:
  friend class EspBle;
  friend struct EspBleHidKeyboardHostImpl;

  static constexpr size_t MaxRediscoverPeers = 4;

  explicit EspBleHidHost(EspBle *owner);
  ~EspBleHidHost();
  void resetBackend();
  void handleDisconnected(EspBleConnectionId connectionId);
  // Called from EspBle's event dispatch on every SecurityChanged. When
  // auto-rediscover is on and the (Central) peer was discovered before, queues a
  // fresh discover() unless one is already pending for the connection.
  void handleSecurityEstablished(const EspBleSecurityChanged &event);
  void rememberRediscoverPeer(const String &address);
  void dispatchPendingEvents();
  // Launches the discovery worker for a HidDiscover operation dequeued by
  // EspBle::pumpGattQueue(). Returns false (and emits a failure discovery event)
  // if the worker task could not be created.
  bool runQueuedDiscovery(EspBleConnectionId connectionId);
  bool sendVendorReport(
    EspBleConnectionId connectionId,
    const uint8_t *data,
    size_t length,
    bool feature);

  EspBle *owner_;
  EspBleHidKeyboardHostImpl *impl_ = nullptr;

  template <typename Callback>
  struct ListenerSlot
  {
    EspBleListenerId id = EspBleInvalidListenerId;
    std::shared_ptr<Callback> callback;
  };

  template <typename Callback>
  EspBleListenerId addListener(ListenerSlot<Callback> *slots, Callback callback);

  template <typename Callback>
  static bool removeListenerFrom(
    ListenerSlot<Callback> *slots,
    EspBleListenerId listenerId);
  EspBleListenerId allocateListenerIdLocked();
  bool listenerIdInUseLocked(EspBleListenerId listenerId) const;

  std::shared_ptr<DiscoveryCallback> discoveryCallback_;
  std::shared_ptr<StateCallback> stateCallback_;
  std::shared_ptr<KeyboardCallback> keyboardCallback_;
  std::shared_ptr<MouseCallback> mouseCallback_;
  std::shared_ptr<ConsumerControlCallback> consumerControlCallback_;
  std::shared_ptr<SystemControlCallback> systemControlCallback_;
  std::shared_ptr<GamepadCallback> gamepadCallback_;
  std::shared_ptr<VendorInputCallback> vendorInputCallback_;
  ListenerSlot<DiscoveryCallback> discoveryListeners_[MaxListenersPerEvent];
  ListenerSlot<StateCallback> stateListeners_[MaxListenersPerEvent];
  ListenerSlot<KeyboardCallback> keyboardListeners_[MaxListenersPerEvent];
  ListenerSlot<MouseCallback> mouseListeners_[MaxListenersPerEvent];
  ListenerSlot<ConsumerControlCallback> consumerControlListeners_[MaxListenersPerEvent];
  ListenerSlot<SystemControlCallback> systemControlListeners_[MaxListenersPerEvent];
  ListenerSlot<GamepadCallback> gamepadListeners_[MaxListenersPerEvent];
  ListenerSlot<VendorInputCallback> vendorInputListeners_[MaxListenersPerEvent];
  EspBleListenerId nextListenerId_ = 1;
  mutable std::mutex listenerMutex_;
  EspBleKeyboardLayout keyboardLayout_ = EspBleKeyboardLayout::EnUs;
  // Auto-rediscover state. Touched only on the loop task (record at discovery
  // dispatch, read at security dispatch), so no lock is needed.
  bool autoRediscover_ = false;
  String rediscoverPeers_[MaxRediscoverPeers];
};

class EspBle
{
public:
  static constexpr size_t MaxDiscoveredGattServices = 16;
  static constexpr size_t MaxDiscoveredGattCharacteristics = 48;
  static constexpr size_t MaxDiscoveredGattDescriptors = 48;
  // Client Characteristic Configuration Descriptor, the descriptor a client
  // writes to turn Notification or Indication on. Useful when walking
  // discoveredDescriptors() to find what a characteristic can be subscribed to.
  static constexpr const char *ClientCharacteristicConfigurationUuid =
    "00002902-0000-1000-8000-00805f9b34fb";
  using ConnectionCallback = std::function<void(const EspBleConnection &connection)>;
  using ConnectionFailureCallback = std::function<void(const EspBleConnectionFailure &failure)>;
  using MtuChangedCallback = std::function<void(const EspBleMtuChanged &event)>;
  using SecurityChangedCallback = std::function<void(const EspBleSecurityChanged &event)>;
  using PasskeyDisplayedCallback = std::function<void(const EspBlePasskeyDisplayed &event)>;
  using GattResultCallback = std::function<void(const EspBleGattResult &result)>;
  using NotificationCallback = std::function<void(const EspBleGattNotification &notification)>;

  EspBle();
  ~EspBle();

  EspBle(const EspBle &) = delete;
  EspBle &operator=(const EspBle &) = delete;

  bool begin(const EspBleConfig &config = EspBleConfig());
  void end();
  void update();

  // The address this device currently presents to peers, as a string, or an
  // empty String before begin(). With ownAddressType = ResolvablePrivate this is
  // the RPA in use at the moment, so it changes when the controller rotates it.
  // Its type is the one requested through EspBleConfig::ownAddressType, reported
  // by localAddressType().
  String localAddress() const;
  EspBleAddressType localAddressType() const;

  // Transmit power in dBm. The radio supports discrete levels (-12..+9 dBm in
  // 3 dB steps on the bundled build); setTxPower() rounds to the nearest one and
  // txPower() reports what the radio actually applied. Raising it extends range
  // at the cost of current draw. Returns false before begin(). The level applies
  // to advertising, scanning and connections alike.
  bool setTxPower(int8_t dBm);
  // Current level in dBm, or INT8_MIN when it cannot be determined.
  int8_t txPower() const;

  bool connect(const EspBleScanResult &scanResult, uint32_t timeoutMilliseconds = 10000);
  bool connect(
    const char *address,
    EspBleAddressType addressType,
    uint32_t timeoutMilliseconds = 10000);
  // Close a connection. reason is the HCI reason code sent to the peer, which
  // surfaces there as EspBleConnection::disconnectReason; the default is the
  // "remote user terminated connection" code. Only a handful of codes are valid
  // for a local termination, so leave the default unless the peer expects a
  // specific one.
  static constexpr uint8_t DisconnectReasonRemoteUserTerminated = 0x13;
  bool disconnect(
    EspBleConnectionId connectionId,
    uint8_t reason = DisconnectReasonRemoteUserTerminated);
  // Automatic reconnection for Central connections (default off). When enabled,
  // every peer this central connects to is remembered, and if such a connection
  // drops unexpectedly the library reconnects to the same peer address on its
  // own (retried periodically until it succeeds). Combined with the default
  // persistent subscriptions, notifications resume without any application code.
  // A connection closed by disconnect() is intentional and is not reconnected.
  // Enabling it adopts the currently connected central peers; disabling it stops
  // and forgets all pending reconnects. Relies on a stable peer address.
  void setAutoReconnect(bool enabled);
  bool autoReconnect() const;
  // Request a connection parameter update on an active connection. Intervals are
  // in units of 1.25 ms, supervisionTimeout in units of 10 ms, and latency
  // counts skipped connection events. The negotiated result is delivered to
  // onConnectionParametersUpdated(). Works from either role.
  bool updateConnectionParameters(
    EspBleConnectionId connectionId,
    uint16_t minInterval,
    uint16_t maxInterval,
    uint16_t latency,
    uint16_t supervisionTimeout);
  // PHY preference masks for updatePhy(), matching the LE PHY bit masks.
  static constexpr uint8_t Phy1MMask = 0x01;
  static constexpr uint8_t Phy2MMask = 0x02;
  static constexpr uint8_t PhyCodedMask = 0x04;
  // Request a preferred LE PHY on an active connection. txPhyMask and rxPhyMask
  // are OR-ed combinations of Phy1MMask / Phy2MMask / PhyCodedMask. The
  // negotiated result is delivered to onPhyUpdated(). 2M and Coded depend on
  // radio support and the peer.
  bool updatePhy(EspBleConnectionId connectionId, uint8_t txPhyMask, uint8_t rxPhyMask);
  // Send a GATT Service Changed indication (Generic Attribute service 0x1801,
  // characteristic 0x2A05) covering the attribute-handle range [startHandle,
  // endHandle], telling subscribed clients to rediscover that range. The
  // Generic Attribute service is provided by the backend. Use 0x0001..0xFFFF to
  // indicate the whole database.
  bool notifyServicesChanged(uint16_t startHandle, uint16_t endHandle);
  size_t droppedEventCount() const;
  // Number of persistent subscriptions dropped because the registry was full;
  // non-zero means some subscriptions will not be restored on reconnect.
  size_t droppedPersistentSubscriptionCount() const;
  size_t connectionCount() const;
  bool connection(EspBleConnectionId connectionId, EspBleConnection &connection) const;
  bool requestSecurity(EspBleConnectionId connectionId);
  // Supply the passkey for an in-progress Passkey Entry when this device is the
  // input side (KeyboardOnly, MITM, no static passkey). Call it after initiating
  // the secured connection; the pairing blocks until it arrives (or times out).
  // Not needed when a static passkey is configured.
  bool providePasskey(uint32_t passkey);
  // Accept list (Filter Accept List, formerly "white list"). The controller
  // filters on it when advertising with a restrictive
  // EspBleAdvertisingFilterPolicy, so a peer that is not on the list never
  // reaches the application. Entries are matched by address, so a peer using a
  // rotating RPA cannot be listed usefully unless it is bonded and its identity
  // address is used. Changes take effect the next time advertising starts.
  static constexpr size_t MaxAcceptListEntries = 8;
  bool addToAcceptList(const char *address, EspBleAddressType addressType);
  bool removeFromAcceptList(const char *address, EspBleAddressType addressType);
  void clearAcceptList();
  size_t acceptListCount() const;
  bool acceptListEntry(size_t index, EspBleBond &entry) const;

  size_t bondCount() const;
  bool bond(size_t index, EspBleBond &bond) const;
  bool deleteBond(const EspBleBond &bond);
  bool deleteAllBonds();

  void onConnected(ConnectionCallback callback);
  void onDisconnected(ConnectionCallback callback);
  void onConnectionFailed(ConnectionFailureCallback callback);
  void onMtuChanged(MtuChangedCallback callback);
  void onConnectionParametersUpdated(ConnectionCallback callback);
  void onPhyUpdated(ConnectionCallback callback);
  void onSecurityChanged(SecurityChangedCallback callback);
  void onPasskeyDisplayed(PasskeyDisplayedCallback callback);
  // Numeric Comparison (LE Secure Connections, both sides DisplayYesNo + MITM):
  // the callback delivers the 6-digit value both devices display (in the event's
  // `passkey` field). Confirm the match with confirmNumericComparison(); the
  // pairing blocks until then (or a timeout rejects it).
  void onNumericComparison(PasskeyDisplayedCallback callback);
  bool confirmNumericComparison(bool accept);

  bool discoverCharacteristic(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    uint32_t timeoutMilliseconds = 10000);
  bool discoverServices(
    EspBleConnectionId connectionId,
    uint32_t timeoutMilliseconds = 10000);
  size_t discoveredServiceCount(EspBleConnectionId connectionId) const;
  bool discoveredService(
    EspBleConnectionId connectionId,
    size_t index,
    EspBleGattServiceInfo &service) const;
  size_t discoveredCharacteristicCount(
    EspBleConnectionId connectionId,
    const char *serviceUuid = nullptr) const;
  bool discoveredCharacteristic(
    EspBleConnectionId connectionId,
    size_t index,
    EspBleGattCharacteristicInfo &characteristic,
    const char *serviceUuid = nullptr) const;
  size_t discoveredDescriptorCount(
    EspBleConnectionId connectionId,
    const char *serviceUuid = nullptr,
    const char *characteristicUuid = nullptr) const;
  bool discoveredDescriptor(
    EspBleConnectionId connectionId,
    size_t index,
    EspBleGattDescriptorInfo &descriptor,
    const char *serviceUuid = nullptr,
    const char *characteristicUuid = nullptr) const;
  bool readCharacteristic(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    uint32_t timeoutMilliseconds = 10000);
  bool writeCharacteristic(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    const uint8_t *data,
    size_t length,
    bool response = true,
    uint32_t timeoutMilliseconds = 10000);
  bool writeCharacteristic(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    const String &value,
    bool response = true,
    uint32_t timeoutMilliseconds = 10000);
  bool readDescriptor(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    const char *descriptorUuid,
    uint32_t timeoutMilliseconds = 10000);
  bool writeDescriptor(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    const char *descriptorUuid,
    const uint8_t *data,
    size_t length,
    bool response = true,
    uint32_t timeoutMilliseconds = 10000);
  bool writeDescriptor(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    const char *descriptorUuid,
    const String &value,
    bool response = true,
    uint32_t timeoutMilliseconds = 10000);
  bool subscribe(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    bool notifications = true,
    uint32_t timeoutMilliseconds = 10000);
  bool unsubscribe(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    uint32_t timeoutMilliseconds = 10000);

  // Handle-based characteristic operations. Use these to target a specific
  // characteristic when several share a UUID (e.g. HID Report characteristics):
  // obtain the attribute handle from discoveredCharacteristic() after
  // discoverServices(), then read/write/subscribe by that handle. The
  // EspBleGattResult / EspBleGattNotification `handle` field echoes it back.
  bool readCharacteristic(
    EspBleConnectionId connectionId,
    uint16_t characteristicHandle,
    uint32_t timeoutMilliseconds = 10000);
  bool writeCharacteristic(
    EspBleConnectionId connectionId,
    uint16_t characteristicHandle,
    const uint8_t *data,
    size_t length,
    bool response = true,
    uint32_t timeoutMilliseconds = 10000);
  bool writeCharacteristic(
    EspBleConnectionId connectionId,
    uint16_t characteristicHandle,
    const String &value,
    bool response = true,
    uint32_t timeoutMilliseconds = 10000);
  bool subscribe(
    EspBleConnectionId connectionId,
    uint16_t characteristicHandle,
    bool notifications = true,
    uint32_t timeoutMilliseconds = 10000);
  bool unsubscribe(
    EspBleConnectionId connectionId,
    uint16_t characteristicHandle,
    uint32_t timeoutMilliseconds = 10000);

  // Primary observer (one per event; a second call replaces it).
  void onCharacteristicDiscovered(GattResultCallback callback);
  void onCharacteristicRead(GattResultCallback callback);
  void onCharacteristicWritten(GattResultCallback callback);
  void onServicesDiscovered(GattResultCallback callback);
  void onDescriptorRead(GattResultCallback callback);
  void onDescriptorWritten(GattResultCallback callback);
  void onSubscribed(GattResultCallback callback);
  void onUnsubscribed(GattResultCallback callback);
  void onNotification(NotificationCallback callback);
  // Additional GATT-client observers that coexist with the primary and each
  // other, so a profile helper (e.g. the MIDI host) and application code can
  // both watch the same event. Returns a listener id (EspBleInvalidListenerId
  // if the list is full or callback is empty); removeGattListener() drops one.
  EspBleListenerId addCharacteristicDiscoveredListener(GattResultCallback callback);
  EspBleListenerId addCharacteristicReadListener(GattResultCallback callback);
  EspBleListenerId addCharacteristicWrittenListener(GattResultCallback callback);
  EspBleListenerId addServicesDiscoveredListener(GattResultCallback callback);
  EspBleListenerId addDescriptorReadListener(GattResultCallback callback);
  EspBleListenerId addDescriptorWrittenListener(GattResultCallback callback);
  EspBleListenerId addSubscribedListener(GattResultCallback callback);
  EspBleListenerId addUnsubscribedListener(GattResultCallback callback);
  EspBleListenerId addNotificationListener(NotificationCallback callback);
  bool removeGattListener(EspBleListenerId listenerId);

  bool initialized() const;
  EspBleAdvertising &advertising();
  EspBleScanner &scanner();
  EspBleGattServer &gattServer();
  EspBleHidKeyboard &hidKeyboard();
  EspBleHidMouse &hidMouse();
  EspBleHidConsumerControl &hidConsumerControl();
  EspBleHidSystemControl &hidSystemControl();
  EspBleHidGamepad &hidGamepad();
  EspBleHidVendor &hidVendor();
  EspBleHidCustom &hidCustom();
  EspBleHidHost &hidHost();

  EspBleError lastError() const;
  const char *lastErrorName() const;
  const String &lastErrorDetail() const;
  void clearError();

private:
  friend class EspBleAdvertising;
  friend class EspBleScanner;
  friend class EspBleGattServer;
  friend class EspBleHidKeyboard;
  friend class EspBleHidHost;
  friend class EspBleHidMouse;
  friend class EspBleHidConsumerControl;
  friend class EspBleHidSystemControl;
  friend class EspBleHidGamepad;
  friend class EspBleHidVendor;
  friend class EspBleHidCustom;
  friend struct EspBleScannerImpl;
  friend struct EspBleImpl;
  friend struct EspBleGattServerImpl;
  friend struct EspBleHidDeviceManagerImpl;
  friend struct EspBleHidKeyboardHostImpl;

  void setError(EspBleError error, const char *detail = nullptr);
  // Overwrite the controller's accept list with acceptList_.
  bool syncAcceptList();
  bool preparePeripheral();
  void dispatchConnectionEvents();
  void reapRetiredClients();
  void cancelExpiredConnectAttempt();
  void driveAutoReconnect();
  void expireGattOperation();
  bool startGattOperation(
    EspBleGattOperation operation,
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    const uint8_t *data = nullptr,
    size_t length = 0,
    bool response = true,
    const char *descriptorUuid = nullptr,
    uint32_t timeoutMilliseconds = 10000,
    uint16_t characteristicHandle = 0);
  // Start the next queued GATT operation if the ATT channel is free. Pumped from
  // update() so operations serialize behind whatever is currently running.
  void pumpGattQueue();
  void pumpSendQueue();
  void releaseDeferredNotifications();
  void drainPendingDisconnects();
  // True when a HID discovery for connectionId is already queued or in flight.
  // Lets HID auto-rediscover avoid a second discovery when the app also asked.
  bool hasPendingHidDiscover(EspBleConnectionId connectionId) const;
  EspBleListenerId allocateGattListenerIdLocked();

  bool initialized_ = false;
  bool autoReconnect_ = false;
  EspBleOwnAddressType activeOwnAddressType_ = EspBleOwnAddressType::Public;
  // Mirror of the backend accept list: the wrapper offers add/remove but no way
  // to enumerate or clear, so the entries are tracked here as well.
  EspBleBond acceptList_[MaxAcceptListEntries];
  size_t acceptListCount_ = 0;
  String activeDeviceName_;
  uint16_t activePreferredMtu_ = 0;
  EspBleSecurityConfig activeSecurity_;
  EspBleError lastError_ = EspBleError::None;
  String lastErrorDetail_;
  EspBleAdvertising advertising_;
  EspBleScanner scanner_;
  EspBleGattServer gattServer_;
  EspBleHidKeyboard hidKeyboardDevice_;
  EspBleHidMouse hidMouse_;
  EspBleHidConsumerControl hidConsumerControl_;
  EspBleHidSystemControl hidSystemControl_;
  EspBleHidGamepad hidGamepad_;
  EspBleHidVendor hidVendor_;
  EspBleHidCustom hidCustom_;
  EspBleHidHost hidKeyboardHost_;
  EspBleImpl *impl_ = nullptr;
  ConnectionCallback connectedCallback_;
  ConnectionCallback disconnectedCallback_;
  ConnectionFailureCallback connectionFailedCallback_;
  MtuChangedCallback mtuChangedCallback_;
  ConnectionCallback connectionParametersUpdatedCallback_;
  ConnectionCallback phyUpdatedCallback_;
  SecurityChangedCallback securityChangedCallback_;
  PasskeyDisplayedCallback passkeyDisplayedCallback_;
  PasskeyDisplayedCallback numericComparisonCallback_;
  EspBleCallbackList<GattResultCallback> characteristicDiscoveredListeners_;
  EspBleCallbackList<GattResultCallback> characteristicReadListeners_;
  EspBleCallbackList<GattResultCallback> characteristicWrittenListeners_;
  EspBleCallbackList<GattResultCallback> servicesDiscoveredListeners_;
  EspBleCallbackList<GattResultCallback> descriptorReadListeners_;
  EspBleCallbackList<GattResultCallback> descriptorWrittenListeners_;
  EspBleCallbackList<GattResultCallback> subscribedListeners_;
  EspBleCallbackList<GattResultCallback> unsubscribedListeners_;
  EspBleCallbackList<NotificationCallback> notificationListeners_;
  mutable std::mutex gattListenerMutex_;
  EspBleListenerId nextGattListenerId_ = 1;
};

#endif // ESP_BLE_H
