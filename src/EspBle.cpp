#include "EspBle.h"

#include <BLEAdvertising.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>
#include <utility>

namespace
{
constexpr size_t ScanQueueCapacity = 16;
constexpr size_t ConnectionCapacity = 4;
constexpr size_t ConnectionEventQueueCapacity = 8;

bool uuidEquals(const String &left, const char *right)
{
  if (right == nullptr)
  {
    return false;
  }
  return left.equalsIgnoreCase(right);
}
} // namespace

struct EspBleImpl
{
  enum class EventType : uint8_t
  {
    Connected,
    Disconnected,
    Failed,
  };

  struct ConnectionSlot
  {
    bool used = false;
    EspBleConnection connection;
    BLEClient *client = nullptr;
  };

  struct Event
  {
    EventType type = EventType::Connected;
    EspBleConnection connection;
    EspBleConnectionFailure failure;
  };

  class ClientCallbacks : public BLEClientCallbacks
  {
  public:
    explicit ClientCallbacks(EspBleImpl *owner) : owner_(owner) {}

    void onConnect(BLEClient *client) override
    {
      BLEAddress peerAddress = client->getPeerAddress();
      if (!owner_->addConnection(
            client->getConnId(),
            peerAddress.toString(),
            peerAddress.getType(),
            EspBleRole::Central,
            client->getMTU(),
            client))
      {
        client->disconnect();
      }
    }

    void onDisconnect(BLEClient *client) override
    {
      owner_->removeClientConnection(client);
    }

  private:
    EspBleImpl *owner_;
  };

  class ServerCallbacks : public BLEServerCallbacks
  {
  public:
    explicit ServerCallbacks(EspBleImpl *owner) : owner_(owner) {}

    void onConnect(BLEServer *server, ble_gap_conn_desc *description) override
    {
      const BLEAddress peerAddress(description->peer_ota_addr);
      if (!owner_->addConnection(
            description->conn_handle,
            peerAddress.toString(),
            peerAddress.getType(),
            EspBleRole::Peripheral,
            server->getPeerMTU(description->conn_handle),
            nullptr))
      {
        server->disconnect(description->conn_handle);
      }
    }

    void onDisconnect(BLEServer *, ble_gap_conn_desc *description) override
    {
      owner_->removeServerConnection(description->conn_handle);
    }

  private:
    EspBleImpl *owner_;
  };

  explicit EspBleImpl(EspBle *owner)
      : owner(owner), clientCallbacks(this), serverCallbacks(this)
  {
  }

  bool pushEvent(const Event &event)
  {
    if (eventCount == ConnectionEventQueueCapacity)
    {
      ++droppedEvents;
      return false;
    }
    const size_t tail = (eventHead + eventCount) % ConnectionEventQueueCapacity;
    events[tail] = event;
    ++eventCount;
    return true;
  }

  bool addConnection(
    uint16_t handle,
    const String &peerAddress,
    uint8_t peerAddressType,
    EspBleRole localRole,
    uint16_t mtu,
    BLEClient *client)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (slot.used)
      {
        continue;
      }

      slot.used = true;
      slot.client = client;
      slot.connection.id = nextConnectionId++;
      if (nextConnectionId == 0)
      {
        nextConnectionId = 1;
      }
      slot.connection.handle = handle;
      slot.connection.peerAddress = peerAddress;
      slot.connection.peerAddressType = peerAddressType;
      slot.connection.localRole = localRole;
      slot.connection.mtu = mtu;

      Event event;
      event.type = EventType::Connected;
      event.connection = slot.connection;
      pushEvent(event);
      return true;
    }

    Event event;
    event.type = EventType::Failed;
    event.failure.peerAddress = peerAddress;
    event.failure.error = EspBleError::ResourceExhausted;
    event.failure.detail = "connection capacity exhausted";
    pushEvent(event);
    return false;
  }

  void removeClientConnection(BLEClient *client)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.client == client)
      {
        removeConnection(slot);
        return;
      }
    }
  }

  void removeServerConnection(uint16_t handle)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.client == nullptr && slot.connection.handle == handle)
      {
        removeConnection(slot);
        return;
      }
    }
  }

  void removeConnection(ConnectionSlot &slot)
  {
    Event event;
    event.type = EventType::Disconnected;
    event.connection = slot.connection;
    pushEvent(event);
    slot = ConnectionSlot();
  }

  void pushFailure(const EspBleScanResult &target, const char *detail)
  {
    std::lock_guard<std::mutex> lock(mutex);
    Event event;
    event.type = EventType::Failed;
    event.failure.peerAddress = target.address;
    event.failure.error = EspBleError::BackendFailure;
    event.failure.detail = detail;
    pushEvent(event);
  }

  static void connectTaskEntry(void *argument)
  {
    EspBleImpl *impl = static_cast<EspBleImpl *>(argument);
    EspBleScanResult target;
    uint32_t timeoutMilliseconds;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      target = impl->connectTarget;
      timeoutMilliseconds = impl->connectTimeoutMilliseconds;
    }

    BLEClient *client = BLEDevice::createClient();
    bool connected = false;
    if (client != nullptr)
    {
      client->setClientCallbacks(&impl->clientCallbacks);
      connected = client->connect(
        BLEAddress(target.address, target.addressType),
        target.addressType,
        timeoutMilliseconds);
    }
    if (!connected)
    {
      impl->pushFailure(target, client == nullptr ? "failed to create BLE client" : "BLE connection failed");
    }

    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->connecting = false;
      impl->connectTask = nullptr;
    }
    vTaskDelete(nullptr);
  }

  EspBle *owner;
  mutable std::mutex mutex;
  ConnectionSlot connections[ConnectionCapacity];
  Event events[ConnectionEventQueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  EspBleConnectionId nextConnectionId = 1;
  BLEServer *server = nullptr;
  bool connecting = false;
  TaskHandle_t connectTask = nullptr;
  EspBleScanResult connectTarget;
  uint32_t connectTimeoutMilliseconds = 10000;
  ClientCallbacks clientCallbacks;
  ServerCallbacks serverCallbacks;
};

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
  if (!owner_->preparePeripheral())
  {
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
  delete impl_;
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

  if (impl_ == nullptr)
  {
    impl_ = new EspBleImpl(this);
    if (impl_ == nullptr)
    {
      BLEDevice::deinit(false);
      setError(EspBleError::ResourceExhausted, "failed to allocate connection state");
      return false;
    }
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

  if (impl_ != nullptr)
  {
    while (true)
    {
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->connecting)
        {
          break;
        }
      }
      delay(1);
    }
  }
  BLEDevice::deinit(false);
  initialized_ = false;

  delete impl_;
  impl_ = nullptr;
}

void EspBle::update()
{
  scanner_.dispatchPendingResults();
  dispatchConnectionEvents();
}

bool EspBle::connect(const EspBleScanResult &scanResult, uint32_t timeoutMilliseconds)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (scanResult.address.isEmpty() || timeoutMilliseconds == 0)
  {
    setError(EspBleError::InvalidArgument, "peer address and a nonzero timeout are required");
    return false;
  }
  if (impl_ == nullptr)
  {
    setError(EspBleError::InvalidState, "connection state is unavailable");
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->connecting)
    {
      setError(EspBleError::InvalidState, "a connection attempt is already in progress");
      return false;
    }
    impl_->connectTarget = scanResult;
    impl_->connectTimeoutMilliseconds = timeoutMilliseconds;
    impl_->connecting = true;
  }

  TaskHandle_t task = nullptr;
  const BaseType_t result = xTaskCreate(
    EspBleImpl::connectTaskEntry,
    "espble-connect",
    6144,
    impl_,
    1,
    &task);
  if (result != pdPASS)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->connecting = false;
    setError(EspBleError::ResourceExhausted, "failed to create connection task");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->connectTask = task;
  }

  clearError();
  return true;
}

bool EspBle::disconnect(EspBleConnectionId connectionId)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }

  BLEClient *client = nullptr;
  BLEServer *server = nullptr;
  uint16_t handle = 0xffff;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (const EspBleImpl::ConnectionSlot &slot : impl_->connections)
    {
      if (slot.used && slot.connection.id == connectionId)
      {
        client = slot.client;
        server = impl_->server;
        handle = slot.connection.handle;
        break;
      }
    }
  }

  if (handle == 0xffff)
  {
    setError(EspBleError::InvalidArgument, "connection ID was not found");
    return false;
  }

  const int result = client != nullptr ? client->disconnect() : server->disconnect(handle);
  if (result != 0)
  {
    setError(EspBleError::BackendFailure, "failed to request disconnection");
    return false;
  }

  clearError();
  return true;
}

size_t EspBle::connectionCount() const
{
  if (impl_ == nullptr)
  {
    return 0;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  size_t count = 0;
  for (const EspBleImpl::ConnectionSlot &slot : impl_->connections)
  {
    if (slot.used)
    {
      ++count;
    }
  }
  return count;
}

bool EspBle::connection(EspBleConnectionId connectionId, EspBleConnection &connection) const
{
  if (impl_ == nullptr)
  {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  for (const EspBleImpl::ConnectionSlot &slot : impl_->connections)
  {
    if (slot.used && slot.connection.id == connectionId)
    {
      connection = slot.connection;
      return true;
    }
  }
  return false;
}

void EspBle::onConnected(ConnectionCallback callback)
{
  connectedCallback_ = std::move(callback);
}

void EspBle::onDisconnected(ConnectionCallback callback)
{
  disconnectedCallback_ = std::move(callback);
}

void EspBle::onConnectionFailed(ConnectionFailureCallback callback)
{
  connectionFailedCallback_ = std::move(callback);
}

bool EspBle::preparePeripheral()
{
  if (impl_ == nullptr)
  {
    setError(EspBleError::InvalidState, "connection state is unavailable");
    return false;
  }
  if (impl_->server != nullptr)
  {
    return true;
  }

  impl_->server = BLEDevice::createServer();
  if (impl_->server == nullptr)
  {
    setError(EspBleError::BackendFailure, "failed to create BLE server");
    return false;
  }
  impl_->server->setCallbacks(&impl_->serverCallbacks);
  impl_->server->advertiseOnDisconnect(false);
  return true;
}

void EspBle::dispatchConnectionEvents()
{
  if (impl_ == nullptr)
  {
    return;
  }

  while (true)
  {
    EspBleImpl::Event event;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->eventCount == 0)
      {
        break;
      }
      event = std::move(impl_->events[impl_->eventHead]);
      impl_->eventHead = (impl_->eventHead + 1) % ConnectionEventQueueCapacity;
      --impl_->eventCount;
    }

    switch (event.type)
    {
    case EspBleImpl::EventType::Connected:
      if (connectedCallback_)
      {
        connectedCallback_(event.connection);
      }
      break;
    case EspBleImpl::EventType::Disconnected:
      if (disconnectedCallback_)
      {
        disconnectedCallback_(event.connection);
      }
      break;
    case EspBleImpl::EventType::Failed:
      if (connectionFailedCallback_)
      {
        connectionFailedCallback_(event.failure);
      }
      break;
    }
  }
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
