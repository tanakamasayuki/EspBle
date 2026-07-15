#ifndef ESP_BLE_H
#define ESP_BLE_H

#include <Arduino.h>
#include <functional>
#include <sdkconfig.h>

#if !defined(CONFIG_NIMBLE_ENABLED) && !defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"
#endif

#include "espble_version.h"

enum class EspBleError : uint8_t
{
  None = 0,
  InvalidState,
  InvalidArgument,
  BackendFailure,
  ResourceExhausted,
};

struct EspBleConfig
{
  const char *deviceName = "EspBle";
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

class EspBle;
class EspBleAdvertising;
class EspBleScanner;
struct EspBleScannerImpl;

class EspBleAdvertising
{
public:
  static constexpr size_t MaxServiceUuids = 4;

  void clear();
  void setName(const char *name);
  bool addServiceUuid(const char *uuid);
  void setManufacturerData(const uint8_t *data, size_t length);
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

  EspBle *owner_;
  ResultCallback resultCallback_;
  EspBleScannerImpl *impl_ = nullptr;
};

class EspBle
{
public:
  EspBle();
  ~EspBle();

  EspBle(const EspBle &) = delete;
  EspBle &operator=(const EspBle &) = delete;

  bool begin(const EspBleConfig &config = EspBleConfig());
  void end();
  void update();

  bool initialized() const;
  EspBleAdvertising &advertising();
  EspBleScanner &scanner();

  EspBleError lastError() const;
  const char *lastErrorName() const;
  const String &lastErrorDetail() const;
  void clearError();

private:
  friend class EspBleAdvertising;
  friend class EspBleScanner;
  friend struct EspBleScannerImpl;

  void setError(EspBleError error, const char *detail = nullptr);

  bool initialized_ = false;
  EspBleError lastError_ = EspBleError::None;
  String lastErrorDetail_;
  EspBleAdvertising advertising_;
  EspBleScanner scanner_;
};

#endif // ESP_BLE_H
