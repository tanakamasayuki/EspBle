#include "EspBle.h"

#include <BLEAdvertising.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>
#include <mutex>
#include <utility>

namespace
{
constexpr size_t ScanQueueCapacity = 16;

bool uuidEquals(const String &left, const char *right)
{
  if (right == nullptr)
  {
    return false;
  }
  return left.equalsIgnoreCase(right);
}
} // namespace

struct EspBleScannerImpl
{
  class BackendCallbacks : public BLEAdvertisedDeviceCallbacks
  {
  public:
    explicit BackendCallbacks(EspBleScannerImpl *owner) : owner_(owner) {}

    void onResult(BLEAdvertisedDevice device) override
    {
      EspBleScanResult result;
      result.address = device.getAddress().toString();
      result.addressType = device.getAddressType();
      result.rssi = device.getRSSI();
      result.connectable = device.isConnectable();
      result.scannable = device.isScannable();

      if (device.haveName())
      {
        result.name = device.getName();
      }
      if (device.haveManufacturerData())
      {
        result.manufacturerData = device.getManufacturerData();
      }

      const int serviceCount = device.getServiceUUIDCount();
      for (int index = 0;
           index < serviceCount && result.serviceUuidCount < EspBleScanResult::MaxServiceUuids;
           ++index)
      {
        result.serviceUuids[result.serviceUuidCount++] = device.getServiceUUID(index).toString();
      }

      std::lock_guard<std::mutex> lock(owner_->mutex);
      if (owner_->count == ScanQueueCapacity)
      {
        ++owner_->dropped;
        return;
      }

      const size_t tail = (owner_->head + owner_->count) % ScanQueueCapacity;
      owner_->queue[tail] = std::move(result);
      ++owner_->count;
    }

  private:
    EspBleScannerImpl *owner_;
  };

  explicit EspBleScannerImpl(EspBleScanner *scanner)
      : callbacks(this), scanner(scanner)
  {
  }

  mutable std::mutex mutex;
  EspBleScanResult queue[ScanQueueCapacity];
  size_t head = 0;
  size_t count = 0;
  size_t dropped = 0;
  BackendCallbacks callbacks;
  EspBleScanner *scanner;
};

bool EspBleScanResult::hasName() const
{
  return !name.isEmpty();
}

bool EspBleScanResult::hasManufacturerData() const
{
  return !manufacturerData.isEmpty();
}

bool EspBleScanResult::advertisesService(const char *uuid) const
{
  for (size_t index = 0; index < serviceUuidCount; ++index)
  {
    if (uuidEquals(serviceUuids[index], uuid))
    {
      return true;
    }
  }
  return false;
}

EspBleAdvertising::EspBleAdvertising(EspBle *owner) : owner_(owner) {}

void EspBleAdvertising::clear()
{
  name_ = "";
  manufacturerData_ = "";
  serviceUuidCount_ = 0;
  scanResponseEnabled_ = true;
}

void EspBleAdvertising::setName(const char *name)
{
  name_ = name == nullptr ? "" : name;
}

bool EspBleAdvertising::addServiceUuid(const char *uuid)
{
  if (uuid == nullptr || uuid[0] == '\0')
  {
    owner_->setError(EspBleError::InvalidArgument, "service UUID is empty");
    return false;
  }
  if (serviceUuidCount_ == MaxServiceUuids)
  {
    owner_->setError(EspBleError::ResourceExhausted, "too many advertising service UUIDs");
    return false;
  }

  serviceUuids_[serviceUuidCount_++] = uuid;
  owner_->clearError();
  return true;
}

void EspBleAdvertising::setManufacturerData(const uint8_t *data, size_t length)
{
  if (data == nullptr || length == 0)
  {
    manufacturerData_ = "";
    return;
  }
  manufacturerData_ = String(reinterpret_cast<const char *>(data), length);
}

void EspBleAdvertising::setScanResponseEnabled(bool enabled)
{
  scanResponseEnabled_ = enabled;
}

bool EspBleAdvertising::start(uint32_t durationSeconds)
{
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }

  BLEAdvertising *backend = BLEDevice::getAdvertising();
  backend->stop();
  backend->reset();

  BLEAdvertisementData advertisingData;
  size_t previousLength = advertisingData.getPayload().length();
  advertisingData.setFlags(0x06); // General Discoverable, BR/EDR not supported.
  if (advertisingData.getPayload().length() == previousLength)
  {
    owner_->setError(EspBleError::InvalidArgument, "advertising flags do not fit in legacy payload");
    return false;
  }
  for (size_t index = 0; index < serviceUuidCount_; ++index)
  {
    previousLength = advertisingData.getPayload().length();
    advertisingData.setCompleteServices(BLEUUID(serviceUuids_[index].c_str()));
    if (advertisingData.getPayload().length() == previousLength)
    {
      owner_->setError(EspBleError::InvalidArgument, "service UUIDs do not fit in legacy advertising payload");
      return false;
    }
  }
  if (!manufacturerData_.isEmpty())
  {
    previousLength = advertisingData.getPayload().length();
    advertisingData.setManufacturerData(manufacturerData_);
    if (advertisingData.getPayload().length() == previousLength)
    {
      owner_->setError(EspBleError::InvalidArgument, "manufacturer data does not fit in legacy advertising payload");
      return false;
    }
  }
  if (!scanResponseEnabled_ && !name_.isEmpty())
  {
    previousLength = advertisingData.getPayload().length();
    advertisingData.setName(name_);
    if (advertisingData.getPayload().length() == previousLength)
    {
      owner_->setError(EspBleError::InvalidArgument, "name does not fit in legacy advertising payload");
      return false;
    }
  }

  if (!backend->setAdvertisementData(advertisingData))
  {
    owner_->setError(EspBleError::BackendFailure, "failed to set advertising data");
    return false;
  }

  backend->setScanResponse(scanResponseEnabled_);
  if (scanResponseEnabled_ && !name_.isEmpty())
  {
    BLEAdvertisementData scanResponseData;
    scanResponseData.setName(name_);
    if (scanResponseData.getPayload().isEmpty())
    {
      owner_->setError(EspBleError::InvalidArgument, "name does not fit in legacy scan response payload");
      return false;
    }
    if (!backend->setScanResponseData(scanResponseData))
    {
      owner_->setError(EspBleError::BackendFailure, "failed to set scan response data");
      return false;
    }
  }

  if (!backend->start(durationSeconds))
  {
    owner_->setError(EspBleError::BackendFailure, "failed to start advertising");
    return false;
  }

  owner_->clearError();
  return true;
}

bool EspBleAdvertising::stop()
{
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (!BLEDevice::getAdvertising()->stop())
  {
    owner_->setError(EspBleError::BackendFailure, "failed to stop advertising");
    return false;
  }
  owner_->clearError();
  return true;
}

bool EspBleAdvertising::isAdvertising() const
{
  return owner_->initialized() && BLEDevice::getAdvertising()->isAdvertising();
}

EspBleScanner::EspBleScanner(EspBle *owner) : owner_(owner) {}

EspBleScanner::~EspBleScanner()
{
  delete impl_;
}

void EspBleScanner::onResult(ResultCallback callback)
{
  resultCallback_ = std::move(callback);
}

bool EspBleScanner::start(const EspBleScanConfig &config)
{
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (config.windowMilliseconds == 0 || config.intervalMilliseconds == 0 ||
      config.windowMilliseconds > config.intervalMilliseconds)
  {
    owner_->setError(EspBleError::InvalidArgument, "scan window must be nonzero and no greater than interval");
    return false;
  }

  if (impl_ == nullptr)
  {
    impl_ = new EspBleScannerImpl(this);
    if (impl_ == nullptr)
    {
      owner_->setError(EspBleError::ResourceExhausted, "failed to allocate scanner state");
      return false;
    }
  }

  BLEScan *backend = BLEDevice::getScan();
  backend->setAdvertisedDeviceCallbacks(&impl_->callbacks, config.wantDuplicates, true);
  backend->setActiveScan(config.active);
  backend->setInterval(config.intervalMilliseconds);
  backend->setWindow(config.windowMilliseconds);
  backend->setDuplicateFilter(!config.wantDuplicates);

  if (!backend->start(config.durationSeconds, nullptr, false))
  {
    owner_->setError(EspBleError::BackendFailure, "failed to start scan");
    return false;
  }

  owner_->clearError();
  return true;
}

bool EspBleScanner::stop()
{
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (!BLEDevice::getScan()->stop())
  {
    owner_->setError(EspBleError::BackendFailure, "failed to stop scan");
    return false;
  }
  owner_->clearError();
  return true;
}

bool EspBleScanner::isScanning() const
{
  return owner_->initialized() && BLEDevice::getScan()->isScanning();
}

size_t EspBleScanner::droppedResultCount() const
{
  if (impl_ == nullptr)
  {
    return 0;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->dropped;
}

void EspBleScanner::dispatchPendingResults()
{
  if (impl_ == nullptr || !resultCallback_)
  {
    return;
  }

  while (true)
  {
    EspBleScanResult result;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->count == 0)
      {
        break;
      }
      result = std::move(impl_->queue[impl_->head]);
      impl_->head = (impl_->head + 1) % ScanQueueCapacity;
      --impl_->count;
    }
    resultCallback_(result);
  }
}

EspBle::EspBle() : advertising_(this), scanner_(this) {}

EspBle::~EspBle()
{
  end();
}

bool EspBle::begin(const EspBleConfig &config)
{
  if (initialized_)
  {
    clearError();
    return true;
  }
  if (BLEDevice::getInitialized())
  {
    setError(EspBleError::InvalidState, "Arduino BLE stack was initialized outside this EspBle instance");
    return false;
  }

  const char *deviceName = config.deviceName == nullptr ? "" : config.deviceName;
  if (!BLEDevice::init(deviceName))
  {
    setError(EspBleError::BackendFailure, "BLEDevice::init failed");
    return false;
  }

  initialized_ = true;
  clearError();
  return true;
}

void EspBle::end()
{
  if (!initialized_)
  {
    return;
  }

  if (scanner_.isScanning())
  {
    BLEDevice::getScan()->stop();
  }
  if (advertising_.isAdvertising())
  {
    BLEDevice::getAdvertising()->stop();
  }
  BLEDevice::deinit(false);
  initialized_ = false;
}

void EspBle::update()
{
  scanner_.dispatchPendingResults();
}

bool EspBle::initialized() const
{
  return initialized_;
}

EspBleAdvertising &EspBle::advertising()
{
  return advertising_;
}

EspBleScanner &EspBle::scanner()
{
  return scanner_;
}

EspBleError EspBle::lastError() const
{
  return lastError_;
}

const char *EspBle::lastErrorName() const
{
  switch (lastError_)
  {
  case EspBleError::None:
    return "NONE";
  case EspBleError::InvalidState:
    return "INVALID_STATE";
  case EspBleError::InvalidArgument:
    return "INVALID_ARGUMENT";
  case EspBleError::BackendFailure:
    return "BACKEND_FAILURE";
  case EspBleError::ResourceExhausted:
    return "RESOURCE_EXHAUSTED";
  }
  return "UNKNOWN";
}

const String &EspBle::lastErrorDetail() const
{
  return lastErrorDetail_;
}

void EspBle::clearError()
{
  lastError_ = EspBleError::None;
  lastErrorDetail_ = "";
}

void EspBle::setError(EspBleError error, const char *detail)
{
  lastError_ = error;
  lastErrorDetail_ = detail == nullptr ? "" : detail;
}
