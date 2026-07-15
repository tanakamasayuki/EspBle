#include "EspBle.h"

#include <BLEAdvertising.h>
#include <BLECharacteristic.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>
#include <BLEScan.h>
#include <BLEServer.h>
#include <BLEService.h>
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
    GattResult,
    ServerWrite,
    Notification,
    ServerSubscription,
    ServerSendResult,
    MtuChanged,
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
    EspBleGattResult gattResult;
    EspBleGattWrite serverWrite;
    EspBleGattNotification notification;
    EspBleGattSubscription serverSubscription;
    EspBleGattSendResult serverSendResult;
    EspBleMtuChanged mtuChanged;
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

    void onMtuChanged(
      BLEServer *,
      ble_gap_conn_desc *description,
      uint16_t mtu) override
    {
      owner_->updatePeripheralMtu(description->conn_handle, mtu);
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
      if (localRole == EspBleRole::Central && mtu != 23)
      {
        Event mtuEvent;
        mtuEvent.type = EventType::MtuChanged;
        mtuEvent.mtuChanged.connection = slot.connection;
        mtuEvent.mtuChanged.previousMtu = 23;
        pushEvent(mtuEvent);
      }
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

  void updatePeripheralMtu(uint16_t connectionHandle, uint16_t mtu)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.connection.handle == connectionHandle &&
          slot.connection.localRole == EspBleRole::Peripheral)
      {
        if (slot.connection.mtu == mtu)
        {
          return;
        }
        Event event;
        event.type = EventType::MtuChanged;
        event.mtuChanged.previousMtu = slot.connection.mtu;
        slot.connection.mtu = mtu;
        event.mtuChanged.connection = slot.connection;
        pushEvent(event);
        return;
      }
    }
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

  void pushGattResult(const EspBleGattResult &result)
  {
    std::lock_guard<std::mutex> lock(mutex);
    Event event;
    event.type = EventType::GattResult;
    event.gattResult = result;
    pushEvent(event);
  }

  void queueServerWrite(
    uint16_t connectionHandle,
    const String &serviceUuid,
    const String &characteristicUuid,
    const String &value)
  {
    std::lock_guard<std::mutex> lock(mutex);
    Event event;
    event.type = EventType::ServerWrite;
    event.serverWrite.serviceUuid = serviceUuid;
    event.serverWrite.characteristicUuid = characteristicUuid;
    event.serverWrite.value = value;
    for (const ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.connection.handle == connectionHandle &&
          slot.connection.localRole == EspBleRole::Peripheral)
      {
        event.serverWrite.connectionId = slot.connection.id;
        break;
      }
    }
    pushEvent(event);
  }

  EspBleConnectionId findPeripheralConnectionId(uint16_t connectionHandle) const
  {
    for (const ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.connection.handle == connectionHandle &&
          slot.connection.localRole == EspBleRole::Peripheral)
      {
        return slot.connection.id;
      }
    }
    return 0;
  }

  void queueNotification(
    EspBleConnectionId connectionId,
    const String &serviceUuid,
    const String &characteristicUuid,
    const uint8_t *data,
    size_t length,
    bool indication)
  {
    std::lock_guard<std::mutex> lock(mutex);
    Event event;
    event.type = EventType::Notification;
    event.notification.connectionId = connectionId;
    event.notification.serviceUuid = serviceUuid;
    event.notification.characteristicUuid = characteristicUuid;
    event.notification.value = length == 0
      ? String()
      : String(reinterpret_cast<const char *>(data), length);
    event.notification.indication = indication;
    pushEvent(event);
  }

  void queueServerSubscription(
    uint16_t connectionHandle,
    const String &serviceUuid,
    const String &characteristicUuid,
    uint16_t subscriptionValue)
  {
    std::lock_guard<std::mutex> lock(mutex);
    Event event;
    event.type = EventType::ServerSubscription;
    event.serverSubscription.connectionId = findPeripheralConnectionId(connectionHandle);
    event.serverSubscription.serviceUuid = serviceUuid;
    event.serverSubscription.characteristicUuid = characteristicUuid;
    event.serverSubscription.notifications = (subscriptionValue & 0x0001) != 0;
    event.serverSubscription.indications = (subscriptionValue & 0x0002) != 0;
    pushEvent(event);
  }

  void queueServerSendResult(const EspBleGattSendResult &result)
  {
    std::lock_guard<std::mutex> lock(mutex);
    Event event;
    event.type = EventType::ServerSendResult;
    event.serverSendResult = result;
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

  static void gattTaskEntry(void *argument)
  {
    EspBleImpl *impl = static_cast<EspBleImpl *>(argument);
    EspBleGattResult result;
    BLEClient *client = nullptr;
    String writeValue;
    bool response = true;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      result.operation = impl->gattOperation;
      result.connectionId = impl->gattConnectionId;
      result.serviceUuid = impl->gattServiceUuid;
      result.characteristicUuid = impl->gattCharacteristicUuid;
      writeValue = impl->gattWriteValue;
      response = impl->gattWriteResponse;
      for (const ConnectionSlot &slot : impl->connections)
      {
        if (slot.used && slot.connection.id == result.connectionId)
        {
          client = slot.client;
          break;
        }
      }
    }

    if (client == nullptr || !client->isConnected())
    {
      result.error = EspBleError::InvalidState;
      result.detail = "connection is not an active Central connection";
    }
    else
    {
      BLERemoteService *service = client->getService(result.serviceUuid.c_str());
      if (service == nullptr)
      {
        result.error = EspBleError::NotFound;
        result.detail = "GATT service was not found";
      }
      else
      {
        BLERemoteCharacteristic *characteristic =
          service->getCharacteristic(result.characteristicUuid.c_str());
        if (characteristic == nullptr)
        {
          result.error = EspBleError::NotFound;
          result.detail = "GATT characteristic was not found";
        }
        else
        {
          result.readable = characteristic->canRead();
          result.writable = characteristic->canWrite();
          result.writableWithoutResponse = characteristic->canWriteNoResponse();
          result.notifiable = characteristic->canNotify();
          result.indicatable = characteristic->canIndicate();

          if (result.operation == EspBleGattOperation::Discover)
          {
            result.success = true;
          }
          else if (result.operation == EspBleGattOperation::Read)
          {
            if (!result.readable)
            {
              result.error = EspBleError::InvalidState;
              result.detail = "GATT characteristic is not readable";
            }
            else
            {
              result.value = characteristic->readValue();
              result.success = true;
            }
          }
          else if (result.operation == EspBleGattOperation::Write)
          {
            const bool canWrite = response ? result.writable : result.writableWithoutResponse;
            if (!canWrite)
            {
              result.error = EspBleError::InvalidState;
              result.detail = response
                ? "GATT characteristic does not support write with response"
                : "GATT characteristic does not support write without response";
            }
            else
            {
              result.success = characteristic->writeValue(
                reinterpret_cast<uint8_t *>(const_cast<char *>(writeValue.c_str())),
                writeValue.length(),
                response);
              if (!result.success)
              {
                result.error = EspBleError::BackendFailure;
                result.detail = "GATT write failed";
              }
            }
          }
          else if (result.operation == EspBleGattOperation::Subscribe)
          {
            const bool notifications = response;
            if ((notifications && !result.notifiable) || (!notifications && !result.indicatable))
            {
              result.error = EspBleError::InvalidState;
              result.detail = notifications
                ? "GATT characteristic does not support notifications"
                : "GATT characteristic does not support indications";
            }
            else
            {
              const EspBleConnectionId connectionId = result.connectionId;
              const String serviceUuid = result.serviceUuid;
              const String characteristicUuid = result.characteristicUuid;
              result.success = characteristic->subscribe(
                notifications,
                [impl, connectionId, serviceUuid, characteristicUuid](
                  BLERemoteCharacteristic *,
                  uint8_t *data,
                  size_t length,
                  bool isNotification) {
                  impl->queueNotification(
                    connectionId,
                    serviceUuid,
                    characteristicUuid,
                    data,
                    length,
                    !isNotification);
                },
                true);
              if (result.success)
              {
                result.subscribedToNotifications = notifications;
                result.subscribedToIndications = !notifications;
              }
              else
              {
                result.error = EspBleError::BackendFailure;
                result.detail = "GATT subscription failed";
              }
            }
          }
          else
          {
            result.success = characteristic->unsubscribe(true);
            if (!result.success)
            {
              result.error = EspBleError::BackendFailure;
              result.detail = "GATT unsubscribe failed";
            }
          }
        }
      }
    }

    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->gattOperating = false;
      impl->gattTask = nullptr;
    }
    impl->pushGattResult(result);
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
  bool gattOperating = false;
  TaskHandle_t gattTask = nullptr;
  EspBleGattOperation gattOperation = EspBleGattOperation::Discover;
  EspBleConnectionId gattConnectionId = 0;
  String gattServiceUuid;
  String gattCharacteristicUuid;
  String gattWriteValue;
  bool gattWriteResponse = true;
};

struct EspBleGattServerImpl
{
  struct ServiceDefinition
  {
    String uuid;
    BLEService *backend = nullptr;
  };

  struct CharacteristicDefinition
  {
    String serviceUuid;
    String uuid;
    EspBleGattCharacteristicConfig config;
    String value;
    BLECharacteristic *backend = nullptr;
  };

  class BackendCallbacks : public BLECharacteristicCallbacks
  {
  public:
    explicit BackendCallbacks(EspBleGattServerImpl *owner) : owner_(owner) {}

    void onWrite(BLECharacteristic *characteristic, ble_gap_conn_desc *description) override
    {
      String serviceUuid;
      String characteristicUuid;
      String value;
      {
        std::lock_guard<std::mutex> lock(owner_->mutex);
        for (CharacteristicDefinition &definition : owner_->characteristics)
        {
          if (definition.backend == characteristic)
          {
            definition.value = characteristic->getValue();
            serviceUuid = definition.serviceUuid;
            characteristicUuid = definition.uuid;
            value = definition.value;
            break;
          }
        }
      }
      if (!characteristicUuid.isEmpty() && owner_->server->owner_->impl_ != nullptr)
      {
        owner_->server->owner_->impl_->queueServerWrite(
          description->conn_handle,
          serviceUuid,
          characteristicUuid,
          value);
      }
    }

    void onSubscribe(
      BLECharacteristic *characteristic,
      ble_gap_conn_desc *description,
      uint16_t subscriptionValue) override
    {
      String serviceUuid;
      String characteristicUuid;
      {
        std::lock_guard<std::mutex> lock(owner_->mutex);
        for (CharacteristicDefinition &definition : owner_->characteristics)
        {
          if (definition.backend == characteristic)
          {
            serviceUuid = definition.serviceUuid;
            characteristicUuid = definition.uuid;
            break;
          }
        }
      }
      if (!characteristicUuid.isEmpty() && owner_->server->owner_->impl_ != nullptr)
      {
        owner_->server->owner_->impl_->queueServerSubscription(
          description->conn_handle,
          serviceUuid,
          characteristicUuid,
          subscriptionValue);
      }
    }

    void onStatus(
      BLECharacteristic *characteristic,
      BLECharacteristicCallbacks::Status status,
      uint32_t code) override
    {
      std::lock_guard<std::mutex> lock(owner_->mutex);
      if (owner_->sending && owner_->sendBackend == characteristic)
      {
        // Arduino-ESP32 3.3.10's NimBLE path reports SUCCESS_INDICATE from
        // BLE_GAP_EVENT_NOTIFY_TX, but its synchronous wrapper can emit a
        // second timeout because the confirmation semaphore is not released.
        // Preserve the actual controller confirmation already observed.
        if (owner_->sendStatusReceived &&
            owner_->sendStatus == BLECharacteristicCallbacks::Status::SUCCESS_INDICATE &&
            status == BLECharacteristicCallbacks::Status::ERROR_INDICATE_TIMEOUT)
        {
          return;
        }
        owner_->sendStatus = status;
        owner_->sendStatusCode = code;
        owner_->sendStatusReceived = true;
      }
    }

  private:
    EspBleGattServerImpl *owner_;
  };

  explicit EspBleGattServerImpl(EspBleGattServer *server)
      : server(server), callbacks(this)
  {
  }

  static void sendTaskEntry(void *argument)
  {
    EspBleGattServerImpl *impl = static_cast<EspBleGattServerImpl *>(argument);
    BLECharacteristic *backend = nullptr;
    EspBleGattSendResult result;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      backend = impl->sendBackend;
      result.serviceUuid = impl->sendServiceUuid;
      result.characteristicUuid = impl->sendCharacteristicUuid;
      result.value = impl->sendValue;
      result.indication = impl->sendIndication;
      impl->sendStatusReceived = false;
    }

    backend->setValue(
      reinterpret_cast<const uint8_t *>(result.value.c_str()),
      result.value.length());
    backend->notify(!result.indication);

    BLECharacteristicCallbacks::Status status = BLECharacteristicCallbacks::Status::ERROR_GATT;
    uint32_t statusCode = 0;
    bool statusReceived = false;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      status = impl->sendStatus;
      statusCode = impl->sendStatusCode;
      statusReceived = impl->sendStatusReceived;
      impl->sending = false;
      impl->sendTask = nullptr;
    }

    result.success = statusReceived &&
      ((!result.indication && status == BLECharacteristicCallbacks::Status::SUCCESS_NOTIFY) ||
       (result.indication && status == BLECharacteristicCallbacks::Status::SUCCESS_INDICATE));
    if (!result.success)
    {
      result.error = EspBleError::BackendFailure;
      switch (status)
      {
      case BLECharacteristicCallbacks::Status::ERROR_NO_CLIENT:
        result.detail = "no connected GATT Client";
        break;
      case BLECharacteristicCallbacks::Status::ERROR_NO_SUBSCRIBER:
        result.detail = "no subscribed GATT Client";
        break;
      case BLECharacteristicCallbacks::Status::ERROR_NOTIFY_DISABLED:
        result.detail = "notifications are disabled";
        break;
      case BLECharacteristicCallbacks::Status::ERROR_INDICATE_DISABLED:
        result.detail = "indications are disabled";
        break;
      case BLECharacteristicCallbacks::Status::ERROR_INDICATE_TIMEOUT:
        result.detail = "indication confirmation timed out";
        break;
      case BLECharacteristicCallbacks::Status::ERROR_INDICATE_FAILURE:
        result.detail = "indication confirmation failed";
        break;
      default:
        result.detail = statusReceived
          ? String("GATT send failed with backend code ") + statusCode
          : String("GATT send completed without status");
        break;
      }
    }
    if (impl->server->owner_->impl_ != nullptr)
    {
      impl->server->owner_->impl_->queueServerSendResult(result);
    }
    vTaskDelete(nullptr);
  }

  EspBleGattServer *server;
  mutable std::mutex mutex;
  ServiceDefinition services[EspBleGattServer::MaxServices];
  size_t serviceCount = 0;
  CharacteristicDefinition characteristics[EspBleGattServer::MaxCharacteristics];
  size_t characteristicCount = 0;
  BackendCallbacks callbacks;
  bool realized = false;
  bool sending = false;
  TaskHandle_t sendTask = nullptr;
  BLECharacteristic *sendBackend = nullptr;
  String sendServiceUuid;
  String sendCharacteristicUuid;
  String sendValue;
  bool sendIndication = false;
  BLECharacteristicCallbacks::Status sendStatus = BLECharacteristicCallbacks::Status::ERROR_GATT;
  uint32_t sendStatusCode = 0;
  bool sendStatusReceived = false;
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

size_t EspBleConnection::maximumNotificationPayload() const
{
  return mtu > 3 ? mtu - 3 : 0;
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

EspBleGattServer::EspBleGattServer(EspBle *owner) : owner_(owner) {}

EspBleGattServer::~EspBleGattServer()
{
  delete impl_;
}

bool EspBleGattServer::addService(const char *serviceUuid)
{
  if (owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "GATT services must be configured before begin");
    return false;
  }
  if (serviceUuid == nullptr || serviceUuid[0] == '\0')
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT service UUID is empty");
    return false;
  }
  if (impl_ == nullptr)
  {
    impl_ = new EspBleGattServerImpl(this);
    if (impl_ == nullptr)
    {
      owner_->setError(EspBleError::ResourceExhausted, "failed to allocate GATT Server state");
      return false;
    }
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  for (size_t index = 0; index < impl_->serviceCount; ++index)
  {
    if (uuidEquals(impl_->services[index].uuid, serviceUuid))
    {
      owner_->clearError();
      return true;
    }
  }
  if (impl_->serviceCount == MaxServices)
  {
    owner_->setError(EspBleError::ResourceExhausted, "too many GATT services");
    return false;
  }

  impl_->services[impl_->serviceCount++].uuid = serviceUuid;
  owner_->clearError();
  return true;
}

bool EspBleGattServer::addCharacteristic(
  const char *serviceUuid,
  const char *characteristicUuid,
  const EspBleGattCharacteristicConfig &config)
{
  if (owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "GATT characteristics must be configured before begin");
    return false;
  }
  if (serviceUuid == nullptr || serviceUuid[0] == '\0' ||
      characteristicUuid == nullptr || characteristicUuid[0] == '\0')
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT service and characteristic UUIDs are required");
    return false;
  }
  if (!config.readable && !config.writable && !config.writableWithoutResponse &&
      !config.notifiable && !config.indicatable)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT characteristic has no properties");
    return false;
  }
  if (impl_ == nullptr)
  {
    owner_->setError(EspBleError::NotFound, "GATT service was not added");
    return false;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  bool serviceFound = false;
  for (size_t index = 0; index < impl_->serviceCount; ++index)
  {
    if (uuidEquals(impl_->services[index].uuid, serviceUuid))
    {
      serviceFound = true;
      break;
    }
  }
  if (!serviceFound)
  {
    owner_->setError(EspBleError::NotFound, "GATT service was not added");
    return false;
  }
  for (size_t index = 0; index < impl_->characteristicCount; ++index)
  {
    const auto &definition = impl_->characteristics[index];
    if (uuidEquals(definition.serviceUuid, serviceUuid) &&
        uuidEquals(definition.uuid, characteristicUuid))
    {
      owner_->setError(EspBleError::InvalidArgument, "GATT characteristic already exists");
      return false;
    }
  }
  if (impl_->characteristicCount == MaxCharacteristics)
  {
    owner_->setError(EspBleError::ResourceExhausted, "too many GATT characteristics");
    return false;
  }

  auto &definition = impl_->characteristics[impl_->characteristicCount++];
  definition.serviceUuid = serviceUuid;
  definition.uuid = characteristicUuid;
  definition.config = config;
  owner_->clearError();
  return true;
}

bool EspBleGattServer::setValue(
  const char *serviceUuid,
  const char *characteristicUuid,
  const uint8_t *data,
  size_t length)
{
  if (serviceUuid == nullptr || characteristicUuid == nullptr || (data == nullptr && length != 0))
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid GATT value arguments");
    return false;
  }
  if (impl_ == nullptr)
  {
    owner_->setError(EspBleError::NotFound, "GATT characteristic was not found");
    return false;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  for (size_t index = 0; index < impl_->characteristicCount; ++index)
  {
    auto &definition = impl_->characteristics[index];
    if (uuidEquals(definition.serviceUuid, serviceUuid) &&
        uuidEquals(definition.uuid, characteristicUuid))
    {
      definition.value = length == 0 ? String() : String(reinterpret_cast<const char *>(data), length);
      if (definition.backend != nullptr)
      {
        definition.backend->setValue(
          reinterpret_cast<const uint8_t *>(definition.value.c_str()),
          definition.value.length());
      }
      owner_->clearError();
      return true;
    }
  }

  owner_->setError(EspBleError::NotFound, "GATT characteristic was not found");
  return false;
}

bool EspBleGattServer::setValue(
  const char *serviceUuid,
  const char *characteristicUuid,
  const String &value)
{
  return setValue(
    serviceUuid,
    characteristicUuid,
    reinterpret_cast<const uint8_t *>(value.c_str()),
    value.length());
}

bool EspBleGattServer::value(
  const char *serviceUuid,
  const char *characteristicUuid,
  String &value) const
{
  if (impl_ == nullptr || serviceUuid == nullptr || characteristicUuid == nullptr)
  {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  for (size_t index = 0; index < impl_->characteristicCount; ++index)
  {
    const auto &definition = impl_->characteristics[index];
    if (uuidEquals(definition.serviceUuid, serviceUuid) &&
        uuidEquals(definition.uuid, characteristicUuid))
    {
      value = definition.value;
      return true;
    }
  }
  return false;
}

bool EspBleGattServer::notify(
  const char *serviceUuid,
  const char *characteristicUuid,
  const uint8_t *data,
  size_t length)
{
  return send(serviceUuid, characteristicUuid, data, length, false);
}

bool EspBleGattServer::notify(
  const char *serviceUuid,
  const char *characteristicUuid,
  const String &value)
{
  return notify(
    serviceUuid,
    characteristicUuid,
    reinterpret_cast<const uint8_t *>(value.c_str()),
    value.length());
}

bool EspBleGattServer::indicate(
  const char *serviceUuid,
  const char *characteristicUuid,
  const uint8_t *data,
  size_t length)
{
  return send(serviceUuid, characteristicUuid, data, length, true);
}

bool EspBleGattServer::indicate(
  const char *serviceUuid,
  const char *characteristicUuid,
  const String &value)
{
  return indicate(
    serviceUuid,
    characteristicUuid,
    reinterpret_cast<const uint8_t *>(value.c_str()),
    value.length());
}

bool EspBleGattServer::send(
  const char *serviceUuid,
  const char *characteristicUuid,
  const uint8_t *data,
  size_t length,
  bool indication)
{
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (serviceUuid == nullptr || serviceUuid[0] == '\0' ||
      characteristicUuid == nullptr || characteristicUuid[0] == '\0' ||
      (data == nullptr && length != 0) || impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid GATT send arguments");
    return false;
  }

  size_t maximumPayload = static_cast<size_t>(-1);
  bool hasPeripheralConnection = false;
  {
    std::lock_guard<std::mutex> lock(owner_->impl_->mutex);
    for (const EspBleImpl::ConnectionSlot &slot : owner_->impl_->connections)
    {
      if (slot.used && slot.connection.localRole == EspBleRole::Peripheral)
      {
        hasPeripheralConnection = true;
        const size_t connectionMaximum = slot.connection.maximumNotificationPayload();
        if (connectionMaximum < maximumPayload)
        {
          maximumPayload = connectionMaximum;
        }
      }
    }
  }
  if (hasPeripheralConnection && length > maximumPayload)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT send value exceeds negotiated MTU payload");
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->sending)
    {
      owner_->setError(EspBleError::InvalidState, "a GATT Server send is already in progress");
      return false;
    }

    EspBleGattServerImpl::CharacteristicDefinition *found = nullptr;
    for (size_t index = 0; index < impl_->characteristicCount; ++index)
    {
      auto &definition = impl_->characteristics[index];
      if (uuidEquals(definition.serviceUuid, serviceUuid) &&
          uuidEquals(definition.uuid, characteristicUuid))
      {
        found = &definition;
        break;
      }
    }
    if (found == nullptr || found->backend == nullptr)
    {
      owner_->setError(EspBleError::NotFound, "GATT characteristic was not found");
      return false;
    }
    if ((!indication && !found->config.notifiable) ||
        (indication && !found->config.indicatable))
    {
      owner_->setError(
        EspBleError::InvalidState,
        indication ? "GATT characteristic is not indicatable" : "GATT characteristic is not notifiable");
      return false;
    }

    found->value = length == 0
      ? String()
      : String(reinterpret_cast<const char *>(data), length);
    impl_->sendBackend = found->backend;
    impl_->sendServiceUuid = found->serviceUuid;
    impl_->sendCharacteristicUuid = found->uuid;
    impl_->sendValue = found->value;
    impl_->sendIndication = indication;
    impl_->sending = true;
  }

  TaskHandle_t task = nullptr;
  const BaseType_t result = xTaskCreate(
    EspBleGattServerImpl::sendTaskEntry,
    "espble-gatt-send",
    4096,
    impl_,
    1,
    &task);
  if (result != pdPASS)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->sending = false;
    owner_->setError(EspBleError::ResourceExhausted, "failed to create GATT Server send task");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->sendTask = task;
  }
  owner_->clearError();
  return true;
}

void EspBleGattServer::onWritten(WriteCallback callback)
{
  writeCallback_ = std::move(callback);
}

void EspBleGattServer::onSubscriptionChanged(SubscriptionCallback callback)
{
  subscriptionCallback_ = std::move(callback);
}

void EspBleGattServer::onSent(SendCallback callback)
{
  sendCallback_ = std::move(callback);
}

bool EspBleGattServer::realize()
{
  if (impl_ == nullptr || impl_->serviceCount == 0)
  {
    return true;
  }
  if (impl_->realized)
  {
    return true;
  }
  if (!owner_->preparePeripheral())
  {
    return false;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  BLEServer *server = owner_->impl_->server;
  for (size_t serviceIndex = 0; serviceIndex < impl_->serviceCount; ++serviceIndex)
  {
    auto &serviceDefinition = impl_->services[serviceIndex];
    serviceDefinition.backend = server->createService(serviceDefinition.uuid.c_str());
    if (serviceDefinition.backend == nullptr)
    {
      owner_->setError(EspBleError::BackendFailure, "failed to create GATT service");
      return false;
    }
    for (size_t characteristicIndex = 0;
         characteristicIndex < impl_->characteristicCount;
         ++characteristicIndex)
    {
      auto &characteristicDefinition = impl_->characteristics[characteristicIndex];
      if (!uuidEquals(characteristicDefinition.serviceUuid, serviceDefinition.uuid.c_str()))
      {
        continue;
      }

      uint32_t properties = 0;
      const auto &config = characteristicDefinition.config;
      if (config.readable) properties |= BLECharacteristic::PROPERTY_READ;
      if (config.writable) properties |= BLECharacteristic::PROPERTY_WRITE;
      if (config.writableWithoutResponse) properties |= BLECharacteristic::PROPERTY_WRITE_NR;
      if (config.notifiable) properties |= BLECharacteristic::PROPERTY_NOTIFY;
      if (config.indicatable) properties |= BLECharacteristic::PROPERTY_INDICATE;

      characteristicDefinition.backend = serviceDefinition.backend->createCharacteristic(
        characteristicDefinition.uuid.c_str(), properties);
      if (characteristicDefinition.backend == nullptr)
      {
        owner_->setError(EspBleError::BackendFailure, "failed to create GATT characteristic");
        return false;
      }
      characteristicDefinition.backend->setCallbacks(&impl_->callbacks);
      characteristicDefinition.backend->setValue(
        reinterpret_cast<const uint8_t *>(characteristicDefinition.value.c_str()),
        characteristicDefinition.value.length());
    }
    if (!serviceDefinition.backend->start())
    {
      owner_->setError(EspBleError::BackendFailure, "failed to start GATT service");
      return false;
    }
  }
  impl_->realized = true;
  return true;
}

void EspBleGattServer::resetBackend()
{
  if (impl_ == nullptr)
  {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  for (size_t index = 0; index < impl_->serviceCount; ++index)
  {
    impl_->services[index].backend = nullptr;
  }
  for (size_t index = 0; index < impl_->characteristicCount; ++index)
  {
    impl_->characteristics[index].backend = nullptr;
  }
  impl_->realized = false;
}

void EspBleGattServer::dispatchWrite(const EspBleGattWrite &write)
{
  if (writeCallback_)
  {
    writeCallback_(write);
  }
}

void EspBleGattServer::dispatchSubscription(const EspBleGattSubscription &subscription)
{
  if (subscriptionCallback_)
  {
    subscriptionCallback_(subscription);
  }
}

void EspBleGattServer::dispatchSendResult(const EspBleGattSendResult &result)
{
  if (sendCallback_)
  {
    sendCallback_(result);
  }
}

EspBle::EspBle() : advertising_(this), scanner_(this), gattServer_(this) {}

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
  if (config.preferredMtu < 23 || config.preferredMtu > 517)
  {
    setError(EspBleError::InvalidArgument, "preferred MTU must be between 23 and 517");
    return false;
  }

  const char *deviceName = config.deviceName == nullptr ? "" : config.deviceName;
  if (!BLEDevice::init(deviceName))
  {
    setError(EspBleError::BackendFailure, "BLEDevice::init failed");
    return false;
  }
  if (BLEDevice::setMTU(config.preferredMtu) != ESP_OK)
  {
    BLEDevice::deinit(false);
    setError(EspBleError::BackendFailure, "failed to set preferred MTU");
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

  if (!gattServer_.realize())
  {
    BLEDevice::deinit(false);
    gattServer_.resetBackend();
    delete impl_;
    impl_ = nullptr;
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

  if (impl_ != nullptr)
  {
    while (true)
    {
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->connecting && !impl_->gattOperating)
        {
          break;
        }
      }
      delay(1);
    }
  }
  if (gattServer_.impl_ != nullptr)
  {
    while (true)
    {
      {
        std::lock_guard<std::mutex> lock(gattServer_.impl_->mutex);
        if (!gattServer_.impl_->sending)
        {
          break;
        }
      }
      delay(1);
    }
  }
  BLEDevice::deinit(false);
  initialized_ = false;
  gattServer_.resetBackend();

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
    if (impl_->gattOperating && impl_->gattConnectionId == connectionId)
    {
      setError(EspBleError::InvalidState, "a GATT operation is active on this connection");
      return false;
    }
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

void EspBle::onMtuChanged(MtuChangedCallback callback)
{
  mtuChangedCallback_ = std::move(callback);
}

bool EspBle::discoverCharacteristic(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid)
{
  return startGattOperation(
    EspBleGattOperation::Discover, connectionId, serviceUuid, characteristicUuid);
}

bool EspBle::readCharacteristic(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid)
{
  return startGattOperation(
    EspBleGattOperation::Read, connectionId, serviceUuid, characteristicUuid);
}

bool EspBle::writeCharacteristic(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  const uint8_t *data,
  size_t length,
  bool response)
{
  return startGattOperation(
    EspBleGattOperation::Write,
    connectionId,
    serviceUuid,
    characteristicUuid,
    data,
    length,
    response);
}

bool EspBle::subscribe(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  bool notifications)
{
  return startGattOperation(
    EspBleGattOperation::Subscribe,
    connectionId,
    serviceUuid,
    characteristicUuid,
    nullptr,
    0,
    notifications);
}

bool EspBle::unsubscribe(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid)
{
  return startGattOperation(
    EspBleGattOperation::Unsubscribe,
    connectionId,
    serviceUuid,
    characteristicUuid);
}

bool EspBle::writeCharacteristic(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  const String &value,
  bool response)
{
  return writeCharacteristic(
    connectionId,
    serviceUuid,
    characteristicUuid,
    reinterpret_cast<const uint8_t *>(value.c_str()),
    value.length(),
    response);
}

void EspBle::onCharacteristicDiscovered(GattResultCallback callback)
{
  characteristicDiscoveredCallback_ = std::move(callback);
}

void EspBle::onCharacteristicRead(GattResultCallback callback)
{
  characteristicReadCallback_ = std::move(callback);
}

void EspBle::onCharacteristicWritten(GattResultCallback callback)
{
  characteristicWrittenCallback_ = std::move(callback);
}

void EspBle::onSubscribed(GattResultCallback callback)
{
  subscribedCallback_ = std::move(callback);
}

void EspBle::onUnsubscribed(GattResultCallback callback)
{
  unsubscribedCallback_ = std::move(callback);
}

void EspBle::onNotification(
  std::function<void(const EspBleGattNotification &notification)> callback)
{
  notificationCallback_ = std::move(callback);
}

bool EspBle::startGattOperation(
  EspBleGattOperation operation,
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  const uint8_t *data,
  size_t length,
  bool response)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (serviceUuid == nullptr || serviceUuid[0] == '\0' ||
      characteristicUuid == nullptr || characteristicUuid[0] == '\0' ||
      (data == nullptr && length != 0))
  {
    setError(EspBleError::InvalidArgument, "invalid GATT operation arguments");
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->gattOperating)
    {
      setError(EspBleError::InvalidState, "a GATT operation is already in progress");
      return false;
    }
    bool centralConnectionFound = false;
    for (const EspBleImpl::ConnectionSlot &slot : impl_->connections)
    {
      if (slot.used && slot.connection.id == connectionId && slot.client != nullptr)
      {
        centralConnectionFound = true;
        break;
      }
    }
    if (!centralConnectionFound)
    {
      setError(EspBleError::InvalidArgument, "Central connection ID was not found");
      return false;
    }

    impl_->gattOperation = operation;
    impl_->gattConnectionId = connectionId;
    impl_->gattServiceUuid = serviceUuid;
    impl_->gattCharacteristicUuid = characteristicUuid;
    impl_->gattWriteValue = length == 0
      ? String()
      : String(reinterpret_cast<const char *>(data), length);
    impl_->gattWriteResponse = response;
    impl_->gattOperating = true;
  }

  TaskHandle_t task = nullptr;
  const BaseType_t result = xTaskCreate(
    EspBleImpl::gattTaskEntry,
    "espble-gatt",
    6144,
    impl_,
    1,
    &task);
  if (result != pdPASS)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->gattOperating = false;
    setError(EspBleError::ResourceExhausted, "failed to create GATT operation task");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->gattTask = task;
  }
  clearError();
  return true;
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
    case EspBleImpl::EventType::GattResult:
    {
      GattResultCallback *callback = nullptr;
      switch (event.gattResult.operation)
      {
      case EspBleGattOperation::Discover:
        callback = &characteristicDiscoveredCallback_;
        break;
      case EspBleGattOperation::Read:
        callback = &characteristicReadCallback_;
        break;
      case EspBleGattOperation::Write:
        callback = &characteristicWrittenCallback_;
        break;
      case EspBleGattOperation::Subscribe:
        callback = &subscribedCallback_;
        break;
      case EspBleGattOperation::Unsubscribe:
        callback = &unsubscribedCallback_;
        break;
      }
      if (callback != nullptr && *callback)
      {
        (*callback)(event.gattResult);
      }
      break;
    }
    case EspBleImpl::EventType::ServerWrite:
      gattServer_.dispatchWrite(event.serverWrite);
      break;
    case EspBleImpl::EventType::Notification:
      if (notificationCallback_)
      {
        notificationCallback_(event.notification);
      }
      break;
    case EspBleImpl::EventType::ServerSubscription:
      gattServer_.dispatchSubscription(event.serverSubscription);
      break;
    case EspBleImpl::EventType::ServerSendResult:
      gattServer_.dispatchSendResult(event.serverSendResult);
      break;
    case EspBleImpl::EventType::MtuChanged:
      if (mtuChangedCallback_)
      {
        mtuChangedCallback_(event.mtuChanged);
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

EspBleGattServer &EspBle::gattServer()
{
  return gattServer_;
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
  case EspBleError::NotFound:
    return "NOT_FOUND";
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
