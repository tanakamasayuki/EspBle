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
  uint16_t preferredMtu = 23;
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
  bool active = true;
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

struct EspBleScanResult
{
  static constexpr size_t MaxServiceUuids = 8;

  String address;
  EspBleAddressType addressType = EspBleAddressType::Public;
  String name;
  int rssi = 0;
  bool connectable = false;
  bool scannable = false;
  String manufacturerData;
  // First Service Data block, if any (AD type 0x16/0x20/0x21). Eddystone and
  // most beacon formats carry their payload here; serviceDataUuid is the
  // associated service UUID (e.g. "0xfeaa" for Eddystone).
  String serviceData;
  String serviceDataUuid;
  String serviceUuids[MaxServiceUuids];
  size_t serviceUuidCount = 0;

  bool hasName() const;
  bool hasManufacturerData() const;
  bool hasServiceData() const;
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
  // Only meaningful in the onDisconnected() callback: the backend/HCI reason
  // code for the disconnection, or 0 when the reason is unknown. A locally
  // requested disconnect, a remote termination, and a supervision timeout
  // report distinct codes. 0 in the onConnected()/onMtuChanged() events.
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
};

struct EspBleGattWrite
{
  EspBleConnectionId connectionId = 0;
  String serviceUuid;
  String characteristicUuid;
  String value;
};

struct EspBleGattDescriptorWrite
{
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
  String serviceUuid;
  String characteristicUuid;
  bool notifications = false;
  bool indications = false;
};

struct EspBleGattSendResult
{
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

class EspBleAdvertising
{
public:
  static constexpr size_t MaxServiceUuids = 4;

  void clear();
  void setName(const char *name);
  bool addServiceUuid(const char *uuid);
  void setManufacturerData(const uint8_t *data, size_t length);
  // Set a Service Data block (AD type 0x16 for a 16-bit UUID). Used by Eddystone
  // and other service-data beacons. Add the same UUID with addServiceUuid() too
  // if scanners should also discover it via the service-UUID list.
  bool setServiceData(const char *uuid, const uint8_t *data, size_t length);
  void setAppearance(uint16_t appearance);
  void setScanResponseEnabled(bool enabled);
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

  EspBle *owner_;
  String name_;
  String manufacturerData_;
  String serviceData_;
  String serviceDataUuid_;
  String serviceUuids_[MaxServiceUuids];
  size_t serviceUuidCount_ = 0;
  uint16_t appearance_ = 0;
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
  static constexpr size_t MaxServices = 4;
  static constexpr size_t MaxCharacteristics = 16;
  static constexpr size_t MaxDescriptors = 16;
  using WriteCallback = std::function<void(const EspBleGattWrite &write)>;
  using DescriptorWriteCallback =
    std::function<void(const EspBleGattDescriptorWrite &write)>;
  using SubscriptionCallback = std::function<void(const EspBleGattSubscription &subscription)>;
  using SendCallback = std::function<void(const EspBleGattSendResult &result)>;

  bool addService(const char *serviceUuid);
  bool addCharacteristic(
    const char *serviceUuid,
    const char *characteristicUuid,
    const EspBleGattCharacteristicConfig &config);
  bool addDescriptor(
    const char *serviceUuid,
    const char *characteristicUuid,
    const char *descriptorUuid,
    const EspBleGattDescriptorConfig &config = EspBleGattDescriptorConfig());
  bool setValue(
    const char *serviceUuid,
    const char *characteristicUuid,
    const uint8_t *data,
    size_t length);
  bool setValue(const char *serviceUuid, const char *characteristicUuid, const String &value);
  bool value(const char *serviceUuid, const char *characteristicUuid, String &value) const;
  bool setDescriptorValue(
    const char *serviceUuid,
    const char *characteristicUuid,
    const char *descriptorUuid,
    const uint8_t *data,
    size_t length);
  bool setDescriptorValue(
    const char *serviceUuid,
    const char *characteristicUuid,
    const char *descriptorUuid,
    const String &value);
  bool descriptorValue(
    const char *serviceUuid,
    const char *characteristicUuid,
    const char *descriptorUuid,
    String &value) const;
  bool notify(
    const char *serviceUuid,
    const char *characteristicUuid,
    const uint8_t *data,
    size_t length);
  bool notify(const char *serviceUuid, const char *characteristicUuid, const String &value);
  bool indicate(
    const char *serviceUuid,
    const char *characteristicUuid,
    const uint8_t *data,
    size_t length);
  bool indicate(const char *serviceUuid, const char *characteristicUuid, const String &value);
  void onWritten(WriteCallback callback);
  void onDescriptorWritten(DescriptorWriteCallback callback);
  void onSubscriptionChanged(SubscriptionCallback callback);
  void onSent(SendCallback callback);

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
    const char *serviceUuid,
    const char *characteristicUuid,
    const uint8_t *data,
    size_t length,
    bool indication);

  EspBle *owner_;
  EspBleGattServerImpl *impl_ = nullptr;
  WriteCallback writeCallback_;
  DescriptorWriteCallback descriptorWriteCallback_;
  SubscriptionCallback subscriptionCallback_;
  SendCallback sendCallback_;
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

private:
  friend class EspBle;
  friend struct EspBleHidKeyboardHostImpl;

  explicit EspBleHidHost(EspBle *owner);
  ~EspBleHidHost();
  void resetBackend();
  void handleDisconnected(EspBleConnectionId connectionId);
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
};

class EspBle
{
public:
  static constexpr size_t MaxDiscoveredGattServices = 16;
  static constexpr size_t MaxDiscoveredGattCharacteristics = 48;
  static constexpr size_t MaxDiscoveredGattDescriptors = 48;
  using ConnectionCallback = std::function<void(const EspBleConnection &connection)>;
  using ConnectionFailureCallback = std::function<void(const EspBleConnectionFailure &failure)>;
  using MtuChangedCallback = std::function<void(const EspBleMtuChanged &event)>;
  using SecurityChangedCallback = std::function<void(const EspBleSecurityChanged &event)>;
  using PasskeyDisplayedCallback = std::function<void(const EspBlePasskeyDisplayed &event)>;
  using GattResultCallback = std::function<void(const EspBleGattResult &result)>;

  EspBle();
  ~EspBle();

  EspBle(const EspBle &) = delete;
  EspBle &operator=(const EspBle &) = delete;

  bool begin(const EspBleConfig &config = EspBleConfig());
  void end();
  void update();

  bool connect(const EspBleScanResult &scanResult, uint32_t timeoutMilliseconds = 10000);
  bool connect(
    const char *address,
    EspBleAddressType addressType,
    uint32_t timeoutMilliseconds = 10000);
  bool disconnect(EspBleConnectionId connectionId);
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
  size_t connectionCount() const;
  bool connection(EspBleConnectionId connectionId, EspBleConnection &connection) const;
  bool requestSecurity(EspBleConnectionId connectionId);
  // Supply the passkey for an in-progress Passkey Entry when this device is the
  // input side (KeyboardOnly, MITM, no static passkey). Call it after initiating
  // the secured connection; the pairing blocks until it arrives (or times out).
  // Not needed when a static passkey is configured.
  bool providePasskey(uint32_t passkey);
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

  void onCharacteristicDiscovered(GattResultCallback callback);
  void onCharacteristicRead(GattResultCallback callback);
  void onCharacteristicWritten(GattResultCallback callback);
  void onServicesDiscovered(GattResultCallback callback);
  void onDescriptorRead(GattResultCallback callback);
  void onDescriptorWritten(GattResultCallback callback);
  void onSubscribed(GattResultCallback callback);
  void onUnsubscribed(GattResultCallback callback);
  void onNotification(std::function<void(const EspBleGattNotification &notification)> callback);

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

  bool initialized_ = false;
  bool autoReconnect_ = false;
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
  GattResultCallback characteristicDiscoveredCallback_;
  GattResultCallback characteristicReadCallback_;
  GattResultCallback characteristicWrittenCallback_;
  GattResultCallback servicesDiscoveredCallback_;
  GattResultCallback descriptorReadCallback_;
  GattResultCallback descriptorWrittenCallback_;
  GattResultCallback subscribedCallback_;
  GattResultCallback unsubscribedCallback_;
  std::function<void(const EspBleGattNotification &notification)> notificationCallback_;
};

#endif // ESP_BLE_H
