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
#include <BLESecurity.h>
#include <BLEUtils.h>
#include <host/ble_hs_mbuf.h>
#include <host/ble_store.h>
#include <os/os_mbuf.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <mutex>
#include <utility>

#include "keymap/keymap_da_dk.h"
#include "keymap/keymap_de_de.h"
#include "keymap/keymap_en_gb.h"
#include "keymap/keymap_es_es.h"
#include "keymap/keymap_fi_fi.h"
#include "keymap/keymap_fr_ch.h"
#include "keymap/keymap_fr_fr.h"
#include "keymap/keymap_hu_hu.h"
#include "keymap/keymap_it_it.h"
#include "keymap/keymap_ja_jp.h"
#include "keymap/keymap_nb_no.h"
#include "keymap/keymap_nl_nl.h"
#include "keymap/keymap_pt_br.h"
#include "keymap/keymap_pt_pt.h"
#include "keymap/keymap_sv_se.h"

namespace
{
constexpr size_t ScanQueueCapacity = 16;
constexpr size_t ConnectionCapacity = 4;
constexpr size_t ConnectionEventQueueCapacity = 8;
constexpr uint16_t HidKeyboardAppearance = 0x03c1;
#if defined(CONFIG_BT_NIMBLE_MAX_BONDS)
constexpr size_t BondCapacity = CONFIG_BT_NIMBLE_MAX_BONDS;
#else
constexpr size_t BondCapacity = 16;
#endif

bool uuidEquals(const String &left, const char *right)
{
  if (right == nullptr || right[0] == '\0' || left.isEmpty())
  {
    return false;
  }
  if (left.equalsIgnoreCase(right))
  {
    return true;
  }
  return BLEUUID(left.c_str()).equals(BLEUUID(right));
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
    SecurityChanged,
    PasskeyDisplayed,
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
    EspBleSecurityChanged securityChanged;
    EspBlePasskeyDisplayed passkeyDisplayed;
  };

  class ClientCallbacks : public BLEClientCallbacks
  {
  public:
    explicit ClientCallbacks(EspBleImpl *owner) : owner_(owner) {}

    void onConnect(BLEClient *client) override
    {
      BLEAddress peerAddress = client->getPeerAddress();
      ble_gap_conn_desc description{};
      ble_gap_conn_find(client->getConnId(), &description);
      if (!owner_->addConnection(
            client->getConnId(),
            peerAddress.toString(),
            peerAddress.getType(),
            EspBleRole::Central,
            client->getMTU(),
            description.sec_state.encrypted,
            description.sec_state.authenticated,
            description.sec_state.bonded,
            description.sec_state.key_size,
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
            description->sec_state.encrypted,
            description->sec_state.authenticated,
            description->sec_state.bonded,
            description->sec_state.key_size,
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

  class SecurityCallbacks : public BLESecurityCallbacks
  {
  public:
    explicit SecurityCallbacks(EspBleImpl *owner) : owner_(owner) {}

    void onAuthenticationComplete(ble_gap_conn_desc *description) override
    {
      if (description != nullptr)
      {
        owner_->updateSecurity(description->conn_handle, description->sec_state);
      }
    }

    void onPassKeyNotify(uint32_t passkey) override
    {
      owner_->queuePasskeyDisplayed(passkey);
    }

  private:
    EspBleImpl *owner_;
  };

  explicit EspBleImpl(EspBle *owner)
      : owner(owner), clientCallbacks(this), serverCallbacks(this), securityCallbacks(this)
  {
  }

  ~EspBleImpl()
  {
    delete securityBackend;
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
    bool encrypted,
    bool authenticated,
    bool bonded,
    uint8_t encryptionKeySize,
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
      slot.connection.encrypted = encrypted;
      slot.connection.authenticated = authenticated;
      slot.connection.bonded = bonded;
      slot.connection.encryptionKeySize = encryptionKeySize;

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

  void updateSecurity(uint16_t connectionHandle, const ble_gap_sec_state &state)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (!slot.used || slot.connection.handle != connectionHandle)
      {
        continue;
      }

      slot.connection.encrypted = state.encrypted;
      slot.connection.authenticated = state.authenticated;
      slot.connection.bonded = state.bonded;
      slot.connection.encryptionKeySize = state.key_size;

      Event event;
      event.type = EventType::SecurityChanged;
      event.securityChanged.connection = slot.connection;
      event.securityChanged.success = state.encrypted;
      if (!event.securityChanged.success)
      {
        event.securityChanged.error = EspBleError::BackendFailure;
        event.securityChanged.detail = "BLE pairing or encryption failed";
      }
      pushEvent(event);
      return;
    }
  }

  void queuePasskeyDisplayed(uint32_t passkey)
  {
    std::lock_guard<std::mutex> lock(mutex);
    const ConnectionSlot *selected = nullptr;
    for (const ConnectionSlot &slot : connections)
    {
      if (!slot.used)
      {
        continue;
      }
      if (selected == nullptr)
      {
        selected = &slot;
      }
      if (!slot.connection.encrypted)
      {
        selected = &slot;
        break;
      }
    }
    if (selected == nullptr)
    {
      return;
    }

    Event event;
    event.type = EventType::PasskeyDisplayed;
    event.passkeyDisplayed.connection = selected->connection;
    event.passkeyDisplayed.passkey = passkey;
    pushEvent(event);
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
  SecurityCallbacks securityCallbacks;
  BLESecurity *securityBackend = nullptr;
  bool securityEnabled = false;
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

struct EspBleHidKeyboardDeviceImpl
{
  static constexpr size_t OutputQueueCapacity = 8;

  explicit EspBleHidKeyboardDeviceImpl(EspBleHidKeyboardDevice *device)
      : device(device)
  {
  }

  static int accessCallback(
    uint16_t connectionHandle,
    uint16_t attributeHandle,
    ble_gatt_access_ctxt *context,
    void *argument)
  {
    return static_cast<EspBleHidKeyboardDeviceImpl *>(argument)->handleAccess(
      connectionHandle, attributeHandle, context);
  }

  static int appendValue(os_mbuf *buffer, const void *value, size_t length)
  {
    return os_mbuf_append(buffer, value, static_cast<uint16_t>(length)) == 0
      ? 0
      : BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  int handleAccess(
    uint16_t connectionHandle,
    uint16_t,
    ble_gatt_access_ctxt *context)
  {
    if (context->op == BLE_GATT_ACCESS_OP_READ_CHR)
    {
      if (context->chr == &hidCharacteristics[0])
      {
        return appendValue(context->om, hidInformation, sizeof(hidInformation));
      }
      if (context->chr == &hidCharacteristics[1])
      {
        return appendValue(context->om, reportMap, sizeof(reportMap));
      }
      if (context->chr == &hidCharacteristics[3])
      {
        return appendValue(context->om, inputValue, sizeof(inputValue));
      }
      if (context->chr == &hidCharacteristics[4])
      {
        return appendValue(context->om, &outputValue, 1);
      }
      if (context->chr == &deviceInformationCharacteristics[0])
      {
        const char *manufacturer = config.manufacturer == nullptr ? "" : config.manufacturer;
        return appendValue(context->om, manufacturer, strlen(manufacturer));
      }
      if (context->chr == &deviceInformationCharacteristics[1])
      {
        return appendValue(context->om, pnpId, sizeof(pnpId));
      }
      if (context->chr == &batteryCharacteristics[0])
      {
        return appendValue(context->om, &batteryLevel, 1);
      }
    }
    else if (context->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
      if (context->chr == &hidCharacteristics[2])
      {
        return 0;
      }
      if (context->chr == &hidCharacteristics[4])
      {
        uint16_t length = 0;
        uint8_t value = 0;
        if (ble_hs_mbuf_to_flat(context->om, &value, 1, &length) != 0 || length != 1)
        {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        outputValue = value;
        queueOutputReport(connectionHandle, value);
        return 0;
      }
    }
    else if (context->op == BLE_GATT_ACCESS_OP_READ_DSC)
    {
      if (context->dsc == &inputDescriptors[0])
      {
        const uint8_t reference[] = {config.reportId, 0x01};
        return appendValue(context->om, reference, sizeof(reference));
      }
      if (context->dsc == &outputDescriptors[0])
      {
        const uint8_t reference[] = {config.reportId, 0x02};
        return appendValue(context->om, reference, sizeof(reference));
      }
    }
    return BLE_ATT_ERR_UNLIKELY;
  }

  void queueOutputReport(uint16_t connectionHandle, uint8_t leds)
  {
    EspBleHidKeyboardOutputReport report;
    report.leds = leds;
    if (device->owner_->impl_ != nullptr)
    {
      std::lock_guard<std::mutex> connectionLock(device->owner_->impl_->mutex);
      report.connectionId = device->owner_->impl_->findPeripheralConnectionId(connectionHandle);
    }

    std::lock_guard<std::mutex> lock(mutex);
    if (outputCount == OutputQueueCapacity)
    {
      ++droppedOutputReports;
      return;
    }
    const size_t tail = (outputHead + outputCount) % OutputQueueCapacity;
    outputReports[tail] = report;
    ++outputCount;
  }

  EspBleHidKeyboardDevice *device;
  mutable std::mutex mutex;
  EspBleHidKeyboardDeviceConfig config;
  bool configured = false;
  bool realized = false;
  uint16_t inputValueHandle = 0;
  uint16_t outputValueHandle = 0;
  uint16_t batteryValueHandle = 0;
  uint8_t inputValue[8] = {};
  uint8_t outputValue = 0;
  uint8_t batteryLevel = 100;
  uint8_t hidInformation[4] = {0x11, 0x01, 0x00, 0x01};
  uint8_t pnpId[7] = {};
  uint8_t reportMap[65] = {};
  ble_uuid16_t hidServiceUuid = BLE_UUID16_INIT(0x1812);
  ble_uuid16_t deviceInformationServiceUuid = BLE_UUID16_INIT(0x180a);
  ble_uuid16_t batteryServiceUuid = BLE_UUID16_INIT(0x180f);
  ble_uuid16_t hidInformationUuid = BLE_UUID16_INIT(0x2a4a);
  ble_uuid16_t reportMapUuid = BLE_UUID16_INIT(0x2a4b);
  ble_uuid16_t hidControlPointUuid = BLE_UUID16_INIT(0x2a4c);
  ble_uuid16_t reportUuid = BLE_UUID16_INIT(0x2a4d);
  ble_uuid16_t reportReferenceUuid = BLE_UUID16_INIT(0x2908);
  ble_uuid16_t manufacturerUuid = BLE_UUID16_INIT(0x2a29);
  ble_uuid16_t pnpIdUuid = BLE_UUID16_INIT(0x2a50);
  ble_uuid16_t batteryLevelUuid = BLE_UUID16_INIT(0x2a19);
  ble_gatt_dsc_def inputDescriptors[2] = {};
  ble_gatt_dsc_def outputDescriptors[2] = {};
  ble_gatt_chr_def hidCharacteristics[6] = {};
  ble_gatt_chr_def deviceInformationCharacteristics[3] = {};
  ble_gatt_chr_def batteryCharacteristics[2] = {};
  ble_gatt_svc_def services[4] = {};
  EspBleHidKeyboardOutputReport outputReports[OutputQueueCapacity];
  size_t outputHead = 0;
  size_t outputCount = 0;
  size_t droppedOutputReports = 0;
};

struct EspBleHidKeyboardHostImpl
{
  static constexpr size_t QueueCapacity = 8;

  struct Connection
  {
    bool used = false;
    EspBleConnectionId connectionId = 0;
    uint8_t reportId = 0;
    BLERemoteCharacteristic *inputReport = nullptr;
    BLERemoteCharacteristic *outputReport = nullptr;
    uint8_t keys[EspBleHidKeyboardState::BitmapSize] = {};
    uint8_t modifiers = 0;
    bool numLock = false;
    bool capsLock = false;
    bool scrollLock = false;
    bool compose = false;
    bool kana = false;
  };

  enum class EventType : uint8_t
  {
    Discovery,
    State,
  };

  struct Event
  {
    EventType type = EventType::Discovery;
    EspBleHidKeyboardHostDiscovery discovery;
    EspBleHidKeyboardState state;
  };

  explicit EspBleHidKeyboardHostImpl(EspBleHidKeyboardHost *host) : host(host) {}

  static bool containsSequence(
    const uint8_t *data,
    size_t length,
    const uint8_t *sequence,
    size_t sequenceLength)
  {
    if (sequenceLength == 0 || sequenceLength > length)
    {
      return false;
    }
    for (size_t offset = 0; offset + sequenceLength <= length; ++offset)
    {
      if (memcmp(data + offset, sequence, sequenceLength) == 0)
      {
        return true;
      }
    }
    return false;
  }

  static bool supportsSixKeyKeyboard(const String &reportMap)
  {
    const uint8_t *data = reinterpret_cast<const uint8_t *>(reportMap.c_str());
    const size_t length = reportMap.length();
    const uint8_t keyboardApplication[] = {0x05, 0x01, 0x09, 0x06, 0xa1, 0x01};
    const uint8_t modifiers[] = {0x19, 0xe0, 0x29, 0xe7};
    const uint8_t sixKeys[] = {0x95, 0x06, 0x75, 0x08};
    const uint8_t keyboardUsages[] = {0x05, 0x07, 0x19, 0x00};
    return containsSequence(data, length, keyboardApplication, sizeof(keyboardApplication)) &&
      containsSequence(data, length, modifiers, sizeof(modifiers)) &&
      containsSequence(data, length, sixKeys, sizeof(sixKeys)) &&
      containsSequence(data, length, keyboardUsages, sizeof(keyboardUsages));
  }

  static uint8_t keypadUsageToAscii(uint8_t usage, bool numLock)
  {
    switch (usage)
    {
    case 0x54: return '/';
    case 0x55: return '*';
    case 0x56: return '-';
    case 0x57: return '+';
    case 0x58: return '\r';
    case 0x67: return '=';
    default: break;
    }
    if (!numLock)
    {
      return 0;
    }
    if (usage >= 0x59 && usage <= 0x61)
    {
      return static_cast<uint8_t>('1' + usage - 0x59);
    }
    if (usage == 0x62) return '0';
    if (usage == 0x63) return '.';
    return 0;
  }

  static uint8_t usageToAscii(
    uint8_t usage,
    uint8_t modifiers,
    EspBleKeyboardLayout layout,
    bool capsLock,
    bool numLock)
  {
    if ((usage >= 0x54 && usage <= 0x63) || usage == 0x67)
    {
      return keypadUsageToAscii(usage, numLock);
    }
    const bool shifted = (modifiers &
      (EspBleHidKeyboardInputReport::LeftShift |
       EspBleHidKeyboardInputReport::RightShift)) != 0;
    const bool effectiveShift =
      usage >= 0x04 && usage <= 0x1d ? shifted != capsLock : shifted;
    const uint8_t (*table)[2] = nullptr;
    size_t tableSize = 128;
    bool useEnUsTable = false;
    switch (layout)
    {
    case EspBleKeyboardLayout::DaDk: table = KEYCODE_TO_ASCII_DA_DK; break;
    case EspBleKeyboardLayout::DeDe: table = KEYCODE_TO_ASCII_DE_DE; break;
    case EspBleKeyboardLayout::EnGb: table = KEYCODE_TO_ASCII_EN_GB; break;
    case EspBleKeyboardLayout::EsEs: table = KEYCODE_TO_ASCII_ES_ES; break;
    case EspBleKeyboardLayout::FiFi: table = KEYCODE_TO_ASCII_FI_FI; break;
    case EspBleKeyboardLayout::FrCh: table = KEYCODE_TO_ASCII_FR_CH; break;
    case EspBleKeyboardLayout::FrFr: table = KEYCODE_TO_ASCII_FR_FR; break;
    case EspBleKeyboardLayout::HuHu: table = KEYCODE_TO_ASCII_HU_HU; break;
    case EspBleKeyboardLayout::ItIt: table = KEYCODE_TO_ASCII_IT_IT; break;
    case EspBleKeyboardLayout::JaJp:
      table = KEYCODE_TO_ASCII_JA_JP;
      tableSize = 0x90;
      break;
    case EspBleKeyboardLayout::NbNo: table = KEYCODE_TO_ASCII_NB_NO; break;
    case EspBleKeyboardLayout::NlNl: table = KEYCODE_TO_ASCII_NL_NL; break;
    case EspBleKeyboardLayout::PtBr: table = KEYCODE_TO_ASCII_PT_BR; break;
    case EspBleKeyboardLayout::PtPt: table = KEYCODE_TO_ASCII_PT_PT; break;
    case EspBleKeyboardLayout::SvSe: table = KEYCODE_TO_ASCII_SV_SE; break;
    case EspBleKeyboardLayout::EnUs:
    case EspBleKeyboardLayout::KoKr:
    case EspBleKeyboardLayout::ZhCn:
    case EspBleKeyboardLayout::ZhTw:
      useEnUsTable = true;
      break;
    default: return 0;
    }
    if (table != nullptr)
    {
      return usage < tableSize ? table[usage][effectiveShift ? 1 : 0] : 0;
    }
    if (!useEnUsTable)
    {
      return 0;
    }

    if (usage >= 0x04 && usage <= 0x1d)
    {
      const uint8_t letter = static_cast<uint8_t>('a' + usage - 0x04);
      return effectiveShift ? static_cast<uint8_t>(letter - 'a' + 'A') : letter;
    }
    if (usage >= 0x1e && usage <= 0x27)
    {
      static const uint8_t normal[] = "1234567890";
      static const uint8_t shiftedDigits[] = "!@#$%^&*()";
      const size_t index = usage - 0x1e;
      return effectiveShift ? shiftedDigits[index] : normal[index];
    }
    switch (usage)
    {
    case 0x28: return '\r';
    case 0x29: return 0x1b;
    case 0x2a: return '\b';
    case 0x2b: return '\t';
    case 0x2c: return ' ';
    default: break;
    }
    switch (usage)
    {
    case 0x2d: return effectiveShift ? '_' : '-';
    case 0x2e: return effectiveShift ? '+' : '=';
    case 0x2f: return effectiveShift ? '{' : '[';
    case 0x30: return effectiveShift ? '}' : ']';
    case 0x31: return effectiveShift ? '|' : '\\';
    case 0x32: return effectiveShift ? '~' : '#';
    case 0x33: return effectiveShift ? ':' : ';';
    case 0x34: return effectiveShift ? '"' : '\'';
    case 0x35: return effectiveShift ? '~' : '`';
    case 0x36: return effectiveShift ? '<' : ',';
    case 0x37: return effectiveShift ? '>' : '.';
    case 0x38: return effectiveShift ? '?' : '/';
    default: return 0;
    }
  }

  Connection *findConnection(EspBleConnectionId connectionId)
  {
    for (Connection &connection : connections)
    {
      if (connection.used && connection.connectionId == connectionId)
      {
        return &connection;
      }
    }
    return nullptr;
  }

  Connection *allocateConnection(EspBleConnectionId connectionId)
  {
    Connection *existing = findConnection(connectionId);
    if (existing != nullptr)
    {
      return existing;
    }
    for (Connection &connection : connections)
    {
      if (!connection.used)
      {
        connection.used = true;
        connection.connectionId = connectionId;
        return &connection;
      }
    }
    return nullptr;
  }

  void pushEvent(const Event &event)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (eventCount == QueueCapacity)
    {
      ++droppedEvents;
      return;
    }
    events[(eventHead + eventCount) % QueueCapacity] = event;
    ++eventCount;
  }

  void queueInputReport(
    EspBleConnectionId connectionId,
    uint8_t reportId,
    const uint8_t *data,
    size_t length)
  {
    if (data == nullptr || length != 8)
    {
      return;
    }

    Event event;
    event.type = EventType::State;
    event.state.connectionId = connectionId;
    event.state.reportId = reportId;
    event.state.modifiers = data[0];
    for (size_t index = 0; index < 6; ++index)
    {
      const uint8_t usage = data[index + 2];
      if (usage >= 0x04)
      {
        event.state.keys[usage >> 3] |= static_cast<uint8_t>(1u << (usage & 7));
      }
    }
    for (uint8_t bit = 0; bit < 8; ++bit)
    {
      if ((event.state.modifiers & static_cast<uint8_t>(1u << bit)) != 0)
      {
        const uint8_t usage = static_cast<uint8_t>(0xe0 + bit);
        event.state.keys[usage >> 3] |= static_cast<uint8_t>(1u << (usage & 7));
      }
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      Connection *connection = findConnection(connectionId);
      if (connection == nullptr)
      {
        return;
      }
      event.state.numLock = connection->numLock;
      event.state.capsLock = connection->capsLock;
      event.state.scrollLock = connection->scrollLock;
      event.state.compose = connection->compose;
      event.state.kana = connection->kana;
      bool changed = connection->modifiers != event.state.modifiers;
      for (size_t index = 0; index < EspBleHidKeyboardState::BitmapSize; ++index)
      {
        event.state.changedKeys[index] = connection->keys[index] ^ event.state.keys[index];
        changed = changed || event.state.changedKeys[index] != 0;
      }
      if (!changed)
      {
        return;
      }
      memcpy(connection->keys, event.state.keys, sizeof(connection->keys));
      connection->modifiers = event.state.modifiers;
      if (eventCount == QueueCapacity)
      {
        ++droppedEvents;
        return;
      }
      events[(eventHead + eventCount) % QueueCapacity] = event;
      ++eventCount;
    }
  }

  static void discoveryTaskEntry(void *argument)
  {
    EspBleHidKeyboardHostImpl *impl = static_cast<EspBleHidKeyboardHostImpl *>(argument);
    EspBleHidKeyboardHostDiscovery result;
    BLEClient *client = nullptr;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      result.connectionId = impl->discoveryConnectionId;
    }
    {
      std::lock_guard<std::mutex> lock(impl->host->owner_->impl_->mutex);
      for (const EspBleImpl::ConnectionSlot &slot : impl->host->owner_->impl_->connections)
      {
        if (slot.used && slot.connection.id == result.connectionId && slot.client != nullptr)
        {
          client = slot.client;
          break;
        }
      }
    }

    BLERemoteCharacteristic *inputReport = nullptr;
    BLERemoteCharacteristic *outputReports[8] = {};
    uint8_t outputReportIds[8] = {};
    size_t outputReportCount = 0;
    if (client == nullptr || !client->isConnected())
    {
      result.error = EspBleError::InvalidState;
      result.detail = "connection is not an active Central connection";
    }
    else
    {
      BLERemoteService *hidService = client->getService(BLEUUID((uint16_t)0x1812));
      if (hidService == nullptr)
      {
        result.error = EspBleError::NotFound;
        result.detail = "HID Service was not found";
      }
      else
      {
        BLERemoteCharacteristic *reportMap =
          hidService->getCharacteristic(BLEUUID((uint16_t)0x2a4b));
        const String reportMapValue = reportMap == nullptr ? String() : reportMap->readValue();
        if (reportMap == nullptr || !supportsSixKeyKeyboard(reportMapValue))
        {
          result.error = EspBleError::InvalidState;
          result.detail = "HID Report Map is not a supported 6-key keyboard";
        }
        else
        {
          BLERemoteCharacteristic *hidInformation =
            hidService->getCharacteristic(BLEUUID((uint16_t)0x2a4a));
          const String hidInformationValue =
            hidInformation == nullptr ? String() : hidInformation->readValue();
          if (hidInformationValue.length() >= 3)
          {
            result.hasCountryCode = true;
            result.countryCode = static_cast<uint8_t>(hidInformationValue[2]);
          }
          std::map<uint16_t, BLERemoteCharacteristic *> *characteristics =
            hidService->getCharacteristicsByHandle();
          for (const auto &entry : *characteristics)
          {
            BLERemoteCharacteristic *characteristic = entry.second;
            if (!characteristic->getUUID().equals(BLEUUID((uint16_t)0x2a4d)))
            {
              continue;
            }
            BLERemoteDescriptor *reference =
              characteristic->getDescriptor(BLEUUID((uint16_t)0x2908));
            const String value = reference == nullptr ? String() : reference->readValue();
            if (value.length() != 2)
            {
              continue;
            }
            const uint8_t reportId = static_cast<uint8_t>(value[0]);
            const uint8_t reportType = static_cast<uint8_t>(value[1]);
            if (reportType == 1 && characteristic->canNotify() && inputReport == nullptr)
            {
              inputReport = characteristic;
              result.reportId = reportId;
            }
            else if (reportType == 2 && outputReportCount < 8)
            {
              outputReports[outputReportCount] = characteristic;
              outputReportIds[outputReportCount++] = reportId;
            }
          }
          if (inputReport == nullptr)
          {
            result.error = EspBleError::NotFound;
            result.detail = "HID Keyboard Input Report was not found";
          }
          else
          {
            BLERemoteCharacteristic *outputReport = nullptr;
            for (size_t index = 0; index < outputReportCount; ++index)
            {
              if (outputReportIds[index] == result.reportId)
              {
                outputReport = outputReports[index];
                break;
              }
            }
            result.hasOutputReport = outputReport != nullptr;

            BLERemoteService *batteryService =
              client->getService(BLEUUID((uint16_t)0x180f));
            BLERemoteCharacteristic *battery = batteryService == nullptr
              ? nullptr
              : batteryService->getCharacteristic(BLEUUID((uint16_t)0x2a19));
            if (battery != nullptr && battery->canRead())
            {
              result.batteryLevel = battery->readUInt8();
              result.hasBatteryLevel = true;
            }

            {
              std::lock_guard<std::mutex> lock(impl->mutex);
              Connection *connection = impl->allocateConnection(result.connectionId);
              if (connection == nullptr)
              {
                result.error = EspBleError::ResourceExhausted;
                result.detail = "too many HID Keyboard Host connections";
              }
              else
              {
                connection->reportId = result.reportId;
                connection->inputReport = inputReport;
                connection->outputReport = outputReport;
              }
            }
            if (result.error == EspBleError::None)
            {
              const EspBleConnectionId connectionId = result.connectionId;
              const uint8_t reportId = result.reportId;
              result.success = inputReport->subscribe(
                true,
                [impl, connectionId, reportId](
                  BLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
                  impl->queueInputReport(connectionId, reportId, data, length);
                },
                true);
              if (!result.success)
              {
                result.error = EspBleError::BackendFailure;
                result.detail = "failed to subscribe to HID Keyboard Input Report";
                std::lock_guard<std::mutex> lock(impl->mutex);
                Connection *connection = impl->findConnection(result.connectionId);
                if (connection != nullptr)
                {
                  *connection = Connection();
                }
              }
            }
          }
        }
      }
    }

    Event event;
    event.type = EventType::Discovery;
    event.discovery = result;
    impl->pushEvent(event);
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->discovering = false;
      impl->discoveryTask = nullptr;
    }
    {
      std::lock_guard<std::mutex> lock(impl->host->owner_->impl_->mutex);
      impl->host->owner_->impl_->gattOperating = false;
    }
    vTaskDelete(nullptr);
  }

  EspBleHidKeyboardHost *host;
  mutable std::mutex mutex;
  Connection connections[ConnectionCapacity];
  Event events[QueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  bool discovering = false;
  EspBleConnectionId discoveryConnectionId = 0;
  TaskHandle_t discoveryTask = nullptr;
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
  appearance_ = 0;
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
  for (size_t index = 0; index < serviceUuidCount_; ++index)
  {
    if (uuidEquals(serviceUuids_[index], uuid))
    {
      owner_->clearError();
      return true;
    }
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

void EspBleAdvertising::setAppearance(uint16_t appearance)
{
  appearance_ = appearance;
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
  if (appearance_ != 0)
  {
    previousLength = advertisingData.getPayload().length();
    advertisingData.setAppearance(appearance_);
    if (advertisingData.getPayload().length() == previousLength)
    {
      owner_->setError(EspBleError::InvalidArgument, "appearance does not fit in legacy advertising payload");
      return false;
    }
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
  if (((config.encryptedRead || config.authenticatedRead) && !config.readable) ||
      ((config.encryptedWrite || config.authenticatedWrite) &&
       !config.writable && !config.writableWithoutResponse))
  {
    owner_->setError(
      EspBleError::InvalidArgument,
      "secured GATT access requires the corresponding read or write property");
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
      if (config.encryptedRead) properties |= BLECharacteristic::PROPERTY_READ_ENC;
      if (config.encryptedWrite) properties |= BLECharacteristic::PROPERTY_WRITE_ENC;
      if (config.authenticatedRead) properties |= BLECharacteristic::PROPERTY_READ_AUTHEN;
      if (config.authenticatedWrite) properties |= BLECharacteristic::PROPERTY_WRITE_AUTHEN;

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

EspBleHidKeyboardDevice::EspBleHidKeyboardDevice(EspBle *owner) : owner_(owner) {}

EspBleHidKeyboardDevice::~EspBleHidKeyboardDevice()
{
  delete impl_;
}

bool EspBleHidKeyboardDevice::configure(const EspBleHidKeyboardDeviceConfig &config)
{
  if (owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "HID Keyboard Device must be configured before begin");
    return false;
  }
  if (config.reportId == 0 || config.initialBatteryLevel > 100)
  {
    owner_->setError(
      EspBleError::InvalidArgument,
      "HID Keyboard report ID must be nonzero and battery level must be at most 100");
    return false;
  }
  if (impl_ == nullptr)
  {
    impl_ = new EspBleHidKeyboardDeviceImpl(this);
    if (impl_ == nullptr)
    {
      owner_->setError(EspBleError::ResourceExhausted, "failed to allocate HID Keyboard Device state");
      return false;
    }
  }

  const bool firstConfiguration = !impl_->configured;
  impl_->config = config;
  impl_->configured = true;
  if (firstConfiguration && !owner_->advertising().addServiceUuid("1812"))
  {
    impl_->configured = false;
    return false;
  }
  owner_->advertising().setAppearance(HidKeyboardAppearance);
  owner_->clearError();
  return true;
}

bool EspBleHidKeyboardDevice::realize()
{
  if (impl_ == nullptr || !impl_->configured || impl_->realized)
  {
    return true;
  }
  if (!owner_->preparePeripheral())
  {
    return false;
  }

  const uint8_t reportMap[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xa1, 0x01,       // Collection (Application)
    0x85, 0x01,       // Report ID
    0x05, 0x07,       // Usage Page (Keyboard)
    0x19, 0xe0,       // Usage Minimum (Left Control)
    0x29, 0xe7,       // Usage Maximum (Right GUI)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x75, 0x01,       // Report Size (1)
    0x95, 0x08,       // Report Count (8)
    0x81, 0x02,       // Input (Data, Variable, Absolute)
    0x95, 0x01,       // Report Count (1)
    0x75, 0x08,       // Report Size (8)
    0x81, 0x01,       // Input (Constant)
    0x95, 0x06,       // Report Count (6)
    0x75, 0x08,       // Report Size (8)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x65,       // Logical Maximum (101)
    0x05, 0x07,       // Usage Page (Keyboard)
    0x19, 0x00,       // Usage Minimum (0)
    0x29, 0x65,       // Usage Maximum (101)
    0x81, 0x00,       // Input (Data, Array)
    0x95, 0x05,       // Report Count (5)
    0x75, 0x01,       // Report Size (1)
    0x05, 0x08,       // Usage Page (LEDs)
    0x19, 0x01,       // Usage Minimum (Num Lock)
    0x29, 0x05,       // Usage Maximum (Kana)
    0x91, 0x02,       // Output (Data, Variable, Absolute)
    0x95, 0x01,       // Report Count (1)
    0x75, 0x03,       // Report Size (3)
    0x91, 0x01,       // Output (Constant)
    0xc0              // End Collection
  };
  memcpy(impl_->reportMap, reportMap, sizeof(reportMap));
  impl_->reportMap[7] = impl_->config.reportId;
  impl_->hidInformation[2] = impl_->config.countryCode;
  impl_->batteryLevel = impl_->config.initialBatteryLevel;
  impl_->pnpId[0] = 0x02; // USB Implementers Forum vendor ID source.
  impl_->pnpId[1] = static_cast<uint8_t>(impl_->config.vendorId);
  impl_->pnpId[2] = static_cast<uint8_t>(impl_->config.vendorId >> 8);
  impl_->pnpId[3] = static_cast<uint8_t>(impl_->config.productId);
  impl_->pnpId[4] = static_cast<uint8_t>(impl_->config.productId >> 8);
  impl_->pnpId[5] = static_cast<uint8_t>(impl_->config.productVersion);
  impl_->pnpId[6] = static_cast<uint8_t>(impl_->config.productVersion >> 8);

  impl_->inputDescriptors[0].uuid = &impl_->reportReferenceUuid.u;
  impl_->inputDescriptors[0].att_flags = BLE_ATT_F_READ;
  impl_->inputDescriptors[0].access_cb = EspBleHidKeyboardDeviceImpl::accessCallback;
  impl_->inputDescriptors[0].arg = impl_;
  impl_->outputDescriptors[0].uuid = &impl_->reportReferenceUuid.u;
  impl_->outputDescriptors[0].att_flags = BLE_ATT_F_READ;
  impl_->outputDescriptors[0].access_cb = EspBleHidKeyboardDeviceImpl::accessCallback;
  impl_->outputDescriptors[0].arg = impl_;

  impl_->hidCharacteristics[0].uuid = &impl_->hidInformationUuid.u;
  impl_->hidCharacteristics[0].access_cb = EspBleHidKeyboardDeviceImpl::accessCallback;
  impl_->hidCharacteristics[0].arg = impl_;
  impl_->hidCharacteristics[0].flags = BLE_GATT_CHR_F_READ;
  impl_->hidCharacteristics[1].uuid = &impl_->reportMapUuid.u;
  impl_->hidCharacteristics[1].access_cb = EspBleHidKeyboardDeviceImpl::accessCallback;
  impl_->hidCharacteristics[1].arg = impl_;
  impl_->hidCharacteristics[1].flags = BLE_GATT_CHR_F_READ;
  impl_->hidCharacteristics[2].uuid = &impl_->hidControlPointUuid.u;
  impl_->hidCharacteristics[2].access_cb = EspBleHidKeyboardDeviceImpl::accessCallback;
  impl_->hidCharacteristics[2].arg = impl_;
  impl_->hidCharacteristics[2].flags = BLE_GATT_CHR_F_WRITE_NO_RSP;
  impl_->hidCharacteristics[3].uuid = &impl_->reportUuid.u;
  impl_->hidCharacteristics[3].access_cb = EspBleHidKeyboardDeviceImpl::accessCallback;
  impl_->hidCharacteristics[3].arg = impl_;
  impl_->hidCharacteristics[3].descriptors = impl_->inputDescriptors;
  impl_->hidCharacteristics[3].flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY;
  impl_->hidCharacteristics[3].val_handle = &impl_->inputValueHandle;
  impl_->hidCharacteristics[4].uuid = &impl_->reportUuid.u;
  impl_->hidCharacteristics[4].access_cb = EspBleHidKeyboardDeviceImpl::accessCallback;
  impl_->hidCharacteristics[4].arg = impl_;
  impl_->hidCharacteristics[4].descriptors = impl_->outputDescriptors;
  impl_->hidCharacteristics[4].flags =
    BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP;
  impl_->hidCharacteristics[4].val_handle = &impl_->outputValueHandle;

  impl_->deviceInformationCharacteristics[0].uuid = &impl_->manufacturerUuid.u;
  impl_->deviceInformationCharacteristics[0].access_cb =
    EspBleHidKeyboardDeviceImpl::accessCallback;
  impl_->deviceInformationCharacteristics[0].arg = impl_;
  impl_->deviceInformationCharacteristics[0].flags = BLE_GATT_CHR_F_READ;
  impl_->deviceInformationCharacteristics[1].uuid = &impl_->pnpIdUuid.u;
  impl_->deviceInformationCharacteristics[1].access_cb =
    EspBleHidKeyboardDeviceImpl::accessCallback;
  impl_->deviceInformationCharacteristics[1].arg = impl_;
  impl_->deviceInformationCharacteristics[1].flags = BLE_GATT_CHR_F_READ;

  impl_->batteryCharacteristics[0].uuid = &impl_->batteryLevelUuid.u;
  impl_->batteryCharacteristics[0].access_cb = EspBleHidKeyboardDeviceImpl::accessCallback;
  impl_->batteryCharacteristics[0].arg = impl_;
  impl_->batteryCharacteristics[0].flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY;
  impl_->batteryCharacteristics[0].val_handle = &impl_->batteryValueHandle;

  impl_->services[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
  impl_->services[0].uuid = &impl_->hidServiceUuid.u;
  impl_->services[0].characteristics = impl_->hidCharacteristics;
  impl_->services[1].type = BLE_GATT_SVC_TYPE_PRIMARY;
  impl_->services[1].uuid = &impl_->deviceInformationServiceUuid.u;
  impl_->services[1].characteristics = impl_->deviceInformationCharacteristics;
  impl_->services[2].type = BLE_GATT_SVC_TYPE_PRIMARY;
  impl_->services[2].uuid = &impl_->batteryServiceUuid.u;
  impl_->services[2].characteristics = impl_->batteryCharacteristics;

  int backendCode = ble_gatts_count_cfg(impl_->services);
  if (backendCode == 0)
  {
    backendCode = ble_gatts_add_svcs(impl_->services);
  }
  if (backendCode != 0)
  {
    owner_->setError(
      EspBleError::BackendFailure,
      (String("failed to register HID services, backend code ") + backendCode).c_str());
    return false;
  }
  owner_->impl_->server->start();
  if (impl_->inputValueHandle == 0 || impl_->outputValueHandle == 0)
  {
    owner_->setError(EspBleError::BackendFailure, "HID report handles were not registered");
    return false;
  }
  impl_->realized = true;
  return true;
}

bool EspBleHidKeyboardDevice::sendInputReport(const EspBleHidKeyboardInputReport &report)
{
  if (!owner_->initialized() || impl_ == nullptr || !impl_->realized ||
      impl_->inputValueHandle == 0)
  {
    owner_->setError(EspBleError::InvalidState, "HID Keyboard Device is not initialized");
    return false;
  }

  uint16_t connectionHandles[ConnectionCapacity] = {};
  size_t connectionCount = 0;
  {
    std::lock_guard<std::mutex> lock(owner_->impl_->mutex);
    for (const EspBleImpl::ConnectionSlot &slot : owner_->impl_->connections)
    {
      if (slot.used && slot.connection.localRole == EspBleRole::Peripheral)
      {
        connectionHandles[connectionCount++] = slot.connection.handle;
      }
    }
  }
  if (connectionCount == 0)
  {
    owner_->setError(EspBleError::InvalidState, "no connected HID Host");
    return false;
  }

  impl_->inputValue[0] = report.modifiers;
  impl_->inputValue[1] = 0;
  memcpy(impl_->inputValue + 2, report.keys, sizeof(report.keys));

  bool sent = false;
  int lastBackendCode = 0;
  for (size_t index = 0; index < connectionCount; ++index)
  {
    os_mbuf *value = ble_hs_mbuf_from_flat(impl_->inputValue, sizeof(impl_->inputValue));
    if (value == nullptr)
    {
      lastBackendCode = BLE_HS_ENOMEM;
      continue;
    }
    const int backendCode = ble_gatts_notify_custom(
      connectionHandles[index], impl_->inputValueHandle, value);
    if (backendCode == 0)
    {
      sent = true;
    }
    else
    {
      lastBackendCode = backendCode;
    }
  }
  if (!sent)
  {
    owner_->setError(
      EspBleError::BackendFailure,
      (String("failed to notify HID input report, backend code ") + lastBackendCode).c_str());
    return false;
  }
  owner_->clearError();
  return true;
}

bool EspBleHidKeyboardDevice::releaseAll()
{
  return sendInputReport(EspBleHidKeyboardInputReport());
}

bool EspBleHidKeyboardDevice::setBatteryLevel(uint8_t level)
{
  if (level > 100)
  {
    owner_->setError(EspBleError::InvalidArgument, "battery level must be between 0 and 100");
    return false;
  }
  if (impl_ == nullptr || !impl_->configured)
  {
    owner_->setError(EspBleError::InvalidState, "HID Keyboard Device is not configured");
    return false;
  }
  impl_->config.initialBatteryLevel = level;
  impl_->batteryLevel = level;

  if (owner_->initialized() && impl_->realized && impl_->batteryValueHandle != 0)
  {
    uint16_t connectionHandles[ConnectionCapacity] = {};
    size_t connectionCount = 0;
    {
      std::lock_guard<std::mutex> lock(owner_->impl_->mutex);
      for (const EspBleImpl::ConnectionSlot &slot : owner_->impl_->connections)
      {
        if (slot.used && slot.connection.localRole == EspBleRole::Peripheral)
        {
          connectionHandles[connectionCount++] = slot.connection.handle;
        }
      }
    }
    for (size_t index = 0; index < connectionCount; ++index)
    {
      os_mbuf *value = ble_hs_mbuf_from_flat(&impl_->batteryLevel, 1);
      if (value != nullptr)
      {
        ble_gatts_notify_custom(connectionHandles[index], impl_->batteryValueHandle, value);
      }
    }
  }
  owner_->clearError();
  return true;
}

void EspBleHidKeyboardDevice::onOutputReport(OutputReportCallback callback)
{
  outputReportCallback_ = std::move(callback);
}

bool EspBleHidKeyboardDevice::configured() const
{
  return impl_ != nullptr && impl_->configured;
}

void EspBleHidKeyboardDevice::resetBackend()
{
  if (impl_ == nullptr)
  {
    return;
  }
  impl_->realized = false;
  impl_->inputValueHandle = 0;
  impl_->outputValueHandle = 0;
  impl_->batteryValueHandle = 0;
  memset(impl_->inputValue, 0, sizeof(impl_->inputValue));
  impl_->outputValue = 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->outputHead = 0;
  impl_->outputCount = 0;
}

void EspBleHidKeyboardDevice::dispatchPendingOutputReports()
{
  if (impl_ == nullptr || !outputReportCallback_)
  {
    return;
  }
  while (true)
  {
    EspBleHidKeyboardOutputReport report;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->outputCount == 0)
      {
        break;
      }
      report = impl_->outputReports[impl_->outputHead];
      impl_->outputHead = (impl_->outputHead + 1) % EspBleHidKeyboardDeviceImpl::OutputQueueCapacity;
      --impl_->outputCount;
    }
    outputReportCallback_(report);
  }
}

EspBleHidKeyboardHost::EspBleHidKeyboardHost(EspBle *owner) : owner_(owner) {}

EspBleHidKeyboardHost::~EspBleHidKeyboardHost()
{
  delete impl_;
}

bool EspBleHidKeyboardHost::discover(EspBleConnectionId connectionId)
{
  if (!owner_->initialized() || owner_->impl_ == nullptr || connectionId == 0)
  {
    owner_->setError(EspBleError::InvalidState, "BLE Central connection is not initialized");
    return false;
  }
  if (impl_ == nullptr)
  {
    impl_ = new EspBleHidKeyboardHostImpl(this);
    if (impl_ == nullptr)
    {
      owner_->setError(EspBleError::ResourceExhausted, "failed to allocate HID Keyboard Host state");
      return false;
    }
  }

  {
    std::lock_guard<std::mutex> ownerLock(owner_->impl_->mutex);
    bool found = false;
    for (const EspBleImpl::ConnectionSlot &slot : owner_->impl_->connections)
    {
      if (slot.used && slot.connection.id == connectionId && slot.client != nullptr)
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      owner_->setError(EspBleError::InvalidArgument, "Central connection ID was not found");
      return false;
    }
    if (owner_->impl_->gattOperating)
    {
      owner_->setError(EspBleError::InvalidState, "a GATT operation is already in progress");
      return false;
    }
    owner_->impl_->gattOperating = true;
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->discovering)
    {
      std::lock_guard<std::mutex> ownerLock(owner_->impl_->mutex);
      owner_->impl_->gattOperating = false;
      owner_->setError(EspBleError::InvalidState, "HID Keyboard discovery is already in progress");
      return false;
    }
    impl_->discovering = true;
    impl_->discoveryConnectionId = connectionId;
  }

  TaskHandle_t task = nullptr;
  const BaseType_t taskResult = xTaskCreate(
    EspBleHidKeyboardHostImpl::discoveryTaskEntry,
    "espble-hid-host",
    8192,
    impl_,
    1,
    &task);
  if (taskResult != pdPASS)
  {
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      impl_->discovering = false;
    }
    {
      std::lock_guard<std::mutex> ownerLock(owner_->impl_->mutex);
      owner_->impl_->gattOperating = false;
    }
    owner_->setError(EspBleError::ResourceExhausted, "failed to create HID discovery task");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->discoveryTask = task;
  }
  owner_->clearError();
  return true;
}

bool EspBleHidKeyboardHost::setKeyboardLeds(
  EspBleConnectionId connectionId,
  bool numLock,
  bool capsLock,
  bool scrollLock,
  bool compose,
  bool kana)
{
  if (!owner_->initialized() || owner_->impl_ == nullptr || impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidState, "HID Keyboard Host is not initialized");
    return false;
  }

  {
    std::lock_guard<std::mutex> ownerLock(owner_->impl_->mutex);
    if (owner_->impl_->gattOperating)
    {
      owner_->setError(EspBleError::InvalidState, "a GATT operation is already in progress");
      return false;
    }
    owner_->impl_->gattOperating = true;
  }

  BLERemoteCharacteristic *outputReport = nullptr;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    EspBleHidKeyboardHostImpl::Connection *connection = impl_->findConnection(connectionId);
    if (connection != nullptr)
    {
      outputReport = connection->outputReport;
    }
  }
  if (outputReport == nullptr)
  {
    std::lock_guard<std::mutex> ownerLock(owner_->impl_->mutex);
    owner_->impl_->gattOperating = false;
    owner_->setError(EspBleError::NotFound, "HID Keyboard Output Report was not found");
    return false;
  }

  uint8_t leds =
    (numLock ? 0x01 : 0) |
    (capsLock ? 0x02 : 0) |
    (scrollLock ? 0x04 : 0) |
    (compose ? 0x08 : 0) |
    (kana ? 0x10 : 0);
  const bool response = outputReport->canWrite();
  const bool success = outputReport->writeValue(
    &leds, 1, response);
  {
    std::lock_guard<std::mutex> ownerLock(owner_->impl_->mutex);
    owner_->impl_->gattOperating = false;
  }
  if (!success)
  {
    owner_->setError(EspBleError::BackendFailure, "failed to write HID Keyboard LEDs");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    EspBleHidKeyboardHostImpl::Connection *connection = impl_->findConnection(connectionId);
    if (connection != nullptr)
    {
      connection->numLock = numLock;
      connection->capsLock = capsLock;
      connection->scrollLock = scrollLock;
      connection->compose = compose;
      connection->kana = kana;
    }
  }
  owner_->clearError();
  return true;
}

void EspBleHidKeyboardHost::onDiscovered(DiscoveryCallback callback)
{
  discoveryCallback_ = std::move(callback);
}

void EspBleHidKeyboardHost::onKeyboardState(StateCallback callback)
{
  stateCallback_ = std::move(callback);
}

void EspBleHidKeyboardHost::onKeyboard(KeyboardCallback callback)
{
  keyboardCallback_ = std::move(callback);
}

template <typename Callback>
EspBleListenerId EspBleHidKeyboardHost::addListener(
  ListenerSlot<Callback> *slots,
  Callback callback)
{
  if (!callback)
  {
    owner_->setError(EspBleError::InvalidArgument, "listener callback is empty");
    return EspBleInvalidListenerId;
  }
  for (size_t i = 0; i < MaxListenersPerEvent; ++i)
  {
    if (slots[i].id == EspBleInvalidListenerId)
    {
      EspBleListenerId id = nextListenerId_++;
      if (id == EspBleInvalidListenerId)
      {
        id = nextListenerId_++;
      }
      slots[i].id = id;
      slots[i].callback = std::move(callback);
      owner_->clearError();
      return id;
    }
  }
  owner_->setError(EspBleError::ResourceExhausted, "too many HID Keyboard Host listeners");
  return EspBleInvalidListenerId;
}

template <typename Callback>
bool EspBleHidKeyboardHost::removeListenerFrom(
  ListenerSlot<Callback> *slots,
  EspBleListenerId listenerId)
{
  for (size_t i = 0; i < MaxListenersPerEvent; ++i)
  {
    if (slots[i].id == listenerId)
    {
      slots[i] = ListenerSlot<Callback>();
      return true;
    }
  }
  return false;
}

EspBleListenerId EspBleHidKeyboardHost::addDiscoveredListener(DiscoveryCallback callback)
{
  return addListener(discoveryListeners_, std::move(callback));
}

EspBleListenerId EspBleHidKeyboardHost::addKeyboardStateListener(StateCallback callback)
{
  return addListener(stateListeners_, std::move(callback));
}

EspBleListenerId EspBleHidKeyboardHost::addKeyboardListener(KeyboardCallback callback)
{
  return addListener(keyboardListeners_, std::move(callback));
}

bool EspBleHidKeyboardHost::removeListener(EspBleListenerId listenerId)
{
  if (listenerId == EspBleInvalidListenerId)
  {
    owner_->setError(EspBleError::InvalidArgument, "listener ID is invalid");
    return false;
  }
  const bool removed =
    removeListenerFrom(discoveryListeners_, listenerId) ||
    removeListenerFrom(stateListeners_, listenerId) ||
    removeListenerFrom(keyboardListeners_, listenerId);
  if (!removed)
  {
    owner_->setError(EspBleError::NotFound, "listener ID was not found");
    return false;
  }
  owner_->clearError();
  return true;
}

void EspBleHidKeyboardHost::setKeyboardLayout(EspBleKeyboardLayout layout)
{
  keyboardLayout_ = layout;
}

EspBleKeyboardLayout EspBleHidKeyboardHost::keyboardLayout() const
{
  return keyboardLayout_;
}

bool EspBleHidKeyboardHost::ready(EspBleConnectionId connectionId) const
{
  if (impl_ == nullptr)
  {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->findConnection(connectionId) != nullptr;
}

void EspBleHidKeyboardHost::handleDisconnected(EspBleConnectionId connectionId)
{
  if (impl_ == nullptr)
  {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  EspBleHidKeyboardHostImpl::Connection *connection = impl_->findConnection(connectionId);
  if (connection == nullptr)
  {
    return;
  }

  EspBleHidKeyboardHostImpl::Event event;
  event.type = EspBleHidKeyboardHostImpl::EventType::State;
  event.state.connectionId = connectionId;
  event.state.reportId = connection->reportId;
  event.state.numLock = connection->numLock;
  event.state.capsLock = connection->capsLock;
  event.state.scrollLock = connection->scrollLock;
  event.state.compose = connection->compose;
  event.state.kana = connection->kana;
  memcpy(event.state.changedKeys, connection->keys, sizeof(event.state.changedKeys));
  bool hasHeldKey = connection->modifiers != 0;
  for (uint8_t value : connection->keys)
  {
    hasHeldKey = hasHeldKey || value != 0;
  }
  *connection = EspBleHidKeyboardHostImpl::Connection();
  if (hasHeldKey)
  {
    if (impl_->eventCount == EspBleHidKeyboardHostImpl::QueueCapacity)
    {
      ++impl_->droppedEvents;
    }
    else
    {
      impl_->events[(impl_->eventHead + impl_->eventCount) %
        EspBleHidKeyboardHostImpl::QueueCapacity] = event;
      ++impl_->eventCount;
    }
  }
}

void EspBleHidKeyboardHost::resetBackend()
{
  if (impl_ == nullptr)
  {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  for (EspBleHidKeyboardHostImpl::Connection &connection : impl_->connections)
  {
    connection = EspBleHidKeyboardHostImpl::Connection();
  }
  impl_->eventHead = 0;
  impl_->eventCount = 0;
  impl_->discovering = false;
  impl_->discoveryTask = nullptr;
}

void EspBleHidKeyboardHost::dispatchPendingEvents()
{
  if (impl_ == nullptr)
  {
    return;
  }
  while (true)
  {
    EspBleHidKeyboardHostImpl::Event event;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->eventCount == 0)
      {
        break;
      }
      event = std::move(impl_->events[impl_->eventHead]);
      impl_->eventHead =
        (impl_->eventHead + 1) % EspBleHidKeyboardHostImpl::QueueCapacity;
      --impl_->eventCount;
    }
    if (event.type == EspBleHidKeyboardHostImpl::EventType::Discovery)
    {
      DiscoveryCallback callbacks[MaxListenersPerEvent + 1];
      callbacks[0] = discoveryCallback_;
      for (size_t i = 0; i < MaxListenersPerEvent; ++i)
      {
        callbacks[i + 1] = discoveryListeners_[i].callback;
      }
      for (DiscoveryCallback &callback : callbacks)
      {
        if (callback)
        {
          callback(event.discovery);
        }
      }
    }
    else
    {
      StateCallback stateCallbacks[MaxListenersPerEvent + 1];
      stateCallbacks[0] = stateCallback_;
      for (size_t i = 0; i < MaxListenersPerEvent; ++i)
      {
        stateCallbacks[i + 1] = stateListeners_[i].callback;
      }
      for (StateCallback &callback : stateCallbacks)
      {
        if (callback)
        {
          callback(event.state);
        }
      }

      KeyboardCallback keyboardCallbacks[MaxListenersPerEvent + 1];
      keyboardCallbacks[0] = keyboardCallback_;
      bool hasKeyboardCallback = static_cast<bool>(keyboardCallback_);
      for (size_t i = 0; i < MaxListenersPerEvent; ++i)
      {
        keyboardCallbacks[i + 1] = keyboardListeners_[i].callback;
        hasKeyboardCallback = hasKeyboardCallback ||
          static_cast<bool>(keyboardListeners_[i].callback);
      }
      if (hasKeyboardCallback)
      {
        uint8_t previousModifiers = event.state.modifiers;
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
          const uint8_t usage = static_cast<uint8_t>(0xe0 + bit);
          if ((event.state.changedKeys[usage >> 3] &
               static_cast<uint8_t>(1u << (usage & 7))) != 0)
          {
            previousModifiers ^= static_cast<uint8_t>(1u << bit);
          }
        }
        for (uint8_t transition = 0; transition < 2; ++transition)
        {
          const bool pressed = transition == 0;
          for (uint16_t usageValue = 0; usageValue <= 0xff; ++usageValue)
          {
            const uint8_t usage = static_cast<uint8_t>(usageValue);
            const uint8_t mask = static_cast<uint8_t>(1u << (usage & 7));
            if ((event.state.changedKeys[usage >> 3] & mask) == 0 ||
                event.state.isDown(usage) != pressed)
            {
              continue;
            }
            EspBleHidKeyboardEvent keyboardEvent;
            keyboardEvent.connectionId = event.state.connectionId;
            keyboardEvent.reportId = event.state.reportId;
            keyboardEvent.usage = usage;
            keyboardEvent.modifiers = event.state.modifiers;
            keyboardEvent.pressed = pressed;
            keyboardEvent.released = !pressed;
            keyboardEvent.numLock = event.state.numLock;
            keyboardEvent.capsLock = event.state.capsLock;
            keyboardEvent.scrollLock = event.state.scrollLock;
            keyboardEvent.compose = event.state.compose;
            keyboardEvent.kana = event.state.kana;
            keyboardEvent.ascii = EspBleHidKeyboardHostImpl::usageToAscii(
              usage,
              pressed ? event.state.modifiers : previousModifiers,
              keyboardLayout_,
              event.state.capsLock,
              event.state.numLock);
            for (KeyboardCallback &callback : keyboardCallbacks)
            {
              if (callback)
              {
                callback(keyboardEvent);
              }
            }
          }
        }
      }
    }
  }
}

EspBle::EspBle()
    : advertising_(this), scanner_(this), gattServer_(this), hidKeyboardDevice_(this),
      hidKeyboardHost_(this)
{
}

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
  if (!config.security.enabled &&
      (config.security.mitm || config.security.staticPasskeyEnabled ||
       config.security.ioCapability != EspBleSecurityIoCapability::None))
  {
    setError(EspBleError::InvalidArgument, "enable BLE security before configuring MITM or a passkey");
    return false;
  }
  if (config.security.ioCapability != EspBleSecurityIoCapability::None &&
      config.security.ioCapability != EspBleSecurityIoCapability::DisplayOnly &&
      config.security.ioCapability != EspBleSecurityIoCapability::KeyboardOnly)
  {
    setError(EspBleError::InvalidArgument, "unsupported BLE Security I/O capability");
    return false;
  }
  if (config.security.staticPasskeyEnabled && config.security.staticPasskey > 999999)
  {
    setError(EspBleError::InvalidArgument, "static BLE passkey must be between 000000 and 999999");
    return false;
  }
  if (config.security.mitm &&
      (!config.security.staticPasskeyEnabled ||
       config.security.ioCapability == EspBleSecurityIoCapability::None))
  {
    setError(
      EspBleError::InvalidArgument,
      "MITM currently requires a static passkey and DisplayOnly or KeyboardOnly capability");
    return false;
  }
  if (!config.security.mitm &&
      (config.security.staticPasskeyEnabled ||
       config.security.ioCapability != EspBleSecurityIoCapability::None))
  {
    setError(EspBleError::InvalidArgument, "a static passkey and I/O capability require MITM");
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

  impl_->securityEnabled = config.security.enabled;
  if (config.security.enabled)
  {
    impl_->securityBackend = new BLESecurity();
    if (impl_->securityBackend == nullptr)
    {
      BLESecurity::setAuthenticationMode(false, false, false);
      BLESecurity::setForceAuthentication(false);
      BLEDevice::deinit(false);
      delete impl_;
      impl_ = nullptr;
      setError(EspBleError::ResourceExhausted, "failed to allocate security state");
      return false;
    }
    uint8_t ioCapability = ESP_IO_CAP_NONE;
    if (config.security.ioCapability == EspBleSecurityIoCapability::DisplayOnly)
    {
      ioCapability = ESP_IO_CAP_OUT;
    }
    else if (config.security.ioCapability == EspBleSecurityIoCapability::KeyboardOnly)
    {
      ioCapability = ESP_IO_CAP_IN;
    }
    BLESecurity::setCapability(ioCapability);
    if (config.security.staticPasskeyEnabled)
    {
      BLESecurity::setPassKey(true, config.security.staticPasskey);
    }
    BLESecurity::setAuthenticationMode(
      config.security.bonding,
      config.security.mitm,
      true);
    BLESecurity::setForceAuthentication(config.security.pairOnConnect);
    BLEDevice::setSecurityCallbacks(&impl_->securityCallbacks);
  }
  else
  {
    BLESecurity::setAuthenticationMode(false, false, false);
    BLESecurity::setForceAuthentication(false);
    BLEDevice::setSecurityCallbacks(nullptr);
  }

  if (!gattServer_.realize())
  {
    BLEDevice::setSecurityCallbacks(nullptr);
    BLESecurity::setAuthenticationMode(false, false, false);
    BLESecurity::setForceAuthentication(false);
    BLEDevice::deinit(false);
    gattServer_.resetBackend();
    delete impl_;
    impl_ = nullptr;
    return false;
  }
  if (!hidKeyboardDevice_.realize())
  {
    BLEDevice::setSecurityCallbacks(nullptr);
    BLESecurity::setAuthenticationMode(false, false, false);
    BLESecurity::setForceAuthentication(false);
    hidKeyboardDevice_.resetBackend();
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
  BLEDevice::setSecurityCallbacks(nullptr);
  BLESecurity::setAuthenticationMode(false, false, false);
  BLESecurity::setForceAuthentication(false);
  hidKeyboardDevice_.resetBackend();
  hidKeyboardHost_.resetBackend();
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
  hidKeyboardDevice_.dispatchPendingOutputReports();
  hidKeyboardHost_.dispatchPendingEvents();
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

bool EspBle::requestSecurity(EspBleConnectionId connectionId)
{
  if (!initialized_ || impl_ == nullptr)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (!impl_->securityEnabled)
  {
    setError(EspBleError::InvalidState, "BLE security is not enabled");
    return false;
  }

  uint16_t connectionHandle = 0xffff;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (const EspBleImpl::ConnectionSlot &slot : impl_->connections)
    {
      if (slot.used && slot.connection.id == connectionId)
      {
        connectionHandle = slot.connection.handle;
        break;
      }
    }
  }
  if (connectionHandle == 0xffff)
  {
    setError(EspBleError::InvalidArgument, "connection ID was not found");
    return false;
  }

  int backendCode = 0;
  if (!BLESecurity::startSecurity(connectionHandle, &backendCode))
  {
    setError(
      EspBleError::BackendFailure,
      (String("failed to request BLE security, backend code ") + backendCode).c_str());
    return false;
  }
  clearError();
  return true;
}

size_t EspBle::bondCount() const
{
  if (!initialized_)
  {
    return 0;
  }
  ble_addr_t peers[BondCapacity];
  int count = 0;
  return ble_store_util_bonded_peers(peers, &count, BondCapacity) == 0 && count > 0
    ? static_cast<size_t>(count)
    : 0;
}

bool EspBle::bond(size_t index, EspBleBond &bond) const
{
  if (!initialized_)
  {
    return false;
  }
  ble_addr_t peers[BondCapacity];
  int count = 0;
  if (ble_store_util_bonded_peers(peers, &count, BondCapacity) != 0 ||
      index >= static_cast<size_t>(count))
  {
    return false;
  }
  const BLEAddress address(peers[index]);
  bond.peerAddress = address.toString();
  bond.peerAddressType = address.getType();
  return true;
}

bool EspBle::deleteBond(const EspBleBond &bond)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (connectionCount() != 0)
  {
    setError(EspBleError::InvalidState, "disconnect before deleting a BLE bond");
    return false;
  }

  ble_addr_t peers[BondCapacity];
  int count = 0;
  if (ble_store_util_bonded_peers(peers, &count, BondCapacity) != 0)
  {
    setError(EspBleError::BackendFailure, "failed to enumerate BLE bonds");
    return false;
  }
  for (int index = 0; index < count; ++index)
  {
    const BLEAddress address(peers[index]);
    if (address.getType() == bond.peerAddressType &&
        address.toString().equalsIgnoreCase(bond.peerAddress))
    {
      if (ble_store_util_delete_peer(&peers[index]) != 0)
      {
        setError(EspBleError::BackendFailure, "failed to delete BLE bond");
        return false;
      }
      clearError();
      return true;
    }
  }
  setError(EspBleError::NotFound, "BLE bond was not found");
  return false;
}

bool EspBle::deleteAllBonds()
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (connectionCount() != 0)
  {
    setError(EspBleError::InvalidState, "disconnect before deleting BLE bonds");
    return false;
  }

  ble_addr_t peers[BondCapacity];
  int count = 0;
  if (ble_store_util_bonded_peers(peers, &count, BondCapacity) != 0)
  {
    setError(EspBleError::BackendFailure, "failed to enumerate BLE bonds");
    return false;
  }
  for (int index = 0; index < count; ++index)
  {
    if (ble_store_util_delete_peer(&peers[index]) != 0)
    {
      setError(EspBleError::BackendFailure, "failed to delete all BLE bonds");
      return false;
    }
  }
  clearError();
  return true;
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

void EspBle::onSecurityChanged(SecurityChangedCallback callback)
{
  securityChangedCallback_ = std::move(callback);
}

void EspBle::onPasskeyDisplayed(PasskeyDisplayedCallback callback)
{
  passkeyDisplayedCallback_ = std::move(callback);
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
      hidKeyboardHost_.handleDisconnected(event.connection.id);
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
    case EspBleImpl::EventType::SecurityChanged:
      if (securityChangedCallback_)
      {
        securityChangedCallback_(event.securityChanged);
      }
      break;
    case EspBleImpl::EventType::PasskeyDisplayed:
      if (passkeyDisplayedCallback_)
      {
        passkeyDisplayedCallback_(event.passkeyDisplayed);
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

EspBleHidKeyboardDevice &EspBle::hidKeyboardDevice()
{
  return hidKeyboardDevice_;
}

EspBleHidKeyboardHost &EspBle::hidKeyboardHost()
{
  return hidKeyboardHost_;
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
