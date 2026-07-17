#ifndef ESP_BLE_H
#define ESP_BLE_H

#include <Arduino.h>
#include <functional>
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
};

enum class EspBleSecurityIoCapability : uint8_t
{
  None = 0,
  DisplayOnly,
  KeyboardOnly,
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
};

struct EspBleScanConfig
{
  bool active = true;
  bool wantDuplicates = false;
  uint16_t intervalMilliseconds = 100;
  uint16_t windowMilliseconds = 50;
  uint32_t durationSeconds = 0;
};

struct EspBleScanResult
{
  static constexpr size_t MaxServiceUuids = 8;

  String address;
  uint8_t addressType = 0;
  String name;
  int rssi = 0;
  bool connectable = false;
  bool scannable = false;
  String manufacturerData;
  String serviceUuids[MaxServiceUuids];
  size_t serviceUuidCount = 0;

  bool hasName() const;
  bool hasManufacturerData() const;
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

struct EspBleConnection
{
  EspBleConnectionId id = 0;
  uint16_t handle = 0xffff;
  String peerAddress;
  uint8_t peerAddressType = 0;
  EspBleRole localRole = EspBleRole::Central;
  uint16_t mtu = 23;
  bool encrypted = false;
  bool authenticated = false;
  bool bonded = false;
  uint8_t encryptionKeySize = 0;

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
  uint8_t peerAddressType = 0;
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

enum class EspBleGattOperation : uint8_t
{
  Discover = 0,
  Read,
  Write,
  Subscribe,
  Unsubscribe,
};

struct EspBleGattResult
{
  EspBleGattOperation operation = EspBleGattOperation::Discover;
  EspBleConnectionId connectionId = 0;
  String serviceUuid;
  String characteristicUuid;
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
};

struct EspBleGattWrite
{
  EspBleConnectionId connectionId = 0;
  String serviceUuid;
  String characteristicUuid;
  String value;
};

struct EspBleGattNotification
{
  EspBleConnectionId connectionId = 0;
  String serviceUuid;
  String characteristicUuid;
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

struct EspBleHidKeyboardDeviceConfig
{
  const char *manufacturer = "EspBle";
  uint16_t vendorId = 0xffff;
  uint16_t productId = 0x0001;
  uint16_t productVersion = 0x0001;
  uint8_t countryCode = 0;
  uint8_t reportId = 1;
  uint8_t initialBatteryLevel = 100;
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

struct EspBleHidKeyboardEvent
{
  EspBleConnectionId connectionId = 0;
  uint8_t reportId = 0;
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

class EspBle;
class EspBleAdvertising;
class EspBleScanner;
class EspBleGattServer;
class EspBleHidKeyboardDevice;
class EspBleHidKeyboardHost;
struct EspBleScannerImpl;
struct EspBleImpl;
struct EspBleGattServerImpl;
struct EspBleHidKeyboardDeviceImpl;
struct EspBleHidKeyboardHostImpl;

class EspBleAdvertising
{
public:
  static constexpr size_t MaxServiceUuids = 4;

  void clear();
  void setName(const char *name);
  bool addServiceUuid(const char *uuid);
  void setManufacturerData(const uint8_t *data, size_t length);
  void setAppearance(uint16_t appearance);
  void setScanResponseEnabled(bool enabled);
  bool start(uint32_t durationSeconds = 0);
  bool stop();
  bool isAdvertising() const;

private:
  friend class EspBle;

  explicit EspBleAdvertising(EspBle *owner);

  EspBle *owner_;
  String name_;
  String manufacturerData_;
  String serviceUuids_[MaxServiceUuids];
  size_t serviceUuidCount_ = 0;
  uint16_t appearance_ = 0;
  bool scanResponseEnabled_ = true;
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
  using WriteCallback = std::function<void(const EspBleGattWrite &write)>;
  using SubscriptionCallback = std::function<void(const EspBleGattSubscription &subscription)>;
  using SendCallback = std::function<void(const EspBleGattSendResult &result)>;

  bool addService(const char *serviceUuid);
  bool addCharacteristic(
    const char *serviceUuid,
    const char *characteristicUuid,
    const EspBleGattCharacteristicConfig &config);
  bool setValue(
    const char *serviceUuid,
    const char *characteristicUuid,
    const uint8_t *data,
    size_t length);
  bool setValue(const char *serviceUuid, const char *characteristicUuid, const String &value);
  bool value(const char *serviceUuid, const char *characteristicUuid, String &value) const;
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
  SubscriptionCallback subscriptionCallback_;
  SendCallback sendCallback_;
};

class EspBleHidKeyboardDevice
{
public:
  using OutputReportCallback =
    std::function<void(const EspBleHidKeyboardOutputReport &report)>;

  bool configure(
    const EspBleHidKeyboardDeviceConfig &config = EspBleHidKeyboardDeviceConfig());
  bool sendInputReport(const EspBleHidKeyboardInputReport &report);
  bool releaseAll();
  bool setBatteryLevel(uint8_t level);
  void onOutputReport(OutputReportCallback callback);
  bool configured() const;

private:
  friend class EspBle;
  friend struct EspBleHidKeyboardDeviceImpl;

  explicit EspBleHidKeyboardDevice(EspBle *owner);
  ~EspBleHidKeyboardDevice();
  bool realize();
  void resetBackend();
  void dispatchPendingOutputReports();

  EspBle *owner_;
  EspBleHidKeyboardDeviceImpl *impl_ = nullptr;
  OutputReportCallback outputReportCallback_;
};

class EspBleHidKeyboardHost
{
public:
  static constexpr size_t MaxListenersPerEvent = 4;
  using DiscoveryCallback =
    std::function<void(const EspBleHidKeyboardHostDiscovery &result)>;
  using StateCallback = std::function<void(const EspBleHidKeyboardState &state)>;
  using KeyboardCallback = std::function<void(const EspBleHidKeyboardEvent &event)>;

  bool discover(EspBleConnectionId connectionId);
  bool setKeyboardLeds(
    EspBleConnectionId connectionId,
    bool numLock,
    bool capsLock,
    bool scrollLock,
    bool compose = false,
    bool kana = false);
  void onDiscovered(DiscoveryCallback callback);
  void onKeyboardState(StateCallback callback);
  void onKeyboard(KeyboardCallback callback);
  EspBleListenerId addDiscoveredListener(DiscoveryCallback callback);
  EspBleListenerId addKeyboardStateListener(StateCallback callback);
  EspBleListenerId addKeyboardListener(KeyboardCallback callback);
  bool removeListener(EspBleListenerId listenerId);
  void setKeyboardLayout(EspBleKeyboardLayout layout);
  EspBleKeyboardLayout keyboardLayout() const;
  bool ready(EspBleConnectionId connectionId) const;
  size_t droppedEventCount() const;
  size_t invalidInputReportCount() const;

private:
  friend class EspBle;
  friend struct EspBleHidKeyboardHostImpl;

  explicit EspBleHidKeyboardHost(EspBle *owner);
  ~EspBleHidKeyboardHost();
  void resetBackend();
  void handleDisconnected(EspBleConnectionId connectionId);
  void dispatchPendingEvents();

  EspBle *owner_;
  EspBleHidKeyboardHostImpl *impl_ = nullptr;

  template <typename Callback>
  struct ListenerSlot
  {
    EspBleListenerId id = EspBleInvalidListenerId;
    Callback callback;
  };

  template <typename Callback>
  EspBleListenerId addListener(ListenerSlot<Callback> *slots, Callback callback);

  template <typename Callback>
  static bool removeListenerFrom(
    ListenerSlot<Callback> *slots,
    EspBleListenerId listenerId);

  DiscoveryCallback discoveryCallback_;
  StateCallback stateCallback_;
  KeyboardCallback keyboardCallback_;
  ListenerSlot<DiscoveryCallback> discoveryListeners_[MaxListenersPerEvent];
  ListenerSlot<StateCallback> stateListeners_[MaxListenersPerEvent];
  ListenerSlot<KeyboardCallback> keyboardListeners_[MaxListenersPerEvent];
  EspBleListenerId nextListenerId_ = 1;
  EspBleKeyboardLayout keyboardLayout_ = EspBleKeyboardLayout::EnUs;
};

class EspBle
{
public:
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
  bool disconnect(EspBleConnectionId connectionId);
  size_t droppedEventCount() const;
  size_t connectionCount() const;
  bool connection(EspBleConnectionId connectionId, EspBleConnection &connection) const;
  bool requestSecurity(EspBleConnectionId connectionId);
  size_t bondCount() const;
  bool bond(size_t index, EspBleBond &bond) const;
  bool deleteBond(const EspBleBond &bond);
  bool deleteAllBonds();

  void onConnected(ConnectionCallback callback);
  void onDisconnected(ConnectionCallback callback);
  void onConnectionFailed(ConnectionFailureCallback callback);
  void onMtuChanged(MtuChangedCallback callback);
  void onSecurityChanged(SecurityChangedCallback callback);
  void onPasskeyDisplayed(PasskeyDisplayedCallback callback);

  bool discoverCharacteristic(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid);
  bool readCharacteristic(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid);
  bool writeCharacteristic(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    const uint8_t *data,
    size_t length,
    bool response = true);
  bool writeCharacteristic(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    const String &value,
    bool response = true);
  bool subscribe(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    bool notifications = true);
  bool unsubscribe(
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid);
  void onCharacteristicDiscovered(GattResultCallback callback);
  void onCharacteristicRead(GattResultCallback callback);
  void onCharacteristicWritten(GattResultCallback callback);
  void onSubscribed(GattResultCallback callback);
  void onUnsubscribed(GattResultCallback callback);
  void onNotification(std::function<void(const EspBleGattNotification &notification)> callback);

  bool initialized() const;
  EspBleAdvertising &advertising();
  EspBleScanner &scanner();
  EspBleGattServer &gattServer();
  EspBleHidKeyboardDevice &hidKeyboardDevice();
  EspBleHidKeyboardHost &hidKeyboardHost();

  EspBleError lastError() const;
  const char *lastErrorName() const;
  const String &lastErrorDetail() const;
  void clearError();

private:
  friend class EspBleAdvertising;
  friend class EspBleScanner;
  friend class EspBleGattServer;
  friend class EspBleHidKeyboardDevice;
  friend class EspBleHidKeyboardHost;
  friend struct EspBleScannerImpl;
  friend struct EspBleImpl;
  friend struct EspBleGattServerImpl;
  friend struct EspBleHidKeyboardDeviceImpl;
  friend struct EspBleHidKeyboardHostImpl;

  void setError(EspBleError error, const char *detail = nullptr);
  bool preparePeripheral();
  void dispatchConnectionEvents();
  void reapRetiredClients();
  bool startGattOperation(
    EspBleGattOperation operation,
    EspBleConnectionId connectionId,
    const char *serviceUuid,
    const char *characteristicUuid,
    const uint8_t *data = nullptr,
    size_t length = 0,
    bool response = true);

  bool initialized_ = false;
  String activeDeviceName_;
  uint16_t activePreferredMtu_ = 0;
  EspBleSecurityConfig activeSecurity_;
  EspBleError lastError_ = EspBleError::None;
  String lastErrorDetail_;
  EspBleAdvertising advertising_;
  EspBleScanner scanner_;
  EspBleGattServer gattServer_;
  EspBleHidKeyboardDevice hidKeyboardDevice_;
  EspBleHidKeyboardHost hidKeyboardHost_;
  EspBleImpl *impl_ = nullptr;
  ConnectionCallback connectedCallback_;
  ConnectionCallback disconnectedCallback_;
  ConnectionFailureCallback connectionFailedCallback_;
  MtuChangedCallback mtuChangedCallback_;
  SecurityChangedCallback securityChangedCallback_;
  PasskeyDisplayedCallback passkeyDisplayedCallback_;
  GattResultCallback characteristicDiscoveredCallback_;
  GattResultCallback characteristicReadCallback_;
  GattResultCallback characteristicWrittenCallback_;
  GattResultCallback subscribedCallback_;
  GattResultCallback unsubscribedCallback_;
  std::function<void(const EspBleGattNotification &notification)> notificationCallback_;
};

#endif // ESP_BLE_H
