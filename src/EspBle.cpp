#include "EspBle.h"

#include <BLEAdvertising.h>
#include <BLECharacteristic.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLEDescriptor.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteDescriptor.h>
#include <BLERemoteService.h>
#include <BLEScan.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <BLESecurity.h>
#include <BLEUtils.h>
#include <host/ble_gap.h>
#include <host/ble_hs_mbuf.h>
#include <host/ble_store.h>
#include <os/os_mbuf.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cctype>
#include <cstring>
#include <map>
#include <mutex>
#include <new>
#include <utility>

#include "EspBleHidReportMap.h"


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

bool isValidBleAddress(const char *address)
{
  if (address == nullptr || strlen(address) != 17) return false;
  for (size_t index = 0; index < 17; ++index)
  {
    if ((index + 1) % 3 == 0)
    {
      if (address[index] != ':') return false;
    }
    else if (!isxdigit(static_cast<unsigned char>(address[index])))
    {
      return false;
    }
  }
  return true;
}

bool isValidAddressType(EspBleAddressType type)
{
  return static_cast<uint8_t>(type) <=
    static_cast<uint8_t>(EspBleAddressType::RandomIdentity);
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
    ServerDescriptorWrite,
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

  // Central clients waiting to be freed on the loop task. The backend has no
  // deleteClient() API and BLEDevice::deinit() frees only the most recently
  // created client, so EspBle owns the lifetime of every other client.
  struct RetiredClient
  {
    BLEClient *client = nullptr;
    EspBleConnectionId connectionId = 0;
  };

  static constexpr size_t RetiredClientCapacity = ConnectionCapacity * 2;

  struct GattDatabaseSnapshot
  {
    EspBleConnectionId connectionId = 0;
    bool valid = false;
    EspBleGattServiceInfo services[EspBle::MaxDiscoveredGattServices];
    size_t serviceCount = 0;
    EspBleGattCharacteristicInfo
      characteristics[EspBle::MaxDiscoveredGattCharacteristics];
    size_t characteristicCount = 0;
    EspBleGattDescriptorInfo descriptors[EspBle::MaxDiscoveredGattDescriptors];
    size_t descriptorCount = 0;

    void reset(EspBleConnectionId newConnectionId = 0)
    {
      connectionId = newConnectionId;
      valid = false;
      serviceCount = 0;
      characteristicCount = 0;
      descriptorCount = 0;
    }
  };

  struct Event
  {
    EventType type = EventType::Connected;
    EspBleConnection connection;
    EspBleConnectionFailure failure;
    EspBleGattResult gattResult;
    EspBleGattWrite serverWrite;
    String serverDescriptorUuid;
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
            static_cast<EspBleAddressType>(peerAddress.getType()),
            EspBleRole::Central,
            client->getMTU(),
            description.sec_state.encrypted,
            description.sec_state.authenticated,
            description.sec_state.bonded,
            description.sec_state.key_size,
            client))
      {
        client->disconnect();
        // Never added to a slot, so onDisconnect will not retire it.
        owner_->retireClient(client, 0);
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
            static_cast<EspBleAddressType>(peerAddress.getType()),
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
    delete gattDatabase;
    delete securityBackend;
  }

  static bool isEvictableEvent(EventType type)
  {
    return type == EventType::Notification;
  }

  bool pushEvent(const Event &event)
  {
    if (eventCount == ConnectionEventQueueCapacity)
    {
      if (!isEvictableEvent(event.type))
      {
        // Lifecycle and completion events must not be lost; evict the oldest
        // notification instead of dropping the new event.
        for (size_t offset = 0; offset < eventCount; ++offset)
        {
          if (!isEvictableEvent(
                events[(eventHead + offset) % ConnectionEventQueueCapacity].type))
          {
            continue;
          }
          for (size_t next = offset; next + 1 < eventCount; ++next)
          {
            events[(eventHead + next) % ConnectionEventQueueCapacity] = std::move(
              events[(eventHead + next + 1) % ConnectionEventQueueCapacity]);
          }
          --eventCount;
          ++droppedEvents;
          const size_t tail = (eventHead + eventCount) % ConnectionEventQueueCapacity;
          events[tail] = event;
          ++eventCount;
          return true;
        }
      }
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
    EspBleAddressType peerAddressType,
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

  void retireClientLocked(BLEClient *client, EspBleConnectionId connectionId)
  {
    if (client == nullptr)
    {
      return;
    }
    for (size_t index = 0; index < retiredClientCount; ++index)
    {
      if (retiredClients[index].client == client)
      {
        return;
      }
    }
    if (retiredClientCount < RetiredClientCapacity)
    {
      retiredClients[retiredClientCount].client = client;
      retiredClients[retiredClientCount].connectionId = connectionId;
      ++retiredClientCount;
    }
  }

  void retireClient(BLEClient *client, EspBleConnectionId connectionId)
  {
    std::lock_guard<std::mutex> lock(mutex);
    retireClientLocked(client, connectionId);
  }

  void removeClientConnection(BLEClient *client)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.client == client)
      {
        retireClientLocked(slot.client, slot.connection.id);
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
    if (gattDatabase != nullptr && gattDatabase->connectionId == slot.connection.id)
    {
      gattDatabase->reset();
    }
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
    if (event.serverWrite.connectionId == 0)
    {
      // 0 is not a valid connection ID; drop instead of delivering an event
      // that cannot be attributed to a connection.
      ++droppedEvents;
      return;
    }
    pushEvent(event);
  }

  void queueServerDescriptorWrite(
    const String &serviceUuid,
    const String &characteristicUuid,
    const String &descriptorUuid,
    const String &value)
  {
    std::lock_guard<std::mutex> lock(mutex);
    Event event;
    event.type = EventType::ServerDescriptorWrite;
    event.serverWrite.serviceUuid = serviceUuid;
    event.serverWrite.characteristicUuid = characteristicUuid;
    event.serverWrite.value = value;
    event.serverDescriptorUuid = descriptorUuid;
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
    if (event.serverSubscription.connectionId == 0)
    {
      ++droppedEvents;
      return;
    }
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
      {
        // BLEDevice::deinit() frees the most recently created client, so it
        // must never also be freed by EspBle.
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->newestClient = client;
      }
      client->setClientCallbacks(&impl->clientCallbacks);
      connected = client->connect(
        BLEAddress(target.address, static_cast<uint8_t>(target.addressType)),
        static_cast<uint8_t>(target.addressType),
        timeoutMilliseconds);
    }
    if (!connected)
    {
      impl->pushFailure(target, client == nullptr ? "failed to create BLE client" : "BLE connection failed");
      impl->retireClient(client, 0);
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
      result.descriptorUuid = impl->gattDescriptorUuid;
      writeValue = impl->gattWriteValue;
      response = impl->gattWriteResponse;
      result.response = response;
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
    else if (result.operation == EspBleGattOperation::DiscoverServices)
    {
      GattDatabaseSnapshot *database = nullptr;
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (impl->gattDatabase == nullptr)
        {
          impl->gattDatabase = new (std::nothrow) GattDatabaseSnapshot();
        }
        database = impl->gattDatabase;
        if (database != nullptr) database->reset(result.connectionId);
      }
      if (database == nullptr)
      {
        result.error = EspBleError::ResourceExhausted;
        result.detail = "failed to allocate the GATT database snapshot";
      }
      else
      {
        std::map<std::string, BLERemoteService *> *services = client->getServices();
        if (services == nullptr)
        {
          result.error = EspBleError::BackendFailure;
          result.detail = "failed to enumerate GATT services";
        }
        else result.success = true;
        if (services != nullptr)
        for (const auto &serviceItem : *services)
        {
          BLERemoteService *service = serviceItem.second;
          if (service == nullptr) continue;
          const String serviceUuid = service->getUUID().toString();
          {
            std::lock_guard<std::mutex> lock(impl->mutex);
            if (database->serviceCount == EspBle::MaxDiscoveredGattServices)
            {
              result.success = false;
              result.error = EspBleError::ResourceExhausted;
              result.detail = "too many discovered GATT services";
            }
            else
            {
              EspBleGattServiceInfo &info = database->services[database->serviceCount++];
              info.serviceUuid = serviceUuid;
              info.handle = service->getHandle();
            }
          }
          if (!result.success) break;

          std::map<std::string, BLERemoteCharacteristic *> *characteristics =
            service->getCharacteristics();
          if (characteristics == nullptr)
          {
            result.success = false;
            result.error = EspBleError::BackendFailure;
            result.detail = "failed to enumerate GATT characteristics";
            break;
          }
          for (const auto &characteristicItem : *characteristics)
          {
            BLERemoteCharacteristic *characteristic = characteristicItem.second;
            if (characteristic == nullptr) continue;
            const String characteristicUuid = characteristic->getUUID().toString();
            {
              std::lock_guard<std::mutex> lock(impl->mutex);
              if (database->characteristicCount == EspBle::MaxDiscoveredGattCharacteristics)
              {
                result.success = false;
                result.error = EspBleError::ResourceExhausted;
                result.detail = "too many discovered GATT characteristics";
              }
              else
              {
                EspBleGattCharacteristicInfo &info =
                  database->characteristics[database->characteristicCount++];
                info.serviceUuid = serviceUuid;
                info.characteristicUuid = characteristicUuid;
                info.handle = characteristic->getHandle();
                info.readable = characteristic->canRead();
                info.writable = characteristic->canWrite();
                info.writableWithoutResponse = characteristic->canWriteNoResponse();
                info.notifiable = characteristic->canNotify();
                info.indicatable = characteristic->canIndicate();
              }
            }
            if (!result.success) break;

            std::map<std::string, BLERemoteDescriptor *> *descriptors =
              characteristic->getDescriptors();
            if (descriptors == nullptr) continue;
            for (const auto &descriptorItem : *descriptors)
            {
              BLERemoteDescriptor *descriptor = descriptorItem.second;
              if (descriptor == nullptr) continue;
              std::lock_guard<std::mutex> lock(impl->mutex);
              if (database->descriptorCount == EspBle::MaxDiscoveredGattDescriptors)
              {
                result.success = false;
                result.error = EspBleError::ResourceExhausted;
                result.detail = "too many discovered GATT descriptors";
                break;
              }
              EspBleGattDescriptorInfo &info =
                database->descriptors[database->descriptorCount++];
              info.serviceUuid = serviceUuid;
              info.characteristicUuid = characteristicUuid;
              info.descriptorUuid = descriptor->getUUID().toString();
              info.handle = descriptor->getHandle();
            }
            if (!result.success) break;
          }
          if (!result.success) break;
        }
      }
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
          else if (result.operation == EspBleGattOperation::ReadDescriptor ||
                   result.operation == EspBleGattOperation::WriteDescriptor)
          {
            BLERemoteDescriptor *descriptor =
              characteristic->getDescriptor(BLEUUID(result.descriptorUuid.c_str()));
            if (descriptor == nullptr)
            {
              result.error = EspBleError::NotFound;
              result.detail = "GATT descriptor was not found";
            }
            else if (result.operation == EspBleGattOperation::ReadDescriptor)
            {
              result.value = descriptor->readValue();
              result.success = true;
            }
            else
            {
              result.success = descriptor->writeValue(
                reinterpret_cast<uint8_t *>(const_cast<char *>(writeValue.c_str())),
                writeValue.length(),
                response);
              if (!result.success)
              {
                result.error = EspBleError::BackendFailure;
                result.detail = "GATT descriptor write failed";
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
      // Publish and clear atomically so update() cannot enqueue a timeout for
      // an operation that already completed. A late backend completion after
      // a timeout is deliberately suppressed.
      if (!impl->gattTimedOut)
      {
        if (result.operation == EspBleGattOperation::DiscoverServices &&
            impl->gattDatabase != nullptr &&
            impl->gattDatabase->connectionId == result.connectionId)
        {
          impl->gattDatabase->valid = result.success;
        }
        Event event;
        event.type = EventType::GattResult;
        event.gattResult = result;
        impl->pushEvent(event);
      }
      else if (result.operation == EspBleGattOperation::DiscoverServices &&
               impl->gattDatabase != nullptr &&
               impl->gattDatabase->connectionId == result.connectionId)
      {
        impl->gattDatabase->reset();
      }
      impl->gattOperating = false;
      impl->gattTask = nullptr;
    }
    vTaskDelete(nullptr);
  }

  EspBle *owner;
  mutable std::mutex mutex;
  ConnectionSlot connections[ConnectionCapacity];
  RetiredClient retiredClients[RetiredClientCapacity];
  size_t retiredClientCount = 0;
  BLEClient *newestClient = nullptr;
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
  uint32_t connectStartMilliseconds = 0;
  bool connectCancelRequested = false;
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
  String gattDescriptorUuid;
  String gattWriteValue;
  bool gattWriteResponse = true;
  uint32_t gattStartMilliseconds = 0;
  uint32_t gattTimeoutMilliseconds = 10000;
  bool gattTimedOut = false;
  GattDatabaseSnapshot *gattDatabase = nullptr;
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

  struct DescriptorDefinition
  {
    String serviceUuid;
    String characteristicUuid;
    String uuid;
    EspBleGattDescriptorConfig config;
    String value;
    BLEDescriptor *backend = nullptr;
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

  class DescriptorCallbacks : public BLEDescriptorCallbacks
  {
  public:
    explicit DescriptorCallbacks(EspBleGattServerImpl *owner) : owner_(owner) {}

    void onWrite(BLEDescriptor *descriptor) override
    {
      String serviceUuid;
      String characteristicUuid;
      String descriptorUuid;
      String value;
      {
        std::lock_guard<std::mutex> lock(owner_->mutex);
        for (DescriptorDefinition &definition : owner_->descriptors)
        {
          if (definition.backend == descriptor)
          {
            const uint8_t *data = descriptor->getValue();
            definition.value = descriptor->getLength() == 0
              ? String()
              : String(reinterpret_cast<const char *>(data), descriptor->getLength());
            serviceUuid = definition.serviceUuid;
            characteristicUuid = definition.characteristicUuid;
            descriptorUuid = definition.uuid;
            value = definition.value;
            break;
          }
        }
      }
      if (!descriptorUuid.isEmpty() && owner_->server->owner_->impl_ != nullptr)
      {
        owner_->server->owner_->impl_->queueServerDescriptorWrite(
          serviceUuid, characteristicUuid, descriptorUuid, value);
      }
    }

  private:
    EspBleGattServerImpl *owner_;
  };

  explicit EspBleGattServerImpl(EspBleGattServer *server)
      : server(server), callbacks(this), descriptorCallbacks(this)
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
    // Queue the result before clearing the busy flag: end() tears the stack
    // down as soon as sending is observed false.
    if (impl->server->owner_->impl_ != nullptr)
    {
      impl->server->owner_->impl_->queueServerSendResult(result);
    }
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->sending = false;
      impl->sendTask = nullptr;
    }
    vTaskDelete(nullptr);
  }

  EspBleGattServer *server;
  mutable std::mutex mutex;
  ServiceDefinition services[EspBleGattServer::MaxServices];
  size_t serviceCount = 0;
  CharacteristicDefinition characteristics[EspBleGattServer::MaxCharacteristics];
  size_t characteristicCount = 0;
  DescriptorDefinition descriptors[EspBleGattServer::MaxDescriptors];
  size_t descriptorCount = 0;
  BackendCallbacks callbacks;
  DescriptorCallbacks descriptorCallbacks;
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

struct EspBleHidDeviceManagerImpl
{
  static constexpr size_t OutputQueueCapacity = 8;
  static constexpr size_t ProfileCount = 6;
  static constexpr size_t MaxVendorReportSize = 64;

  struct VendorReportEntry
  {
    EspBleConnectionId connectionId = 0;
    uint8_t reportType = 0;
    uint8_t data[MaxVendorReportSize] = {};
    size_t length = 0;
  };

  // CCCD subscription state per connection, tracked from GAP subscribe
  // events. Reports are only notified to subscribed peers.
  struct SubscriptionSlot
  {
    bool used = false;
    uint16_t connectionHandle = 0xffff;
    uint8_t inputNotifications = 0;
    bool batteryNotifications = false;
  };

  explicit EspBleHidDeviceManagerImpl(EspBleHidKeyboard *device)
      : device(device)
  {
  }

  static int gapListenerEntry(ble_gap_event *event, void *argument)
  {
    EspBleHidDeviceManagerImpl *impl = static_cast<EspBleHidDeviceManagerImpl *>(argument);
    if (event->type == BLE_GAP_EVENT_SUBSCRIBE)
    {
      impl->handleSubscribe(
        event->subscribe.conn_handle,
        event->subscribe.attr_handle,
        event->subscribe.cur_notify != 0);
    }
    else if (event->type == BLE_GAP_EVENT_DISCONNECT)
    {
      impl->clearSubscriptions(event->disconnect.conn.conn_handle);
    }
    return 0;
  }

  void handleSubscribe(uint16_t connectionHandle, uint16_t attributeHandle, bool notifications)
  {
    uint8_t reportId = 0;
    for (uint8_t index = 0; index < ProfileCount; ++index)
    {
      if (inputValueHandles[index] != 0 && attributeHandle == inputValueHandles[index])
      {
        reportId = index + 1;
        break;
      }
    }
    if (reportId == 0 && (attributeHandle != batteryValueHandle || batteryValueHandle == 0))
    {
      return;
    }
    std::lock_guard<std::mutex> lock(mutex);
    SubscriptionSlot *slot = nullptr;
    for (SubscriptionSlot &candidate : subscriptions)
    {
      if (candidate.used && candidate.connectionHandle == connectionHandle)
      {
        slot = &candidate;
        break;
      }
    }
    if (slot == nullptr)
    {
      for (SubscriptionSlot &candidate : subscriptions)
      {
        if (!candidate.used)
        {
          candidate.used = true;
          candidate.connectionHandle = connectionHandle;
          candidate.inputNotifications = 0;
          candidate.batteryNotifications = false;
          slot = &candidate;
          break;
        }
      }
    }
    if (slot == nullptr)
    {
      return;
    }
    if (reportId != 0)
    {
      const uint8_t mask = static_cast<uint8_t>(1u << (reportId - 1));
      if (notifications) slot->inputNotifications |= mask;
      else slot->inputNotifications &= static_cast<uint8_t>(~mask);
    }
    else
    {
      slot->batteryNotifications = notifications;
    }
  }

  void clearSubscriptions(uint16_t connectionHandle)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (SubscriptionSlot &slot : subscriptions)
    {
      if (slot.used && slot.connectionHandle == connectionHandle)
      {
        slot = SubscriptionSlot();
      }
    }
  }

  bool subscribed(uint16_t connectionHandle, uint8_t reportId, bool battery = false) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (const SubscriptionSlot &slot : subscriptions)
    {
      if (slot.used && slot.connectionHandle == connectionHandle)
      {
        return battery ? slot.batteryNotifications :
          (slot.inputNotifications & static_cast<uint8_t>(1u << (reportId - 1))) != 0;
      }
    }
    return false;
  }

  static int accessCallback(
    uint16_t connectionHandle,
    uint16_t attributeHandle,
    ble_gatt_access_ctxt *context,
    void *argument)
  {
    return static_cast<EspBleHidDeviceManagerImpl *>(argument)->handleAccess(
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
      // Values written by the loop task are read here on the NimBLE host
      // task; hold the mutex to avoid torn reads.
      std::lock_guard<std::mutex> lock(mutex);
      if (context->chr == &hidCharacteristics[0])
      {
        return appendValue(context->om, hidInformation, sizeof(hidInformation));
      }
      if (context->chr == &hidCharacteristics[1])
      {
        return appendValue(context->om, reportMap, reportMapLength);
      }
      for (uint8_t index = 0; index < ProfileCount; ++index)
      {
        if (inputCharacteristics[index] != nullptr && context->chr == inputCharacteristics[index])
        {
          return appendValue(context->om, inputValues[index], inputLengths[index]);
        }
      }
      if (context->chr == outputCharacteristic)
      {
        return appendValue(context->om, &outputValue, 1);
      }
      if (context->chr == vendorOutputCharacteristic)
      {
        return appendValue(context->om, vendorOutputValue, vendorOutputLength);
      }
      if (context->chr == vendorFeatureCharacteristic)
      {
        return appendValue(context->om, vendorFeatureValue, vendorFeatureLength);
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
      if (context->chr == outputCharacteristic)
      {
        uint16_t length = 0;
        uint8_t value = 0;
        if (ble_hs_mbuf_to_flat(context->om, &value, 1, &length) != 0 || length != 1)
        {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        {
          std::lock_guard<std::mutex> lock(mutex);
          outputValue = value;
        }
        queueOutputReport(connectionHandle, value);
        return 0;
      }
      if (context->chr == vendorOutputCharacteristic ||
          context->chr == vendorFeatureCharacteristic)
      {
        uint8_t value[MaxVendorReportSize] = {};
        uint16_t length = 0;
        if (ble_hs_mbuf_to_flat(context->om, value, sizeof(value), &length) != 0 ||
            length != vendorReportSize)
        {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        const uint8_t reportType = context->chr == vendorOutputCharacteristic
          ? ESP_BLE_HID_REPORT_TYPE_OUTPUT
          : ESP_BLE_HID_REPORT_TYPE_FEATURE;
        {
          std::lock_guard<std::mutex> lock(mutex);
          uint8_t *destination = reportType == ESP_BLE_HID_REPORT_TYPE_OUTPUT
            ? vendorOutputValue : vendorFeatureValue;
          size_t &destinationLength = reportType == ESP_BLE_HID_REPORT_TYPE_OUTPUT
            ? vendorOutputLength : vendorFeatureLength;
          memcpy(destination, value, length);
          destinationLength = length;
        }
        queueVendorReport(connectionHandle, reportType, value, length);
        return 0;
      }
    }
    else if (context->op == BLE_GATT_ACCESS_OP_READ_DSC)
    {
      for (uint8_t index = 0; index < ProfileCount; ++index)
      {
        if (context->dsc == &inputDescriptors[index][0])
        {
          const uint8_t reference[] = {static_cast<uint8_t>(index + 1), 0x01};
          return appendValue(context->om, reference, sizeof(reference));
        }
      }
      if (context->dsc == &outputDescriptors[0])
      {
        const uint8_t reference[] = {ESP_BLE_HID_REPORT_ID_KEYBOARD, 0x02};
        return appendValue(context->om, reference, sizeof(reference));
      }
      if (context->dsc == &vendorOutputDescriptors[0])
      {
        const uint8_t reference[] = {ESP_BLE_HID_REPORT_ID_VENDOR, 0x02};
        return appendValue(context->om, reference, sizeof(reference));
      }
      if (context->dsc == &vendorFeatureDescriptors[0])
      {
        const uint8_t reference[] = {ESP_BLE_HID_REPORT_ID_VENDOR, 0x03};
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
    if (report.connectionId == 0)
    {
      // 0 is not a valid connection ID; the connection is already gone.
      std::lock_guard<std::mutex> lock(mutex);
      ++droppedOutputReports;
      return;
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

  void queueVendorReport(
    uint16_t connectionHandle, uint8_t reportType,
    const uint8_t *data, size_t length)
  {
    VendorReportEntry report;
    report.reportType = reportType;
    report.length = length;
    memcpy(report.data, data, length);
    if (device->owner_->impl_ != nullptr)
    {
      std::lock_guard<std::mutex> connectionLock(device->owner_->impl_->mutex);
      report.connectionId = device->owner_->impl_->findPeripheralConnectionId(connectionHandle);
    }
    if (report.connectionId == 0)
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++droppedVendorReports;
      return;
    }
    std::lock_guard<std::mutex> lock(mutex);
    if (vendorReportCount == OutputQueueCapacity)
    {
      ++droppedVendorReports;
      return;
    }
    const size_t tail = (vendorReportHead + vendorReportCount) % OutputQueueCapacity;
    vendorReports[tail] = report;
    ++vendorReportCount;
  }

  EspBleHidKeyboard *device;
  mutable std::mutex mutex;
  EspBleHidDeviceConfig config;
  bool configured = false;
  uint8_t profileMask = 0;
  uint8_t mouseButtonCount = 5;
  uint8_t vendorReportSize = 63;
  bool realized = false;
  uint16_t inputValueHandles[ProfileCount] = {};
  uint16_t outputValueHandle = 0;
  uint16_t batteryValueHandle = 0;
  uint8_t inputValues[ProfileCount][MaxVendorReportSize] = {};
  uint8_t inputLengths[ProfileCount] = {8, 4, 11, 2, 1, 63};
  uint8_t outputValue = 0;
  uint8_t vendorOutputValue[MaxVendorReportSize] = {};
  size_t vendorOutputLength = 0;
  uint8_t vendorFeatureValue[MaxVendorReportSize] = {};
  size_t vendorFeatureLength = 0;
  uint8_t batteryLevel = 100;
  uint8_t hidInformation[4] = {0x11, 0x01, 0x00, 0x01};
  uint8_t pnpId[7] = {};
  uint8_t reportMap[384] = {};
  size_t reportMapLength = 0;
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
  ble_gatt_dsc_def inputDescriptors[ProfileCount][2] = {};
  ble_gatt_dsc_def outputDescriptors[2] = {};
  ble_gatt_dsc_def vendorOutputDescriptors[2] = {};
  ble_gatt_dsc_def vendorFeatureDescriptors[2] = {};
  ble_gatt_chr_def hidCharacteristics[14] = {};
  ble_gatt_chr_def *inputCharacteristics[ProfileCount] = {};
  ble_gatt_chr_def *outputCharacteristic = nullptr;
  ble_gatt_chr_def *vendorOutputCharacteristic = nullptr;
  ble_gatt_chr_def *vendorFeatureCharacteristic = nullptr;
  ble_gatt_chr_def deviceInformationCharacteristics[3] = {};
  ble_gatt_chr_def batteryCharacteristics[2] = {};
  ble_gatt_svc_def services[4] = {};
  EspBleHidKeyboardOutputReport outputReports[OutputQueueCapacity];
  size_t outputHead = 0;
  size_t outputCount = 0;
  size_t droppedOutputReports = 0;
  VendorReportEntry vendorReports[OutputQueueCapacity];
  size_t vendorReportHead = 0;
  size_t vendorReportCount = 0;
  size_t droppedVendorReports = 0;
  SubscriptionSlot subscriptions[ConnectionCapacity];
  ble_gap_event_listener gapListener = {};
  bool gapListenerRegistered = false;
};

struct EspBleHidKeyboardHostImpl
{
  static constexpr size_t QueueCapacity = 8;
  static constexpr size_t MaxInputReports = 8;
  static constexpr size_t MaxFieldsPerReport = 40;

  struct ReportFormat
  {
    uint16_t inputBitLength = 0;
    EspBleHidReportField fields[MaxFieldsPerReport];
    size_t fieldCount = 0;
  };

  struct Connection
  {
    bool used = false;
    EspBleConnectionId connectionId = 0;
    uint8_t reportId = 0;
    BLERemoteCharacteristic *inputReport = nullptr;
    BLERemoteCharacteristic *outputReport = nullptr;
    BLERemoteCharacteristic *vendorOutputReport = nullptr;
    BLERemoteCharacteristic *vendorFeatureReport = nullptr;
    BLERemoteCharacteristic *inputReports[MaxInputReports] = {};
    EspBleHidReportKind inputKinds[MaxInputReports] = {};
    uint8_t inputReportIds[MaxInputReports] = {};
    ReportFormat inputFormats[MaxInputReports];
    size_t inputReportCount = 0;
    uint8_t mouseButtons = 0;
    uint16_t consumerUsage = 0;
    uint8_t systemUsage = 0;
    uint8_t gamepadData[64] = {};
    size_t gamepadLength = 0;
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
    Raw,
  };

  struct Event
  {
    EventType type = EventType::Discovery;
    EspBleHidKeyboardHostDiscovery discovery;
    EspBleHidKeyboardState state;
    EspBleHidReportKind kind = EspBleHidReportKind::Unknown;
    EspBleConnectionId connectionId = 0;
    uint8_t reportId = 0;
    uint8_t raw[64] = {};
    size_t rawLength = 0;
    uint8_t previousButtons = 0;
    uint16_t previousUsage = 0;
    bool changed = false;
    int16_t mouseX = 0;
    int16_t mouseY = 0;
    int16_t mouseWheel = 0;
    uint8_t mouseButtons = 0;
  };

  explicit EspBleHidKeyboardHostImpl(EspBleHidHost *host) : host(host) {}

  static void resetConnection(Connection &connection)
  {
    connection.~Connection();
    new (&connection) Connection();
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

  bool pushEventLocked(const Event &event, bool critical)
  {
    if (eventCount == QueueCapacity)
    {
      bool evicted = false;
      if (critical)
      {
        // Discovery results and the all-release event synthesized on
        // disconnection must not be lost; evict the oldest key state event.
        for (size_t offset = 0; offset < eventCount; ++offset)
        {
          if (events[(eventHead + offset) % QueueCapacity].type != EventType::State)
          {
            continue;
          }
          for (size_t next = offset; next + 1 < eventCount; ++next)
          {
            events[(eventHead + next) % QueueCapacity] =
              std::move(events[(eventHead + next + 1) % QueueCapacity]);
          }
          --eventCount;
          ++droppedEvents;
          evicted = true;
          break;
        }
      }
      if (!evicted)
      {
        ++droppedEvents;
        return false;
      }
    }
    events[(eventHead + eventCount) % QueueCapacity] = event;
    ++eventCount;
    return true;
  }

  void pushEvent(const Event &event)
  {
    std::lock_guard<std::mutex> lock(mutex);
    pushEventLocked(event, event.type == EventType::Discovery);
  }

  void queueInputReport(
    EspBleConnectionId connectionId,
    uint8_t reportId,
    const uint8_t *data,
    size_t length)
  {
    if (data == nullptr || length != 8)
    {
      // Count instead of vanishing: an unexpected report length otherwise
      // shows up as "discovery succeeded but no keys arrive".
      std::lock_guard<std::mutex> lock(mutex);
      ++invalidInputReports;
      return;
    }
    for (size_t index = 0; index < 6; ++index)
    {
      const uint8_t usage = data[index + 2];
      if (usage >= 0x01 && usage <= 0x03)
      {
        // Phantom state (ErrorRollOver/POSTFail/ErrorUndefined): standard
        // hosts ignore the report and keep the previous key state.
        return;
      }
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
      pushEventLocked(event, false);
    }
  }

  void queueRawReport(EspBleConnectionId connectionId, uint8_t reportId,
                      EspBleHidReportKind kind, const uint8_t *data, size_t length)
  {
    Event event;
    if (data == nullptr || length == 0 || length > sizeof(event.raw))
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++invalidInputReports;
      return;
    }
    event.type = EventType::Raw;
    event.kind = kind;
    event.connectionId = connectionId;
    event.reportId = reportId;
    event.rawLength = length;
    memcpy(event.raw, data, length);
    std::lock_guard<std::mutex> lock(mutex);
    Connection *connection = findConnection(connectionId);
    if (connection == nullptr) return;
    const ReportFormat *format = nullptr;
    for (size_t index = 0; index < connection->inputReportCount; ++index)
    {
      if (connection->inputReportIds[index] == reportId && connection->inputKinds[index] == kind)
      {
        format = &connection->inputFormats[index];
        break;
      }
    }
    if (format == nullptr || (format->inputBitLength != 0 &&
        length != (format->inputBitLength + 7) / 8))
    {
      ++invalidInputReports;
      return;
    }
    if (kind == EspBleHidReportKind::Mouse)
    {
      event.previousButtons = connection->mouseButtons;
      for (size_t index = 0; index < format->fieldCount; ++index)
      {
        const EspBleHidReportField &field = format->fields[index];
        const int32_t value = espBleHidReadFieldValue(field, data, length);
        if (field.usagePage == 0x09 && field.usage >= 1 && field.usage <= 8 && value != 0)
          event.mouseButtons |= static_cast<uint8_t>(1u << (field.usage - 1));
        else if (field.usagePage == 0x01 && field.usage == 0x30) event.mouseX = value;
        else if (field.usagePage == 0x01 && field.usage == 0x31) event.mouseY = value;
        else if (field.usagePage == 0x01 && field.usage == 0x38) event.mouseWheel = value;
      }
      connection->mouseButtons = event.mouseButtons;
      event.changed = event.mouseButtons != event.previousButtons || event.mouseX != 0 ||
        event.mouseY != 0 || event.mouseWheel != 0;
    }
    else if (kind == EspBleHidReportKind::ConsumerControl && length >= 2)
    {
      const uint16_t usage = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
      event.previousUsage = connection->consumerUsage;
      connection->consumerUsage = usage;
      event.changed = usage != event.previousUsage;
    }
    else if (kind == EspBleHidReportKind::SystemControl)
    {
      event.previousUsage = connection->systemUsage;
      connection->systemUsage = data[0];
      event.changed = data[0] != event.previousUsage;
    }
    else if (kind == EspBleHidReportKind::Gamepad)
    {
      event.changed = connection->gamepadLength != length ||
        memcmp(connection->gamepadData, data, length) != 0;
      memcpy(connection->gamepadData, data, length);
      connection->gamepadLength = length;
    }
    else if (kind == EspBleHidReportKind::Vendor)
    {
      event.changed = true;
    }
    if (event.changed) pushEventLocked(event, false);
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

    BLERemoteCharacteristic *inputReports[8] = {};
    EspBleHidReportKind inputKinds[8] = {};
    uint8_t inputReportIds[8] = {};
    size_t inputReportCount = 0;
    BLERemoteCharacteristic *outputReports[8] = {};
    uint8_t outputReportIds[8] = {};
    size_t outputReportCount = 0;
    BLERemoteCharacteristic *featureReports[8] = {};
    uint8_t featureReportIds[8] = {};
    size_t featureReportCount = 0;
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
        const EspBleHidKeyboardReportMapInfo keyboardInfo = espBleParseKeyboardReportMap(
          reinterpret_cast<const uint8_t *>(reportMapValue.c_str()),
          reportMapValue.length());
        const EspBleHidReportMapInfo mapInfo = espBleParseHidReportMap(
          reinterpret_cast<const uint8_t *>(reportMapValue.c_str()), reportMapValue.length());
        if (reportMap == nullptr || mapInfo.count == 0)
        {
          result.error = EspBleError::InvalidState;
          result.detail = "HID Report Map has no supported input report";
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
            if (value.length() == 2)
            {
              const uint8_t reportId = static_cast<uint8_t>(value[0]);
              const uint8_t reportType = static_cast<uint8_t>(value[1]);
              const EspBleHidReportKind kind = mapInfo.kindForReportId(reportId);
              if (reportType == 1 && characteristic->canNotify() &&
                  kind != EspBleHidReportKind::Unknown && inputReportCount < 8)
              {
                inputReports[inputReportCount] = characteristic;
                inputKinds[inputReportCount] = kind;
                inputReportIds[inputReportCount++] = reportId;
                if (kind == EspBleHidReportKind::Keyboard) result.reportId = reportId;
              }
              else if (reportType == 2 && outputReportCount < 8)
              {
                outputReports[outputReportCount] = characteristic;
                outputReportIds[outputReportCount++] = reportId;
              }
              else if (reportType == 3 && featureReportCount < 8)
              {
                featureReports[featureReportCount] = characteristic;
                featureReportIds[featureReportCount++] = reportId;
              }
            }
            else if (mapInfo.count == 1 && !mapInfo.entries[0].hasReportId)
            {
              // No Report Reference descriptor: acceptable when the Report
              // Map declares no report IDs (single-report keyboards).
              if (characteristic->canNotify() && inputReportCount == 0)
              {
                inputReports[0] = characteristic;
                inputKinds[0] = mapInfo.entries[0].kind;
                inputReportIds[0] = 0;
                inputReportCount = 1;
                result.reportId = 0;
              }
              else if ((characteristic->canWrite() || characteristic->canWriteNoResponse()) &&
                       outputReportCount < 8)
              {
                outputReports[outputReportCount] = characteristic;
                outputReportIds[outputReportCount++] = 0;
              }
            }
          }
          if (inputReportCount == 0)
          {
            result.error = EspBleError::NotFound;
            result.detail = "supported HID Input Report was not found";
          }
          else
          {
            BLERemoteCharacteristic *outputReport = nullptr;
            BLERemoteCharacteristic *vendorOutputReport = nullptr;
            BLERemoteCharacteristic *vendorFeatureReport = nullptr;
            for (size_t index = 0; index < outputReportCount; ++index)
            {
              if (keyboardInfo.keyboardFound && outputReportIds[index] == result.reportId)
              {
                outputReport = outputReports[index];
              }
              if (outputReportIds[index] == ESP_BLE_HID_REPORT_ID_VENDOR)
                vendorOutputReport = outputReports[index];
            }
            for (size_t index = 0; index < featureReportCount; ++index)
              if (featureReportIds[index] == ESP_BLE_HID_REPORT_ID_VENDOR)
                vendorFeatureReport = featureReports[index];
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
                result.detail = "too many HID Host connections";
              }
              else
              {
                connection->reportId = result.reportId;
                connection->inputReport = nullptr;
                connection->outputReport = outputReport;
                connection->vendorOutputReport = vendorOutputReport;
                connection->vendorFeatureReport = vendorFeatureReport;
                connection->inputReportCount = inputReportCount;
                for (size_t index = 0; index < inputReportCount; ++index)
                {
                  connection->inputReports[index] = inputReports[index];
                  connection->inputKinds[index] = inputKinds[index];
                  connection->inputReportIds[index] = inputReportIds[index];
                  ReportFormat &format = connection->inputFormats[index];
                  for (size_t entryIndex = 0; entryIndex < mapInfo.count; ++entryIndex)
                  {
                    if (mapInfo.entries[entryIndex].kind == inputKinds[index] &&
                        mapInfo.entries[entryIndex].reportId == inputReportIds[index])
                      format.inputBitLength = mapInfo.entries[entryIndex].inputBitLength;
                  }
                  for (size_t fieldIndex = 0; fieldIndex < mapInfo.fieldCount &&
                       format.fieldCount < MaxFieldsPerReport; ++fieldIndex)
                  {
                    if (mapInfo.fields[fieldIndex].kind == inputKinds[index] &&
                        mapInfo.fields[fieldIndex].reportId == inputReportIds[index])
                      format.fields[format.fieldCount++] = mapInfo.fields[fieldIndex];
                  }
                  if (inputKinds[index] == EspBleHidReportKind::Keyboard)
                    connection->inputReport = inputReports[index];
                }
              }
            }
            if (result.error == EspBleError::None)
            {
              result.success = true;
              for (size_t index = 0; index < inputReportCount && result.success; ++index)
              {
                const EspBleConnectionId connectionId = result.connectionId;
                const uint8_t reportId = inputReportIds[index];
                const EspBleHidReportKind kind = inputKinds[index];
                result.success = inputReports[index]->subscribe(
                  true,
                  [impl, connectionId, reportId, kind](
                    BLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
                    if (kind == EspBleHidReportKind::Keyboard)
                      impl->queueInputReport(connectionId, reportId, data, length);
                    else
                      impl->queueRawReport(connectionId, reportId, kind, data, length);
                  }, true);
              }
              if (!result.success)
              {
                result.error = EspBleError::BackendFailure;
                result.detail = "failed to subscribe to HID Input Report";
                std::lock_guard<std::mutex> lock(impl->mutex);
                Connection *connection = impl->findConnection(result.connectionId);
                if (connection != nullptr)
                {
                  resetConnection(*connection);
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

  EspBleHidHost *host;
  mutable std::mutex mutex;
  Connection connections[ConnectionCapacity];
  Event events[QueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  size_t invalidInputReports = 0;
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
      result.addressType = static_cast<EspBleAddressType>(device.getAddressType());
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
  if (serviceUuidCount_ > 0)
  {
    // CSS Part A 1.1: a data type must not occur more than once in a payload,
    // so all UUIDs of one size share a single "Complete List" AD structure.
    String uuids16;
    String uuids32;
    String uuids128;
    for (size_t index = 0; index < serviceUuidCount_; ++index)
    {
      BLEUUID uuid(serviceUuids_[index].c_str());
      switch (uuid.bitSize())
      {
      case 16:
        uuids16 += String(
          reinterpret_cast<const char *>(&uuid.getNative()->u16.value), 2);
        break;
      case 32:
        uuids32 += String(
          reinterpret_cast<const char *>(&uuid.getNative()->u32.value), 4);
        break;
      default:
        uuids128 += String(
          reinterpret_cast<const char *>(uuid.getNative()->u128.value), 16);
        break;
      }
    }
    previousLength = advertisingData.getPayload().length();
    size_t expectedLength = previousLength;
    const struct
    {
      const String *uuids;
      char type;
    } lists[] = {
      {&uuids16, 0x03},  // Complete List of 16-bit Service UUIDs
      {&uuids32, 0x05},  // Complete List of 32-bit Service UUIDs
      {&uuids128, 0x07}, // Complete List of 128-bit Service UUIDs
    };
    for (const auto &list : lists)
    {
      if (list.uuids->isEmpty())
      {
        continue;
      }
      const char header[2] = {static_cast<char>(list.uuids->length() + 1), list.type};
      advertisingData.addData(String(header, 2) + *list.uuids);
      expectedLength += 2 + list.uuids->length();
    }
    if (advertisingData.getPayload().length() != expectedLength)
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

void EspBleScanner::flushPendingResults()
{
  if (impl_ == nullptr)
  {
    return;
  }
  // Results queued but never dispatched must not leak into the next
  // begin() session.
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->head = 0;
  impl_->count = 0;
  impl_->dropped = 0;
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

bool EspBleGattServer::addDescriptor(
  const char *serviceUuid,
  const char *characteristicUuid,
  const char *descriptorUuid,
  const EspBleGattDescriptorConfig &config)
{
  if (owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "GATT descriptors must be configured before begin");
    return false;
  }
  if (serviceUuid == nullptr || serviceUuid[0] == '\0' ||
      characteristicUuid == nullptr || characteristicUuid[0] == '\0' ||
      descriptorUuid == nullptr || descriptorUuid[0] == '\0' || config.maximumLength == 0)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid GATT descriptor arguments");
    return false;
  }
  if (!config.readable && !config.writable)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT descriptor has no access permissions");
    return false;
  }
  if ((config.encryptedRead || config.authenticatedRead) && !config.readable)
  {
    owner_->setError(EspBleError::InvalidArgument, "secured descriptor read requires readable access");
    return false;
  }
  if ((config.encryptedWrite || config.authenticatedWrite) && !config.writable)
  {
    owner_->setError(EspBleError::InvalidArgument, "secured descriptor write requires writable access");
    return false;
  }
  if (impl_ == nullptr)
  {
    owner_->setError(EspBleError::NotFound, "GATT characteristic was not added");
    return false;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  bool characteristicFound = false;
  for (size_t index = 0; index < impl_->characteristicCount; ++index)
  {
    const auto &definition = impl_->characteristics[index];
    if (uuidEquals(definition.serviceUuid, serviceUuid) &&
        uuidEquals(definition.uuid, characteristicUuid))
    {
      characteristicFound = true;
      break;
    }
  }
  if (!characteristicFound)
  {
    owner_->setError(EspBleError::NotFound, "GATT characteristic was not added");
    return false;
  }
  for (size_t index = 0; index < impl_->descriptorCount; ++index)
  {
    const auto &definition = impl_->descriptors[index];
    if (uuidEquals(definition.serviceUuid, serviceUuid) &&
        uuidEquals(definition.characteristicUuid, characteristicUuid) &&
        uuidEquals(definition.uuid, descriptorUuid))
    {
      owner_->setError(EspBleError::InvalidArgument, "GATT descriptor already exists");
      return false;
    }
  }
  if (impl_->descriptorCount == MaxDescriptors)
  {
    owner_->setError(EspBleError::ResourceExhausted, "too many GATT descriptors");
    return false;
  }
  auto &definition = impl_->descriptors[impl_->descriptorCount++];
  definition.serviceUuid = serviceUuid;
  definition.characteristicUuid = characteristicUuid;
  definition.uuid = descriptorUuid;
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

bool EspBleGattServer::setDescriptorValue(
  const char *serviceUuid,
  const char *characteristicUuid,
  const char *descriptorUuid,
  const uint8_t *data,
  size_t length)
{
  if (serviceUuid == nullptr || characteristicUuid == nullptr || descriptorUuid == nullptr ||
      (data == nullptr && length != 0) || impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid GATT descriptor value arguments");
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  for (size_t index = 0; index < impl_->descriptorCount; ++index)
  {
    auto &definition = impl_->descriptors[index];
    if (uuidEquals(definition.serviceUuid, serviceUuid) &&
        uuidEquals(definition.characteristicUuid, characteristicUuid) &&
        uuidEquals(definition.uuid, descriptorUuid))
    {
      if (length > definition.config.maximumLength)
      {
        owner_->setError(EspBleError::InvalidArgument, "GATT descriptor value exceeds maximum length");
        return false;
      }
      definition.value = length == 0
        ? String()
        : String(reinterpret_cast<const char *>(data), length);
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
  owner_->setError(EspBleError::NotFound, "GATT descriptor was not found");
  return false;
}

bool EspBleGattServer::setDescriptorValue(
  const char *serviceUuid,
  const char *characteristicUuid,
  const char *descriptorUuid,
  const String &value)
{
  return setDescriptorValue(
    serviceUuid, characteristicUuid, descriptorUuid,
    reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
}

bool EspBleGattServer::descriptorValue(
  const char *serviceUuid,
  const char *characteristicUuid,
  const char *descriptorUuid,
  String &value) const
{
  if (serviceUuid == nullptr || characteristicUuid == nullptr || descriptorUuid == nullptr ||
      impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  for (size_t index = 0; index < impl_->descriptorCount; ++index)
  {
    const auto &definition = impl_->descriptors[index];
    if (uuidEquals(definition.serviceUuid, serviceUuid) &&
        uuidEquals(definition.characteristicUuid, characteristicUuid) &&
        uuidEquals(definition.uuid, descriptorUuid))
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
    if (impl_->sending)
    {
      impl_->sendTask = task;
    }
  }
  owner_->clearError();
  return true;
}

void EspBleGattServer::onWritten(WriteCallback callback)
{
  writeCallback_ = std::move(callback);
}

void EspBleGattServer::onDescriptorWritten(DescriptorWriteCallback callback)
{
  descriptorWriteCallback_ = std::move(callback);
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

      for (size_t descriptorIndex = 0;
           descriptorIndex < impl_->descriptorCount;
           ++descriptorIndex)
      {
        auto &descriptorDefinition = impl_->descriptors[descriptorIndex];
        if (!uuidEquals(descriptorDefinition.serviceUuid, serviceDefinition.uuid.c_str()) ||
            !uuidEquals(descriptorDefinition.characteristicUuid,
                        characteristicDefinition.uuid.c_str()))
        {
          continue;
        }
        descriptorDefinition.backend = new BLEDescriptor(
          descriptorDefinition.uuid.c_str(), descriptorDefinition.config.maximumLength);
        if (descriptorDefinition.backend == nullptr)
        {
          owner_->setError(EspBleError::ResourceExhausted, "failed to create GATT descriptor");
          return false;
        }
        uint16_t permissions = 0;
        const EspBleGattDescriptorConfig &descriptorConfig = descriptorDefinition.config;
        if (descriptorConfig.readable) permissions |= ESP_GATT_PERM_READ;
        if (descriptorConfig.writable) permissions |= ESP_GATT_PERM_WRITE;
        if (descriptorConfig.encryptedRead) permissions |= ESP_GATT_PERM_READ_ENCRYPTED;
        if (descriptorConfig.encryptedWrite) permissions |= ESP_GATT_PERM_WRITE_ENCRYPTED;
        if (descriptorConfig.authenticatedRead) permissions |= ESP_GATT_PERM_READ_ENC_MITM;
        if (descriptorConfig.authenticatedWrite) permissions |= ESP_GATT_PERM_WRITE_ENC_MITM;
        descriptorDefinition.backend->setAccessPermissions(permissions);
        descriptorDefinition.backend->setCallbacks(&impl_->descriptorCallbacks);
        descriptorDefinition.backend->setValue(
          reinterpret_cast<const uint8_t *>(descriptorDefinition.value.c_str()),
          descriptorDefinition.value.length());
        characteristicDefinition.backend->addDescriptor(descriptorDefinition.backend);
      }
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
  for (size_t index = 0; index < impl_->descriptorCount; ++index)
  {
    impl_->descriptors[index].backend = nullptr;
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

void EspBleGattServer::dispatchDescriptorWrite(const EspBleGattDescriptorWrite &write)
{
  if (descriptorWriteCallback_)
  {
    descriptorWriteCallback_(write);
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

EspBleHidKeyboard::EspBleHidKeyboard(EspBle *owner) : owner_(owner) {}

EspBleHidKeyboard::~EspBleHidKeyboard()
{
  delete impl_;
}

bool EspBleHidKeyboard::configure(const EspBleHidKeyboardConfig &config)
{
  if (!configureProfile(ESP_BLE_HID_REPORT_ID_KEYBOARD, config)) return false;
  layout_ = config.layout;
  return true;
}

bool EspBleHidKeyboard::configureProfile(
  uint8_t reportId, const EspBleHidDeviceConfig &config)
{
  if (owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "HID Keyboard Device must be configured before begin");
    return false;
  }
  if (config.initialBatteryLevel > 100)
  {
    owner_->setError(
      EspBleError::InvalidArgument,
      "HID Keyboard battery level must be at most 100");
    return false;
  }
  if (impl_ == nullptr)
  {
    impl_ = new EspBleHidDeviceManagerImpl(this);
    if (impl_ == nullptr)
    {
      owner_->setError(EspBleError::ResourceExhausted, "failed to allocate HID Keyboard Device state");
      return false;
    }
  }

  const bool firstConfiguration = !impl_->configured;
  if (firstConfiguration)
  {
    impl_->config = config;
  }
  impl_->profileMask |= static_cast<uint8_t>(1u << (reportId - 1));
  impl_->configured = true;
  if (firstConfiguration && !owner_->advertising().addServiceUuid("1812"))
  {
    impl_->configured = false;
    return false;
  }
  if (reportId == ESP_BLE_HID_REPORT_ID_KEYBOARD)
  {
    owner_->advertising().setAppearance(HidKeyboardAppearance);
  }
  owner_->clearError();
  return true;
}

bool EspBleHidKeyboard::realize()
{
  if (impl_ == nullptr || !impl_->configured || impl_->realized)
  {
    return true;
  }
  if (!owner_->preparePeripheral())
  {
    return false;
  }

  static const uint8_t keyboardMap[] = {
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
  static const uint8_t mouseMap[] = {
    0x05,0x01, 0x09,0x02, 0xa1,0x01, 0x85,0x02, 0x09,0x01, 0xa1,0x00,
    0x05,0x09, 0x19,0x01, 0x29,0x05, 0x15,0x00, 0x25,0x01, 0x95,0x05,
    0x75,0x01, 0x81,0x02, 0x95,0x01, 0x75,0x03, 0x81,0x01, 0x05,0x01,
    0x09,0x30, 0x09,0x31, 0x09,0x38, 0x15,0x81, 0x25,0x7f, 0x75,0x08,
    0x95,0x03, 0x81,0x06, 0xc0, 0xc0};
  static const uint8_t gamepadMap[] = {
    0x05,0x01, 0x09,0x05, 0xa1,0x01, 0x85,0x03, 0x15,0x81, 0x25,0x7f,
    0x09,0x30, 0x09,0x31, 0x09,0x32, 0x09,0x35, 0x09,0x33, 0x09,0x34,
    0x75,0x08, 0x95,0x06, 0x81,0x02, 0x15,0x00, 0x25,0x08, 0x35,0x00,
    0x46,0x3b,0x01, 0x65,0x14, 0x09,0x39, 0x75,0x08, 0x95,0x01,
    0x81,0x02, 0x65,0x00, 0x05,0x09, 0x19,0x01, 0x29,0x20, 0x15,0x00,
    0x25,0x01, 0x75,0x01, 0x95,0x20, 0x81,0x02, 0xc0};
  static const uint8_t consumerMap[] = {
    0x05,0x0c, 0x09,0x01, 0xa1,0x01, 0x85,0x04, 0x15,0x00, 0x26,0xff,0x03,
    0x19,0x00, 0x2a,0xff,0x03, 0x75,0x10, 0x95,0x01, 0x81,0x00, 0xc0};
  static const uint8_t systemMap[] = {
    0x05,0x01, 0x09,0x80, 0xa1,0x01, 0x85,0x05, 0x15,0x00, 0x25,0x03,
    0x19,0x00, 0x29,0x03, 0x75,0x08, 0x95,0x01, 0x81,0x00, 0xc0};
  uint8_t vendorMap[] = {
    0x06,0x00,0xff, 0x09,0x01, 0xa1,0x01, 0x85,0x06,
    0x15,0x00, 0x26,0xff,0x00, 0x75,0x08,
    0x09,0x01, 0x95,0x3f, 0x81,0x02,
    0x09,0x02, 0x95,0x3f, 0x91,0x02,
    0x09,0x03, 0x95,0x3f, 0xb1,0x02, 0xc0};
  vendorMap[19] = impl_->vendorReportSize;
  vendorMap[25] = impl_->vendorReportSize;
  vendorMap[31] = impl_->vendorReportSize;
  struct MapPart { const uint8_t *data; size_t length; };
  const MapPart maps[] = {{keyboardMap,sizeof(keyboardMap)}, {mouseMap,sizeof(mouseMap)},
    {gamepadMap,sizeof(gamepadMap)}, {consumerMap,sizeof(consumerMap)},
    {systemMap,sizeof(systemMap)}, {vendorMap,sizeof(vendorMap)}};
  impl_->reportMapLength = 0;
  for (uint8_t index = 0; index < EspBleHidDeviceManagerImpl::ProfileCount; ++index)
  {
    if ((impl_->profileMask & static_cast<uint8_t>(1u << index)) == 0) continue;
    const size_t mapOffset = impl_->reportMapLength;
    memcpy(impl_->reportMap + impl_->reportMapLength, maps[index].data, maps[index].length);
    impl_->reportMapLength += maps[index].length;
    if (index == 1)
    {
      impl_->reportMap[mapOffset + 17] = impl_->mouseButtonCount;
      impl_->reportMap[mapOffset + 23] = impl_->mouseButtonCount;
      impl_->reportMap[mapOffset + 31] = static_cast<uint8_t>(8 - impl_->mouseButtonCount);
    }
  }
  impl_->hidInformation[2] = impl_->config.countryCode;
  impl_->batteryLevel = impl_->config.initialBatteryLevel;
  impl_->pnpId[0] = 0x02; // USB Implementers Forum vendor ID source.
  impl_->pnpId[1] = static_cast<uint8_t>(impl_->config.vendorId);
  impl_->pnpId[2] = static_cast<uint8_t>(impl_->config.vendorId >> 8);
  impl_->pnpId[3] = static_cast<uint8_t>(impl_->config.productId);
  impl_->pnpId[4] = static_cast<uint8_t>(impl_->config.productId >> 8);
  impl_->pnpId[5] = static_cast<uint8_t>(impl_->config.productVersion);
  impl_->pnpId[6] = static_cast<uint8_t>(impl_->config.productVersion >> 8);

  // HOGP requires Security Mode 1 Level 2 (encryption) on the HID Service
  // attributes; the insufficient-encryption error is what makes a Host OS
  // start pairing. Only applied when security is enabled.
  const bool requireEncryption = owner_->impl_->securityEnabled;
  const ble_gatt_chr_flags encryptedRead =
    requireEncryption ? BLE_GATT_CHR_F_READ_ENC : 0;
  const ble_gatt_chr_flags encryptedWrite =
    requireEncryption ? BLE_GATT_CHR_F_WRITE_ENC : 0;
  const uint8_t descriptorFlags = static_cast<uint8_t>(
    BLE_ATT_F_READ | (requireEncryption ? BLE_ATT_F_READ_ENC : 0));

  impl_->outputDescriptors[0].uuid = &impl_->reportReferenceUuid.u;
  impl_->outputDescriptors[0].att_flags = descriptorFlags;
  impl_->outputDescriptors[0].access_cb = EspBleHidDeviceManagerImpl::accessCallback;
  impl_->outputDescriptors[0].arg = impl_;
  impl_->vendorOutputDescriptors[0].uuid = &impl_->reportReferenceUuid.u;
  impl_->vendorOutputDescriptors[0].att_flags = descriptorFlags;
  impl_->vendorOutputDescriptors[0].access_cb = EspBleHidDeviceManagerImpl::accessCallback;
  impl_->vendorOutputDescriptors[0].arg = impl_;
  impl_->vendorFeatureDescriptors[0].uuid = &impl_->reportReferenceUuid.u;
  impl_->vendorFeatureDescriptors[0].att_flags = descriptorFlags;
  impl_->vendorFeatureDescriptors[0].access_cb = EspBleHidDeviceManagerImpl::accessCallback;
  impl_->vendorFeatureDescriptors[0].arg = impl_;

  impl_->hidCharacteristics[0].uuid = &impl_->hidInformationUuid.u;
  impl_->hidCharacteristics[0].access_cb = EspBleHidDeviceManagerImpl::accessCallback;
  impl_->hidCharacteristics[0].arg = impl_;
  impl_->hidCharacteristics[0].flags = BLE_GATT_CHR_F_READ | encryptedRead;
  impl_->hidCharacteristics[1].uuid = &impl_->reportMapUuid.u;
  impl_->hidCharacteristics[1].access_cb = EspBleHidDeviceManagerImpl::accessCallback;
  impl_->hidCharacteristics[1].arg = impl_;
  impl_->hidCharacteristics[1].flags = BLE_GATT_CHR_F_READ | encryptedRead;
  impl_->hidCharacteristics[2].uuid = &impl_->hidControlPointUuid.u;
  impl_->hidCharacteristics[2].access_cb = EspBleHidDeviceManagerImpl::accessCallback;
  impl_->hidCharacteristics[2].arg = impl_;
  impl_->hidCharacteristics[2].flags = BLE_GATT_CHR_F_WRITE_NO_RSP | encryptedWrite;
  size_t characteristicIndex = 3;
  for (uint8_t index = 0; index < EspBleHidDeviceManagerImpl::ProfileCount; ++index)
  {
    if ((impl_->profileMask & static_cast<uint8_t>(1u << index)) == 0) continue;
    impl_->inputDescriptors[index][0].uuid = &impl_->reportReferenceUuid.u;
    impl_->inputDescriptors[index][0].att_flags = descriptorFlags;
    impl_->inputDescriptors[index][0].access_cb = EspBleHidDeviceManagerImpl::accessCallback;
    impl_->inputDescriptors[index][0].arg = impl_;
    ble_gatt_chr_def &characteristic = impl_->hidCharacteristics[characteristicIndex++];
    characteristic.uuid = &impl_->reportUuid.u;
    characteristic.access_cb = EspBleHidDeviceManagerImpl::accessCallback;
    characteristic.arg = impl_;
    characteristic.descriptors = impl_->inputDescriptors[index];
    characteristic.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | encryptedRead;
    characteristic.val_handle = &impl_->inputValueHandles[index];
    impl_->inputCharacteristics[index] = &characteristic;
  }
  if ((impl_->profileMask & 0x01) != 0)
  {
    ble_gatt_chr_def &characteristic = impl_->hidCharacteristics[characteristicIndex++];
    characteristic.uuid = &impl_->reportUuid.u;
    characteristic.access_cb = EspBleHidDeviceManagerImpl::accessCallback;
    characteristic.arg = impl_;
    characteristic.descriptors = impl_->outputDescriptors;
    characteristic.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
      BLE_GATT_CHR_F_WRITE_NO_RSP | encryptedRead | encryptedWrite;
    characteristic.val_handle = &impl_->outputValueHandle;
    impl_->outputCharacteristic = &characteristic;
  }
  if ((impl_->profileMask & static_cast<uint8_t>(
        1u << (ESP_BLE_HID_REPORT_ID_VENDOR - 1))) != 0)
  {
    ble_gatt_chr_def &output = impl_->hidCharacteristics[characteristicIndex++];
    output.uuid = &impl_->reportUuid.u;
    output.access_cb = EspBleHidDeviceManagerImpl::accessCallback;
    output.arg = impl_;
    output.descriptors = impl_->vendorOutputDescriptors;
    output.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
      BLE_GATT_CHR_F_WRITE_NO_RSP | encryptedRead | encryptedWrite;
    impl_->vendorOutputCharacteristic = &output;

    ble_gatt_chr_def &feature = impl_->hidCharacteristics[characteristicIndex++];
    feature.uuid = &impl_->reportUuid.u;
    feature.access_cb = EspBleHidDeviceManagerImpl::accessCallback;
    feature.arg = impl_;
    feature.descriptors = impl_->vendorFeatureDescriptors;
    feature.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
      encryptedRead | encryptedWrite;
    impl_->vendorFeatureCharacteristic = &feature;
  }

  impl_->deviceInformationCharacteristics[0].uuid = &impl_->manufacturerUuid.u;
  impl_->deviceInformationCharacteristics[0].access_cb =
    EspBleHidDeviceManagerImpl::accessCallback;
  impl_->deviceInformationCharacteristics[0].arg = impl_;
  impl_->deviceInformationCharacteristics[0].flags = BLE_GATT_CHR_F_READ;
  impl_->deviceInformationCharacteristics[1].uuid = &impl_->pnpIdUuid.u;
  impl_->deviceInformationCharacteristics[1].access_cb =
    EspBleHidDeviceManagerImpl::accessCallback;
  impl_->deviceInformationCharacteristics[1].arg = impl_;
  impl_->deviceInformationCharacteristics[1].flags = BLE_GATT_CHR_F_READ;

  impl_->batteryCharacteristics[0].uuid = &impl_->batteryLevelUuid.u;
  impl_->batteryCharacteristics[0].access_cb = EspBleHidDeviceManagerImpl::accessCallback;
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
  bool handlesRegistered = true;
  for (uint8_t index = 0; index < EspBleHidDeviceManagerImpl::ProfileCount; ++index)
  {
    if ((impl_->profileMask & static_cast<uint8_t>(1u << index)) != 0 &&
        impl_->inputValueHandles[index] == 0) handlesRegistered = false;
  }
  if ((impl_->profileMask & 0x01) != 0 && impl_->outputValueHandle == 0)
    handlesRegistered = false;
  if (!handlesRegistered)
  {
    owner_->setError(EspBleError::BackendFailure, "HID report handles were not registered");
    return false;
  }
  if (!impl_->gapListenerRegistered &&
      ble_gap_event_listener_register(
        &impl_->gapListener,
        EspBleHidDeviceManagerImpl::gapListenerEntry,
        impl_) == 0)
  {
    impl_->gapListenerRegistered = true;
  }
  impl_->realized = true;
  return true;
}

bool EspBleHidKeyboard::sendReport(const EspBleHidKeyboardReport &report)
{
  uint8_t value[8] = {report.modifiers, 0};
  memcpy(value + 2, report.keys, sizeof(report.keys));
  return sendRawReport(ESP_BLE_HID_REPORT_ID_KEYBOARD, value, sizeof(value));
}

bool EspBleHidKeyboard::sendRawReport(
  uint8_t reportId, const uint8_t *data, size_t length)
{
  if (!owner_->initialized() || impl_ == nullptr || !impl_->realized ||
      reportId < 1 || reportId > EspBleHidDeviceManagerImpl::ProfileCount ||
      impl_->inputValueHandles[reportId - 1] == 0)
  {
    owner_->setError(EspBleError::InvalidState, "HID Keyboard Device is not initialized");
    return false;
  }

  uint16_t connectionHandles[ConnectionCapacity] = {};
  size_t connectionCount = 0;
  bool anyPeripheral = false;
  {
    std::lock_guard<std::mutex> lock(owner_->impl_->mutex);
    for (const EspBleImpl::ConnectionSlot &slot : owner_->impl_->connections)
    {
      if (slot.used && slot.connection.localRole == EspBleRole::Peripheral)
      {
        anyPeripheral = true;
        // HOGP: never push HID data over an unencrypted link when security
        // is enabled.
        if (owner_->impl_->securityEnabled && !slot.connection.encrypted)
        {
          continue;
        }
        connectionHandles[connectionCount++] = slot.connection.handle;
      }
    }
  }
  // Only notify peers that subscribed to the Input Report CCCD.
  size_t eligibleCount = 0;
  for (size_t index = 0; index < connectionCount; ++index)
  {
    if (impl_->subscribed(connectionHandles[index], reportId))
    {
      connectionHandles[eligibleCount++] = connectionHandles[index];
    }
  }
  connectionCount = eligibleCount;
  if (connectionCount == 0)
  {
    owner_->setError(
      EspBleError::InvalidState,
      anyPeripheral ? "no subscribed HID Host" : "no connected HID Host");
    return false;
  }

  const uint8_t reportIndex = reportId - 1;
  if (data == nullptr || length == 0 || length > sizeof(impl_->inputValues[reportIndex]))
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid HID input report");
    return false;
  }
  uint8_t inputValue[sizeof(impl_->inputValues[reportIndex])] = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    memcpy(impl_->inputValues[reportIndex], data, length);
    impl_->inputLengths[reportIndex] = static_cast<uint8_t>(length);
    if (length < sizeof(impl_->inputValues[reportIndex]))
    {
      memset(impl_->inputValues[reportIndex] + length, 0,
             sizeof(impl_->inputValues[reportIndex]) - length);
    }
    memcpy(inputValue, impl_->inputValues[reportIndex], length);
  }

  bool sent = false;
  int lastBackendCode = 0;
  for (size_t index = 0; index < connectionCount; ++index)
  {
    os_mbuf *value = ble_hs_mbuf_from_flat(inputValue, length);
    if (value == nullptr)
    {
      lastBackendCode = BLE_HS_ENOMEM;
      continue;
    }
    const int backendCode = ble_gatts_notify_custom(
      connectionHandles[index], impl_->inputValueHandles[reportIndex], value);
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

bool EspBleHidKeyboard::pressUsage(uint8_t usage, uint8_t modifiers, uint32_t)
{
  EspBleHidKeyboardReport report;
  report.modifiers = modifiers;
  report.keys[0] = usage;
  return sendReport(report);
}

bool EspBleHidKeyboard::tapUsage(uint8_t usage, uint8_t modifiers, uint32_t holdMs)
{
  if (!pressUsage(usage, modifiers)) return false;
  delay(holdMs);
  return releaseAll();
}

bool EspBleHidKeyboard::pressKey(char key, uint32_t)
{
  const uint8_t modifiers[] = {0, EspBleHidKeyboardReport::LeftShift,
    EspBleHidKeyboardReport::RightAlt,
    static_cast<uint8_t>(EspBleHidKeyboardReport::LeftShift |
                         EspBleHidKeyboardReport::RightAlt)};
  for (uint8_t modifier : modifiers)
  {
    for (uint16_t usage = 1; usage < 256; ++usage)
    {
      if (espBleUsageToUnicode(static_cast<uint8_t>(usage), modifier, layout_, false, false) ==
          static_cast<uint8_t>(key))
      {
        return pressUsage(static_cast<uint8_t>(usage), modifier);
      }
    }
  }
  owner_->setError(EspBleError::InvalidArgument, "character is not available in keyboard layout");
  return false;
}

bool EspBleHidKeyboard::tapKey(char key, uint32_t holdMs)
{
  if (!pressKey(key)) return false;
  delay(holdMs);
  return releaseAll();
}

bool EspBleHidKeyboard::write(const char *text, uint32_t interKeyDelayMs)
{
  if (text == nullptr)
  {
    owner_->setError(EspBleError::InvalidArgument, "text must not be null");
    return false;
  }
  for (const char *cursor = text; *cursor != '\0'; ++cursor)
  {
    if (!tapKey(*cursor)) return false;
    if (interKeyDelayMs != 0) delay(interKeyDelayMs);
  }
  return true;
}

void EspBleHidKeyboard::setLayout(EspBleKeyboardLayout layout)
{
  layout_ = layout;
}

EspBleKeyboardLayout EspBleHidKeyboard::layout() const
{
  return layout_;
}

bool EspBleHidKeyboard::releaseAll()
{
  return sendReport(EspBleHidKeyboardReport());
}

bool EspBleHidKeyboard::setBatteryLevel(uint8_t level)
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
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->config.initialBatteryLevel = level;
    impl_->batteryLevel = level;
  }

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
      // Only notify peers that subscribed to the Battery Level CCCD.
      if (!impl_->subscribed(connectionHandles[index], 1, true))
      {
        continue;
      }
      os_mbuf *value = ble_hs_mbuf_from_flat(&level, 1);
      if (value != nullptr)
      {
        ble_gatts_notify_custom(connectionHandles[index], impl_->batteryValueHandle, value);
      }
    }
  }
  owner_->clearError();
  return true;
}

void EspBleHidKeyboard::onOutputReport(OutputReportCallback callback)
{
  outputReportCallback_ = std::move(callback);
}

bool EspBleHidKeyboard::configured() const
{
  return impl_ != nullptr && impl_->configured;
}

void EspBleHidKeyboard::resetBackend()
{
  if (impl_ == nullptr)
  {
    return;
  }
  if (impl_->gapListenerRegistered)
  {
    ble_gap_event_listener_unregister(&impl_->gapListener);
    impl_->gapListenerRegistered = false;
  }
  impl_->realized = false;
  memset(impl_->inputValueHandles, 0, sizeof(impl_->inputValueHandles));
  impl_->outputValueHandle = 0;
  impl_->batteryValueHandle = 0;
  memset(impl_->inputValues, 0, sizeof(impl_->inputValues));
  impl_->outputValue = 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->outputHead = 0;
  impl_->outputCount = 0;
  impl_->vendorReportHead = 0;
  impl_->vendorReportCount = 0;
  impl_->vendorOutputLength = 0;
  impl_->vendorFeatureLength = 0;
  for (EspBleHidDeviceManagerImpl::SubscriptionSlot &slot : impl_->subscriptions)
  {
    slot = EspBleHidDeviceManagerImpl::SubscriptionSlot();
  }
}

void EspBleHidKeyboard::dispatchPendingOutputReports()
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
      impl_->outputHead = (impl_->outputHead + 1) % EspBleHidDeviceManagerImpl::OutputQueueCapacity;
      --impl_->outputCount;
    }
    outputReportCallback_(report);
  }
}

bool EspBleHidMouse::configure(const EspBleHidMouseConfig &config)
{
  if (config.buttons == 0 || config.buttons > 5)
  {
    owner_->setError(EspBleError::InvalidArgument, "mouse buttons must be between 1 and 5");
    return false;
  }
  if (!owner_->hidKeyboardDevice_.configureProfile(ESP_BLE_HID_REPORT_ID_MOUSE, config)) return false;
  owner_->hidKeyboardDevice_.impl_->mouseButtonCount = config.buttons;
  configured_ = true;
  return true;
}

bool EspBleHidMouse::configured() const { return configured_; }
bool EspBleHidMouse::sendReport(const EspBleHidMouseReport &report)
{
  return owner_->hidKeyboardDevice_.sendRawReport(
    ESP_BLE_HID_REPORT_ID_MOUSE, reinterpret_cast<const uint8_t *>(&report), sizeof(report));
}
bool EspBleHidMouse::move(int8_t x, int8_t y, int8_t wheelAmount, uint8_t buttons)
{
  buttons_ = buttons;
  EspBleHidMouseReport report{buttons_, x, y, wheelAmount};
  return sendReport(report);
}
bool EspBleHidMouse::wheel(int8_t amount) { return move(0, 0, amount, buttons_); }
bool EspBleHidMouse::press(uint8_t buttons) { buttons_ |= buttons; return move(0, 0, 0, buttons_); }
bool EspBleHidMouse::release(uint8_t buttons) { buttons_ &= static_cast<uint8_t>(~buttons); return move(0, 0, 0, buttons_); }
bool EspBleHidMouse::click(uint8_t button, uint32_t holdMs)
{
  if (!press(button)) return false;
  delay(holdMs);
  return release(button);
}
bool EspBleHidMouse::releaseAll() { buttons_ = 0; return move(0, 0, 0, 0); }
uint8_t EspBleHidMouse::buttons() const { return buttons_; }

bool EspBleHidConsumerControl::configure(const EspBleHidConsumerControlConfig &config)
{
  configured_ = owner_->hidKeyboardDevice_.configureProfile(
    ESP_BLE_HID_REPORT_ID_CONSUMER_CONTROL, config);
  return configured_;
}
bool EspBleHidConsumerControl::configured() const { return configured_; }
bool EspBleHidConsumerControl::sendReport(uint16_t usage)
{
  uint8_t value[] = {static_cast<uint8_t>(usage), static_cast<uint8_t>(usage >> 8)};
  return owner_->hidKeyboardDevice_.sendRawReport(ESP_BLE_HID_REPORT_ID_CONSUMER_CONTROL, value, sizeof(value));
}
bool EspBleHidConsumerControl::sendUsage(uint16_t usage) { usage_ = usage; return sendReport(usage); }
bool EspBleHidConsumerControl::press(uint16_t usage) { return sendUsage(usage); }
bool EspBleHidConsumerControl::release() { return sendUsage(0); }
bool EspBleHidConsumerControl::click(uint16_t usage, uint32_t holdMs)
{ if (!press(usage)) return false; delay(holdMs); return release(); }
bool EspBleHidConsumerControl::releaseAll() { return release(); }
uint16_t EspBleHidConsumerControl::usage() const { return usage_; }

bool EspBleHidSystemControl::configure(const EspBleHidSystemControlConfig &config)
{
  configured_ = owner_->hidKeyboardDevice_.configureProfile(
    ESP_BLE_HID_REPORT_ID_SYSTEM_CONTROL, config);
  return configured_;
}
bool EspBleHidSystemControl::configured() const { return configured_; }
bool EspBleHidSystemControl::sendReport(uint8_t usage)
{ return owner_->hidKeyboardDevice_.sendRawReport(ESP_BLE_HID_REPORT_ID_SYSTEM_CONTROL, &usage, 1); }
bool EspBleHidSystemControl::sendUsage(uint8_t usage) { usage_ = usage; return sendReport(usage); }
bool EspBleHidSystemControl::press(uint8_t usage) { return sendUsage(usage); }
bool EspBleHidSystemControl::release() { return sendUsage(0); }
bool EspBleHidSystemControl::click(uint8_t usage, uint32_t holdMs)
{ if (!press(usage)) return false; delay(holdMs); return release(); }
bool EspBleHidSystemControl::releaseAll() { return release(); }
uint8_t EspBleHidSystemControl::usage() const { return usage_; }

bool EspBleHidGamepad::configure(const EspBleHidGamepadConfig &config)
{
  configured_ = owner_->hidKeyboardDevice_.configureProfile(
    ESP_BLE_HID_REPORT_ID_GAMEPAD, config);
  return configured_;
}
bool EspBleHidGamepad::configured() const { return configured_; }
bool EspBleHidGamepad::sendReport(const EspBleHidGamepadReport &report)
{
  uint8_t value[11] = {static_cast<uint8_t>(report.x), static_cast<uint8_t>(report.y),
    static_cast<uint8_t>(report.z), static_cast<uint8_t>(report.rz),
    static_cast<uint8_t>(report.rx), static_cast<uint8_t>(report.ry), report.hat,
    static_cast<uint8_t>(report.buttons), static_cast<uint8_t>(report.buttons >> 8),
    static_cast<uint8_t>(report.buttons >> 16), static_cast<uint8_t>(report.buttons >> 24)};
  return owner_->hidKeyboardDevice_.sendRawReport(
    ESP_BLE_HID_REPORT_ID_GAMEPAD, value, sizeof(value));
}
bool EspBleHidGamepad::send(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry,
                            uint8_t hat, uint32_t buttons)
{ EspBleHidGamepadReport report{x, y, z, rz, rx, ry, hat, buttons}; return sendReport(report); }
bool EspBleHidGamepad::releaseAll() { return sendReport(EspBleHidGamepadReport()); }

bool EspBleHidVendor::configure(const EspBleHidVendorConfig &config)
{
  if (config.reportSize == 0 ||
      config.reportSize > EspBleHidDeviceManagerImpl::MaxVendorReportSize)
  {
    owner_->setError(EspBleError::InvalidArgument,
      "vendor HID report size must be between 1 and 64");
    return false;
  }
  configured_ = owner_->hidKeyboardDevice_.configureProfile(
    ESP_BLE_HID_REPORT_ID_VENDOR, config);
  if (configured_)
  {
    owner_->hidKeyboardDevice_.impl_->vendorReportSize = config.reportSize;
    owner_->hidKeyboardDevice_.impl_->inputLengths[ESP_BLE_HID_REPORT_ID_VENDOR - 1] =
      config.reportSize;
  }
  return configured_;
}

bool EspBleHidVendor::configured() const { return configured_; }

bool EspBleHidVendor::sendInput(const void *data, size_t length)
{
  if (!configured_ || owner_->hidKeyboardDevice_.impl_ == nullptr ||
      data == nullptr || length == 0 ||
      length != owner_->hidKeyboardDevice_.impl_->vendorReportSize)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid vendor HID input report");
    return false;
  }
  return owner_->hidKeyboardDevice_.sendRawReport(
    ESP_BLE_HID_REPORT_ID_VENDOR,
    static_cast<const uint8_t *>(data), length);
}

void EspBleHidVendor::onOutputReport(ReportCallback callback)
{
  outputCallback_ = std::move(callback);
}

void EspBleHidVendor::onFeatureReport(ReportCallback callback)
{
  featureCallback_ = std::move(callback);
}

void EspBleHidVendor::dispatchPendingReports()
{
  EspBleHidDeviceManagerImpl *impl = owner_->hidKeyboardDevice_.impl_;
  if (impl == nullptr) return;
  while (true)
  {
    EspBleHidDeviceManagerImpl::VendorReportEntry entry;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->vendorReportCount == 0) break;
      entry = impl->vendorReports[impl->vendorReportHead];
      impl->vendorReportHead =
        (impl->vendorReportHead + 1) % EspBleHidDeviceManagerImpl::OutputQueueCapacity;
      --impl->vendorReportCount;
    }
    EspBleHidVendorReport report;
    report.connectionId = entry.connectionId;
    report.reportId = ESP_BLE_HID_REPORT_ID_VENDOR;
    report.reportType = entry.reportType;
    report.rawData = entry.data;
    report.rawLength = entry.length;
    report.data = entry.data;
    report.length = entry.length;
    ReportCallback &callback = entry.reportType == ESP_BLE_HID_REPORT_TYPE_OUTPUT
      ? outputCallback_ : featureCallback_;
    if (callback) callback(report);
  }
}

EspBleHidHost::EspBleHidHost(EspBle *owner) : owner_(owner) {}

EspBleHidHost::~EspBleHidHost()
{
  delete impl_;
}

bool EspBleHidHost::discover(EspBleConnectionId connectionId)
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
      owner_->setError(EspBleError::ResourceExhausted, "failed to allocate HID Host state");
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
    // Lets disconnect() reject requests against this connection while the
    // discovery worker is still using it.
    owner_->impl_->gattConnectionId = connectionId;
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
    16384,
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
    if (impl_->discovering)
    {
      impl_->discoveryTask = task;
    }
  }
  owner_->clearError();
  return true;
}

bool EspBleHidHost::setKeyboardLeds(
  EspBleConnectionId connectionId,
  bool numLock,
  bool capsLock,
  bool scrollLock,
  bool compose,
  bool kana)
{
  if (!owner_->initialized() || owner_->impl_ == nullptr || impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidState, "HID Host is not initialized");
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
    owner_->impl_->gattConnectionId = connectionId;
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
  const bool response = !outputReport->canWriteNoResponse();
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

bool EspBleHidHost::sendVendorOutput(
  EspBleConnectionId connectionId, const uint8_t *data, size_t length)
{
  return sendVendorReport(connectionId, data, length, false);
}

bool EspBleHidHost::sendVendorFeature(
  EspBleConnectionId connectionId, const uint8_t *data, size_t length)
{
  return sendVendorReport(connectionId, data, length, true);
}

bool EspBleHidHost::sendVendorReport(
  EspBleConnectionId connectionId,
  const uint8_t *data,
  size_t length,
  bool feature)
{
  if (!owner_->initialized() || owner_->impl_ == nullptr || impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidState, "HID Host is not initialized");
    return false;
  }
  if (data == nullptr || length == 0 || length > 64)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid vendor HID report");
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
    owner_->impl_->gattConnectionId = connectionId;
  }

  BLERemoteCharacteristic *report = nullptr;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    EspBleHidKeyboardHostImpl::Connection *connection = impl_->findConnection(connectionId);
    if (connection != nullptr)
      report = feature ? connection->vendorFeatureReport : connection->vendorOutputReport;
  }
  if (report == nullptr)
  {
    std::lock_guard<std::mutex> ownerLock(owner_->impl_->mutex);
    owner_->impl_->gattOperating = false;
    owner_->setError(EspBleError::NotFound,
      feature ? "HID Vendor Feature Report was not found"
              : "HID Vendor Output Report was not found");
    return false;
  }

  const bool response = feature || !report->canWriteNoResponse();
  const bool success = report->writeValue(
    const_cast<uint8_t *>(data), length, response);
  {
    std::lock_guard<std::mutex> ownerLock(owner_->impl_->mutex);
    owner_->impl_->gattOperating = false;
  }
  if (!success)
  {
    owner_->setError(EspBleError::BackendFailure,
      feature ? "failed to write HID Vendor Feature Report"
              : "failed to write HID Vendor Output Report");
    return false;
  }
  owner_->clearError();
  return true;
}

void EspBleHidHost::onDiscovered(DiscoveryCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  discoveryCallback_ = callback ? std::make_shared<DiscoveryCallback>(std::move(callback)) : nullptr;
}

void EspBleHidHost::onKeyboardState(StateCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  stateCallback_ = callback ? std::make_shared<StateCallback>(std::move(callback)) : nullptr;
}

void EspBleHidHost::onKeyboard(KeyboardCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  keyboardCallback_ = callback ? std::make_shared<KeyboardCallback>(std::move(callback)) : nullptr;
}

void EspBleHidHost::onMouse(MouseCallback callback)
{ std::lock_guard<std::mutex> lock(listenerMutex_); mouseCallback_ = callback ? std::make_shared<MouseCallback>(std::move(callback)) : nullptr; }
void EspBleHidHost::onConsumerControl(ConsumerControlCallback callback)
{ std::lock_guard<std::mutex> lock(listenerMutex_); consumerControlCallback_ = callback ? std::make_shared<ConsumerControlCallback>(std::move(callback)) : nullptr; }
void EspBleHidHost::onSystemControl(SystemControlCallback callback)
{ std::lock_guard<std::mutex> lock(listenerMutex_); systemControlCallback_ = callback ? std::make_shared<SystemControlCallback>(std::move(callback)) : nullptr; }
void EspBleHidHost::onGamepad(GamepadCallback callback)
{ std::lock_guard<std::mutex> lock(listenerMutex_); gamepadCallback_ = callback ? std::make_shared<GamepadCallback>(std::move(callback)) : nullptr; }
void EspBleHidHost::onVendorInput(VendorInputCallback callback)
{ std::lock_guard<std::mutex> lock(listenerMutex_); vendorInputCallback_ = callback ? std::make_shared<VendorInputCallback>(std::move(callback)) : nullptr; }

template <typename Callback>
EspBleListenerId EspBleHidHost::addListener(
  ListenerSlot<Callback> *slots,
  Callback callback)
{
  if (!callback)
  {
    owner_->setError(EspBleError::InvalidArgument, "listener callback is empty");
    return EspBleInvalidListenerId;
  }
  std::shared_ptr<Callback> stored = std::make_shared<Callback>(std::move(callback));
  std::lock_guard<std::mutex> lock(listenerMutex_);
  for (size_t i = 0; i < MaxListenersPerEvent; ++i)
  {
    if (slots[i].id == EspBleInvalidListenerId)
    {
      const EspBleListenerId id = allocateListenerIdLocked();
      if (id == EspBleInvalidListenerId) break;
      slots[i].id = id;
      slots[i].callback = std::move(stored);
      owner_->clearError();
      return id;
    }
  }
  owner_->setError(EspBleError::ResourceExhausted, "too many HID Host listeners");
  return EspBleInvalidListenerId;
}

bool EspBleHidHost::listenerIdInUseLocked(EspBleListenerId listenerId) const
{
  const auto contains = [listenerId](const auto &slots) {
    for (const auto &slot : slots) if (slot.id == listenerId) return true;
    return false;
  };
  return contains(discoveryListeners_) || contains(stateListeners_) ||
    contains(keyboardListeners_) || contains(mouseListeners_) ||
    contains(consumerControlListeners_) || contains(systemControlListeners_) ||
    contains(gamepadListeners_) || contains(vendorInputListeners_);
}

EspBleListenerId EspBleHidHost::allocateListenerIdLocked()
{
  EspBleListenerId candidate = nextListenerId_;
  if (candidate == EspBleInvalidListenerId) candidate = 1;
  const EspBleListenerId first = candidate;
  do
  {
    if (!listenerIdInUseLocked(candidate))
    {
      nextListenerId_ = candidate + 1;
      if (nextListenerId_ == EspBleInvalidListenerId) nextListenerId_ = 1;
      return candidate;
    }
    ++candidate;
    if (candidate == EspBleInvalidListenerId) candidate = 1;
  } while (candidate != first);
  return EspBleInvalidListenerId;
}

template <typename Callback>
bool EspBleHidHost::removeListenerFrom(
  ListenerSlot<Callback> *slots,
  EspBleListenerId listenerId)
{
  for (size_t i = 0; i < MaxListenersPerEvent; ++i)
  {
    if (slots[i].id == listenerId)
    {
      for (size_t next = i + 1; next < MaxListenersPerEvent; ++next)
      {
        slots[next - 1] = std::move(slots[next]);
      }
      slots[MaxListenersPerEvent - 1] = ListenerSlot<Callback>();
      return true;
    }
  }
  return false;
}

EspBleListenerId EspBleHidHost::addDiscoveredListener(DiscoveryCallback callback)
{
  return addListener(discoveryListeners_, std::move(callback));
}

EspBleListenerId EspBleHidHost::addKeyboardStateListener(StateCallback callback)
{
  return addListener(stateListeners_, std::move(callback));
}

EspBleListenerId EspBleHidHost::addKeyboardListener(KeyboardCallback callback)
{
  return addListener(keyboardListeners_, std::move(callback));
}

EspBleListenerId EspBleHidHost::addMouseListener(MouseCallback callback)
{ return addListener(mouseListeners_, std::move(callback)); }
EspBleListenerId EspBleHidHost::addConsumerControlListener(ConsumerControlCallback callback)
{ return addListener(consumerControlListeners_, std::move(callback)); }
EspBleListenerId EspBleHidHost::addSystemControlListener(SystemControlCallback callback)
{ return addListener(systemControlListeners_, std::move(callback)); }
EspBleListenerId EspBleHidHost::addGamepadListener(GamepadCallback callback)
{ return addListener(gamepadListeners_, std::move(callback)); }
EspBleListenerId EspBleHidHost::addVendorInputListener(VendorInputCallback callback)
{ return addListener(vendorInputListeners_, std::move(callback)); }

bool EspBleHidHost::removeListener(EspBleListenerId listenerId)
{
  if (listenerId == EspBleInvalidListenerId)
  {
    owner_->setError(EspBleError::InvalidArgument, "listener ID is invalid");
    return false;
  }
  std::lock_guard<std::mutex> lock(listenerMutex_);
  const bool removed =
    removeListenerFrom(discoveryListeners_, listenerId) ||
    removeListenerFrom(stateListeners_, listenerId) ||
    removeListenerFrom(keyboardListeners_, listenerId) ||
    removeListenerFrom(mouseListeners_, listenerId) ||
    removeListenerFrom(consumerControlListeners_, listenerId) ||
    removeListenerFrom(systemControlListeners_, listenerId) ||
    removeListenerFrom(gamepadListeners_, listenerId) ||
    removeListenerFrom(vendorInputListeners_, listenerId);
  if (!removed)
  {
    owner_->setError(EspBleError::NotFound, "listener ID was not found");
    return false;
  }
  owner_->clearError();
  return true;
}

void EspBleHidHost::setKeyboardLayout(EspBleKeyboardLayout layout)
{
  keyboardLayout_ = layout;
}

EspBleKeyboardLayout EspBleHidHost::keyboardLayout() const
{
  return keyboardLayout_;
}

size_t EspBleHidHost::droppedEventCount() const
{
  if (impl_ == nullptr)
  {
    return 0;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedEvents;
}

size_t EspBleHidHost::invalidInputReportCount() const
{
  if (impl_ == nullptr)
  {
    return 0;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->invalidInputReports;
}

bool EspBleHidHost::ready(EspBleConnectionId connectionId) const
{
  if (impl_ == nullptr)
  {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->findConnection(connectionId) != nullptr;
}

void EspBleHidHost::handleDisconnected(EspBleConnectionId connectionId)
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
  EspBleHidKeyboardHostImpl::resetConnection(*connection);
  if (hasHeldKey)
  {
    // The all-release event must not be lost even when the queue is full.
    impl_->pushEventLocked(event, true);
  }
}

void EspBleHidHost::resetBackend()
{
  if (impl_ == nullptr)
  {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  for (EspBleHidKeyboardHostImpl::Connection &connection : impl_->connections)
  {
    EspBleHidKeyboardHostImpl::resetConnection(connection);
  }
  impl_->eventHead = 0;
  impl_->eventCount = 0;
  impl_->discovering = false;
  impl_->discoveryTask = nullptr;
}

void EspBleHidHost::dispatchPendingEvents()
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
      std::shared_ptr<DiscoveryCallback> callbacks[MaxListenersPerEvent + 1];
      { std::lock_guard<std::mutex> lock(listenerMutex_); callbacks[0] = discoveryCallback_;
        for (size_t i = 0; i < MaxListenersPerEvent; ++i) callbacks[i + 1] = discoveryListeners_[i].callback; }
      for (auto &callback : callbacks) if (callback) (*callback)(event.discovery);
    }
    else if (event.type == EspBleHidKeyboardHostImpl::EventType::State)
    {
      {
        std::shared_ptr<StateCallback> callbacks[MaxListenersPerEvent + 1];
        { std::lock_guard<std::mutex> lock(listenerMutex_); callbacks[0] = stateCallback_;
          for (size_t i = 0; i < MaxListenersPerEvent; ++i) callbacks[i + 1] = stateListeners_[i].callback; }
        for (auto &callback : callbacks) if (callback) (*callback)(event.state);
      }

      std::shared_ptr<KeyboardCallback> keyboardCallbacks[MaxListenersPerEvent + 1];
      bool hasKeyboardCallback = false;
      {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        keyboardCallbacks[0] = keyboardCallback_;
        hasKeyboardCallback = static_cast<bool>(keyboardCallbacks[0]);
        for (size_t i = 0; i < MaxListenersPerEvent; ++i)
        {
          keyboardCallbacks[i + 1] = keyboardListeners_[i].callback;
          hasKeyboardCallback = hasKeyboardCallback || static_cast<bool>(keyboardCallbacks[i + 1]);
        }
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
            keyboardEvent.unicode = espBleUsageToUnicode(
              usage,
              pressed ? event.state.modifiers : previousModifiers,
              keyboardLayout_,
              event.state.capsLock,
              event.state.numLock);
            keyboardEvent.ascii = keyboardEvent.unicode <= 0xff
              ? static_cast<uint8_t>(keyboardEvent.unicode)
              : 0;
            for (auto &callback : keyboardCallbacks)
            {
              if (callback)
              {
                (*callback)(keyboardEvent);
              }
            }
          }
        }
      }
    }
    else if (event.kind == EspBleHidReportKind::Mouse)
    {
      EspBleHidMouseEvent value;
      value.connectionId = event.connectionId; value.reportId = event.reportId;
      value.rawData = event.raw; value.rawLength = event.rawLength;
      value.buttons = event.mouseButtons; value.previousButtons = event.previousButtons;
      value.x = event.mouseX; value.y = event.mouseY; value.wheel = event.mouseWheel;
      value.moved = value.x != 0 || value.y != 0 || value.wheel != 0;
      value.buttonsChanged = value.buttons != value.previousButtons;
      std::shared_ptr<MouseCallback> callbacks[MaxListenersPerEvent + 1];
      { std::lock_guard<std::mutex> lock(listenerMutex_); callbacks[0] = mouseCallback_;
        for (size_t i = 0; i < MaxListenersPerEvent; ++i) callbacks[i + 1] = mouseListeners_[i].callback; }
      for (auto &callback : callbacks) if (callback) (*callback)(value);
    }
    else if (event.kind == EspBleHidReportKind::ConsumerControl && event.rawLength >= 2)
    {
      EspBleHidConsumerControlEvent value;
      value.connectionId = event.connectionId; value.reportId = event.reportId;
      value.rawData = event.raw; value.rawLength = event.rawLength;
      value.usage = static_cast<uint16_t>(event.raw[0]) |
        (static_cast<uint16_t>(event.raw[1]) << 8);
      value.pressed = value.usage != 0; value.released = value.usage == 0 && event.previousUsage != 0;
      std::shared_ptr<ConsumerControlCallback> callbacks[MaxListenersPerEvent + 1];
      { std::lock_guard<std::mutex> lock(listenerMutex_); callbacks[0] = consumerControlCallback_;
        for (size_t i = 0; i < MaxListenersPerEvent; ++i) callbacks[i + 1] = consumerControlListeners_[i].callback; }
      for (auto &callback : callbacks) if (callback) (*callback)(value);
    }
    else if (event.kind == EspBleHidReportKind::SystemControl)
    {
      EspBleHidSystemControlEvent value;
      value.connectionId = event.connectionId; value.reportId = event.reportId;
      value.rawData = event.raw; value.rawLength = event.rawLength; value.usage = event.raw[0];
      value.pressed = value.usage != 0; value.released = value.usage == 0 && event.previousUsage != 0;
      std::shared_ptr<SystemControlCallback> callbacks[MaxListenersPerEvent + 1];
      { std::lock_guard<std::mutex> lock(listenerMutex_); callbacks[0] = systemControlCallback_;
        for (size_t i = 0; i < MaxListenersPerEvent; ++i) callbacks[i + 1] = systemControlListeners_[i].callback; }
      for (auto &callback : callbacks) if (callback) (*callback)(value);
    }
    else if (event.kind == EspBleHidReportKind::Gamepad)
    {
      EspBleHidGamepadEvent value;
      value.connectionId = event.connectionId; value.reportId = event.reportId;
      value.rawData = event.raw; value.rawLength = event.rawLength; value.changed = event.changed;
      EspBleHidFieldValue *fields = nullptr;
      size_t fieldCount = 0;
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        EspBleHidKeyboardHostImpl::Connection *connection = impl_->findConnection(event.connectionId);
        if (connection != nullptr)
        {
          for (size_t reportIndex = 0; reportIndex < connection->inputReportCount; ++reportIndex)
          {
            if (connection->inputReportIds[reportIndex] != event.reportId ||
                connection->inputKinds[reportIndex] != EspBleHidReportKind::Gamepad) continue;
            const auto &format = connection->inputFormats[reportIndex];
            fieldCount = format.fieldCount;
            fields = new (std::nothrow) EspBleHidFieldValue[fieldCount];
            if (fields == nullptr)
            {
              fieldCount = 0;
              break;
            }
            for (size_t index = 0; index < fieldCount; ++index)
            {
              const EspBleHidReportField &definition = format.fields[index];
              fields[index].reportId = event.reportId;
              fields[index].usagePage = definition.usagePage;
              fields[index].usage = definition.usage;
              fields[index].value = espBleHidReadFieldValue(definition, event.raw, event.rawLength);
              fields[index].logicalMin = definition.logicalMin;
              fields[index].logicalMax = definition.logicalMax;
              fields[index].bitOffset = definition.bitOffset;
              fields[index].bitSize = definition.bitSize;
              fields[index].flags = definition.flags;
            }
            break;
          }
        }
      }
      value.fields = fields; value.fieldCount = fieldCount;
      std::shared_ptr<GamepadCallback> callbacks[MaxListenersPerEvent + 1];
      { std::lock_guard<std::mutex> lock(listenerMutex_); callbacks[0] = gamepadCallback_;
        for (size_t i = 0; i < MaxListenersPerEvent; ++i) callbacks[i + 1] = gamepadListeners_[i].callback; }
      for (auto &callback : callbacks) if (callback) (*callback)(value);
      delete[] fields;
    }
    else if (event.kind == EspBleHidReportKind::Vendor)
    {
      EspBleHidVendorInputEvent value;
      value.connectionId = event.connectionId;
      value.reportId = event.reportId;
      value.rawData = event.raw;
      value.rawLength = event.rawLength;
      std::shared_ptr<VendorInputCallback> callbacks[MaxListenersPerEvent + 1];
      {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        callbacks[0] = vendorInputCallback_;
        for (size_t i = 0; i < MaxListenersPerEvent; ++i)
          callbacks[i + 1] = vendorInputListeners_[i].callback;
      }
      for (auto &callback : callbacks) if (callback) (*callback)(value);
    }
  }
}

EspBle::EspBle()
    : advertising_(this), scanner_(this), gattServer_(this), hidKeyboardDevice_(this),
      hidMouse_(this), hidConsumerControl_(this), hidSystemControl_(this), hidGamepad_(this),
      hidVendor_(this), hidKeyboardHost_(this)
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
    const char *deviceName = config.deviceName == nullptr ? "" : config.deviceName;
    const bool sameConfig = activeDeviceName_ == deviceName &&
      activePreferredMtu_ == config.preferredMtu &&
      activeSecurity_.enabled == config.security.enabled &&
      activeSecurity_.bonding == config.security.bonding &&
      activeSecurity_.pairOnConnect == config.security.pairOnConnect &&
      activeSecurity_.mitm == config.security.mitm &&
      activeSecurity_.ioCapability == config.security.ioCapability &&
      activeSecurity_.staticPasskeyEnabled == config.security.staticPasskeyEnabled &&
      activeSecurity_.staticPasskey == config.security.staticPasskey;
    if (!sameConfig)
    {
      setError(
        EspBleError::InvalidState,
        "BLE stack is already initialized with a different configuration");
      return false;
    }
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

  activeDeviceName_ = deviceName;
  activePreferredMtu_ = config.preferredMtu;
  activeSecurity_ = config.security;
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
      bool cancelConnect = false;
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->connecting && !impl_->gattOperating)
        {
          break;
        }
        cancelConnect = impl_->connecting;
      }
      if (cancelConnect)
      {
        // Abort an in-flight connect attempt instead of blocking here until
        // its timeout (up to tens of seconds). Repeated calls are harmless.
        ble_gap_conn_cancel();
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
  scanner_.flushPendingResults();
  if (impl_ != nullptr)
  {
    // Free every Central client except the most recently created one, which
    // BLEDevice::deinit() frees itself. Slots are cleared first so the
    // onDisconnect callbacks triggered by ~BLEClient find nothing to retire.
    BLEClient *clients[ConnectionCapacity + EspBleImpl::RetiredClientCapacity];
    size_t clientCount = 0;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      for (EspBleImpl::ConnectionSlot &slot : impl_->connections)
      {
        if (slot.used && slot.client != nullptr && slot.client != impl_->newestClient)
        {
          clients[clientCount++] = slot.client;
        }
        slot = EspBleImpl::ConnectionSlot();
      }
      for (size_t index = 0; index < impl_->retiredClientCount; ++index)
      {
        if (impl_->retiredClients[index].client != impl_->newestClient)
        {
          clients[clientCount++] = impl_->retiredClients[index].client;
        }
      }
      impl_->retiredClientCount = 0;
    }
    for (size_t index = 0; index < clientCount; ++index)
    {
      delete clients[index];
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
  cancelExpiredConnectAttempt();
  expireGattOperation();
  scanner_.dispatchPendingResults();
  dispatchConnectionEvents();
  reapRetiredClients();
  hidKeyboardDevice_.dispatchPendingOutputReports();
  hidVendor_.dispatchPendingReports();
  hidKeyboardHost_.dispatchPendingEvents();
}

void EspBle::expireGattOperation()
{
  if (impl_ == nullptr) return;

  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->gattOperating || impl_->gattTimedOut ||
      (millis() - impl_->gattStartMilliseconds) < impl_->gattTimeoutMilliseconds)
  {
    return;
  }

  impl_->gattTimedOut = true;
  EspBleImpl::Event event;
  event.type = EspBleImpl::EventType::GattResult;
  event.gattResult.operation = impl_->gattOperation;
  event.gattResult.connectionId = impl_->gattConnectionId;
  event.gattResult.serviceUuid = impl_->gattServiceUuid;
  event.gattResult.characteristicUuid = impl_->gattCharacteristicUuid;
  event.gattResult.descriptorUuid = impl_->gattDescriptorUuid;
  event.gattResult.response = impl_->gattWriteResponse;
  event.gattResult.error = EspBleError::Timeout;
  event.gattResult.detail = "GATT operation timed out";
  impl_->pushEvent(event);
}

void EspBle::cancelExpiredConnectAttempt()
{
  if (impl_ == nullptr)
  {
    return;
  }

  bool cancel = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    // The backend ignores the timeout argument of the NimBLE
    // BLEClient::connect() overload and always waits its internal 30 second
    // default, so the requested timeout is enforced here instead.
    if (impl_->connecting && !impl_->connectCancelRequested &&
      (millis() - impl_->connectStartMilliseconds) >= impl_->connectTimeoutMilliseconds)
    {
      impl_->connectCancelRequested = true;
      cancel = true;
    }
  }
  if (cancel)
  {
    ble_gap_conn_cancel();
  }
}

void EspBle::reapRetiredClients()
{
  if (impl_ == nullptr)
  {
    return;
  }

  EspBleImpl::RetiredClient retired[EspBleImpl::RetiredClientCapacity];
  size_t count = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->gattOperating)
    {
      // A GATT worker task may still be traversing a retired client's remote
      // service objects; try again on the next update().
      return;
    }
    size_t kept = 0;
    for (size_t index = 0; index < impl_->retiredClientCount; ++index)
    {
      if (impl_->retiredClients[index].client == impl_->newestClient)
      {
        // BLEDevice::deinit() frees the most recently created client; keep it
        // retired until a newer client supersedes it or end() runs.
        impl_->retiredClients[kept++] = impl_->retiredClients[index];
      }
      else
      {
        retired[count++] = impl_->retiredClients[index];
      }
    }
    impl_->retiredClientCount = kept;
  }

  for (size_t index = 0; index < count; ++index)
  {
    // Clear any HID Host slot still pointing into the client's service tree
    // before ~BLEClient frees it.
    hidKeyboardHost_.handleDisconnected(retired[index].connectionId);
    delete retired[index].client;
  }
}

bool EspBle::connect(const EspBleScanResult &scanResult, uint32_t timeoutMilliseconds)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (!isValidBleAddress(scanResult.address.c_str()) ||
      !isValidAddressType(scanResult.addressType) || timeoutMilliseconds == 0)
  {
    setError(EspBleError::InvalidArgument, "valid peer address, address type, and nonzero timeout are required");
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
    impl_->connectStartMilliseconds = millis();
    impl_->connectCancelRequested = false;
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
    if (impl_->connecting)
    {
      impl_->connectTask = task;
    }
  }

  clearError();
  return true;
}

bool EspBle::connect(
  const char *address,
  EspBleAddressType addressType,
  uint32_t timeoutMilliseconds)
{
  EspBleScanResult target;
  target.address = address == nullptr ? "" : address;
  target.addressType = addressType;
  return connect(target, timeoutMilliseconds);
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

size_t EspBle::droppedEventCount() const
{
  if (impl_ == nullptr)
  {
    return 0;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedEvents;
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
  bond.peerAddressType = static_cast<EspBleAddressType>(address.getType());
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
    if (address.getType() == static_cast<uint8_t>(bond.peerAddressType) &&
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
  const char *characteristicUuid,
  uint32_t timeoutMilliseconds)
{
  return startGattOperation(
    EspBleGattOperation::Discover, connectionId, serviceUuid, characteristicUuid,
    nullptr, 0, true, nullptr, timeoutMilliseconds);
}

bool EspBle::discoverServices(
  EspBleConnectionId connectionId,
  uint32_t timeoutMilliseconds)
{
  return startGattOperation(
    EspBleGattOperation::DiscoverServices, connectionId, nullptr, nullptr,
    nullptr, 0, true, nullptr, timeoutMilliseconds);
}

size_t EspBle::discoveredServiceCount(EspBleConnectionId connectionId) const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const auto *database = impl_->gattDatabase;
  return database != nullptr && database->valid && database->connectionId == connectionId
    ? database->serviceCount : 0;
}

bool EspBle::discoveredService(
  EspBleConnectionId connectionId,
  size_t index,
  EspBleGattServiceInfo &service) const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const auto *database = impl_->gattDatabase;
  if (database == nullptr || !database->valid || database->connectionId != connectionId ||
      index >= database->serviceCount) return false;
  service = database->services[index];
  return true;
}

size_t EspBle::discoveredCharacteristicCount(
  EspBleConnectionId connectionId,
  const char *serviceUuid) const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const auto *database = impl_->gattDatabase;
  if (database == nullptr || !database->valid || database->connectionId != connectionId) return 0;
  size_t count = 0;
  for (size_t index = 0; index < database->characteristicCount; ++index)
  {
    if (serviceUuid == nullptr ||
        uuidEquals(database->characteristics[index].serviceUuid, serviceUuid)) ++count;
  }
  return count;
}

bool EspBle::discoveredCharacteristic(
  EspBleConnectionId connectionId,
  size_t index,
  EspBleGattCharacteristicInfo &characteristic,
  const char *serviceUuid) const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const auto *database = impl_->gattDatabase;
  if (database == nullptr || !database->valid || database->connectionId != connectionId) return false;
  size_t found = 0;
  for (size_t candidate = 0; candidate < database->characteristicCount; ++candidate)
  {
    const auto &value = database->characteristics[candidate];
    if (serviceUuid != nullptr && !uuidEquals(value.serviceUuid, serviceUuid)) continue;
    if (found++ == index)
    {
      characteristic = value;
      return true;
    }
  }
  return false;
}

size_t EspBle::discoveredDescriptorCount(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid) const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const auto *database = impl_->gattDatabase;
  if (database == nullptr || !database->valid || database->connectionId != connectionId) return 0;
  size_t count = 0;
  for (size_t index = 0; index < database->descriptorCount; ++index)
  {
    const auto &value = database->descriptors[index];
    if (serviceUuid != nullptr && !uuidEquals(value.serviceUuid, serviceUuid)) continue;
    if (characteristicUuid != nullptr &&
        !uuidEquals(value.characteristicUuid, characteristicUuid)) continue;
    ++count;
  }
  return count;
}

bool EspBle::discoveredDescriptor(
  EspBleConnectionId connectionId,
  size_t index,
  EspBleGattDescriptorInfo &descriptor,
  const char *serviceUuid,
  const char *characteristicUuid) const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const auto *database = impl_->gattDatabase;
  if (database == nullptr || !database->valid || database->connectionId != connectionId) return false;
  size_t found = 0;
  for (size_t candidate = 0; candidate < database->descriptorCount; ++candidate)
  {
    const auto &value = database->descriptors[candidate];
    if (serviceUuid != nullptr && !uuidEquals(value.serviceUuid, serviceUuid)) continue;
    if (characteristicUuid != nullptr &&
        !uuidEquals(value.characteristicUuid, characteristicUuid)) continue;
    if (found++ == index)
    {
      descriptor = value;
      return true;
    }
  }
  return false;
}

bool EspBle::readCharacteristic(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  uint32_t timeoutMilliseconds)
{
  return startGattOperation(
    EspBleGattOperation::Read, connectionId, serviceUuid, characteristicUuid,
    nullptr, 0, true, nullptr, timeoutMilliseconds);
}

bool EspBle::writeCharacteristic(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  const uint8_t *data,
  size_t length,
  bool response,
  uint32_t timeoutMilliseconds)
{
  return startGattOperation(
    EspBleGattOperation::Write,
    connectionId,
    serviceUuid,
    characteristicUuid,
    data,
    length,
    response,
    nullptr,
    timeoutMilliseconds);
}

bool EspBle::readDescriptor(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  const char *descriptorUuid,
  uint32_t timeoutMilliseconds)
{
  return startGattOperation(
    EspBleGattOperation::ReadDescriptor,
    connectionId,
    serviceUuid,
    characteristicUuid,
    nullptr,
    0,
    true,
    descriptorUuid,
    timeoutMilliseconds);
}

bool EspBle::writeDescriptor(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  const char *descriptorUuid,
  const uint8_t *data,
  size_t length,
  bool response,
  uint32_t timeoutMilliseconds)
{
  return startGattOperation(
    EspBleGattOperation::WriteDescriptor,
    connectionId,
    serviceUuid,
    characteristicUuid,
    data,
    length,
    response,
    descriptorUuid,
    timeoutMilliseconds);
}

bool EspBle::subscribe(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  bool notifications,
  uint32_t timeoutMilliseconds)
{
  return startGattOperation(
    EspBleGattOperation::Subscribe,
    connectionId,
    serviceUuid,
    characteristicUuid,
    nullptr,
    0,
    notifications,
    nullptr,
    timeoutMilliseconds);
}

bool EspBle::unsubscribe(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  uint32_t timeoutMilliseconds)
{
  return startGattOperation(
    EspBleGattOperation::Unsubscribe,
    connectionId,
    serviceUuid,
    characteristicUuid,
    nullptr,
    0,
    true,
    nullptr,
    timeoutMilliseconds);
}

bool EspBle::writeCharacteristic(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  const String &value,
  bool response,
  uint32_t timeoutMilliseconds)
{
  return writeCharacteristic(
    connectionId,
    serviceUuid,
    characteristicUuid,
    reinterpret_cast<const uint8_t *>(value.c_str()),
    value.length(),
    response,
    timeoutMilliseconds);
}

bool EspBle::writeDescriptor(
  EspBleConnectionId connectionId,
  const char *serviceUuid,
  const char *characteristicUuid,
  const char *descriptorUuid,
  const String &value,
  bool response,
  uint32_t timeoutMilliseconds)
{
  return writeDescriptor(
    connectionId,
    serviceUuid,
    characteristicUuid,
    descriptorUuid,
    reinterpret_cast<const uint8_t *>(value.c_str()),
    value.length(),
    response,
    timeoutMilliseconds);
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

void EspBle::onServicesDiscovered(GattResultCallback callback)
{
  servicesDiscoveredCallback_ = std::move(callback);
}

void EspBle::onDescriptorRead(GattResultCallback callback)
{
  descriptorReadCallback_ = std::move(callback);
}

void EspBle::onDescriptorWritten(GattResultCallback callback)
{
  descriptorWrittenCallback_ = std::move(callback);
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
  bool response,
  const char *descriptorUuid,
  uint32_t timeoutMilliseconds)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  const bool databaseDiscovery = operation == EspBleGattOperation::DiscoverServices;
  const bool descriptorOperation = operation == EspBleGattOperation::ReadDescriptor ||
    operation == EspBleGattOperation::WriteDescriptor;
  if ((!databaseDiscovery &&
       (serviceUuid == nullptr || serviceUuid[0] == '\0' ||
        characteristicUuid == nullptr || characteristicUuid[0] == '\0')) ||
      (descriptorOperation && (descriptorUuid == nullptr || descriptorUuid[0] == '\0')) ||
      (data == nullptr && length != 0) || timeoutMilliseconds == 0)
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
    impl_->gattServiceUuid = serviceUuid == nullptr ? "" : serviceUuid;
    impl_->gattCharacteristicUuid = characteristicUuid == nullptr ? "" : characteristicUuid;
    impl_->gattDescriptorUuid = descriptorUuid == nullptr ? "" : descriptorUuid;
    impl_->gattWriteValue = length == 0
      ? String()
      : String(reinterpret_cast<const char *>(data), length);
    impl_->gattWriteResponse = response;
    impl_->gattStartMilliseconds = millis();
    impl_->gattTimeoutMilliseconds = timeoutMilliseconds;
    impl_->gattTimedOut = false;
    if (databaseDiscovery && impl_->gattDatabase != nullptr)
    {
      impl_->gattDatabase->reset(connectionId);
    }
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
    if (impl_->gattOperating)
    {
      impl_->gattTask = task;
    }
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
      if (event.connection.localRole == EspBleRole::Peripheral)
      {
        bool hasPeripheral = false;
        {
          std::lock_guard<std::mutex> lock(impl_->mutex);
          for (const EspBleImpl::ConnectionSlot &slot : impl_->connections)
            hasPeripheral = hasPeripheral || (slot.used && slot.connection.localRole == EspBleRole::Peripheral);
        }
        if (!hasPeripheral)
        {
          hidMouse_.buttons_ = 0;
          hidConsumerControl_.usage_ = 0;
          hidSystemControl_.usage_ = 0;
          if (hidKeyboardDevice_.impl_ != nullptr)
          {
            std::lock_guard<std::mutex> lock(hidKeyboardDevice_.impl_->mutex);
            memset(hidKeyboardDevice_.impl_->inputValues, 0,
                   sizeof(hidKeyboardDevice_.impl_->inputValues));
          }
        }
      }
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
      case EspBleGattOperation::DiscoverServices:
        callback = &servicesDiscoveredCallback_;
        break;
      case EspBleGattOperation::ReadDescriptor:
        callback = &descriptorReadCallback_;
        break;
      case EspBleGattOperation::WriteDescriptor:
        callback = &descriptorWrittenCallback_;
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
    case EspBleImpl::EventType::ServerDescriptorWrite:
    {
      EspBleGattDescriptorWrite write;
      write.serviceUuid = event.serverWrite.serviceUuid;
      write.characteristicUuid = event.serverWrite.characteristicUuid;
      write.descriptorUuid = event.serverDescriptorUuid;
      write.value = event.serverWrite.value;
      gattServer_.dispatchDescriptorWrite(write);
      break;
    }
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

EspBleHidKeyboard &EspBle::hidKeyboard() { return hidKeyboardDevice_; }
EspBleHidMouse &EspBle::hidMouse() { return hidMouse_; }
EspBleHidConsumerControl &EspBle::hidConsumerControl() { return hidConsumerControl_; }
EspBleHidSystemControl &EspBle::hidSystemControl() { return hidSystemControl_; }
EspBleHidGamepad &EspBle::hidGamepad() { return hidGamepad_; }
EspBleHidVendor &EspBle::hidVendor() { return hidVendor_; }
EspBleHidHost &EspBle::hidHost() { return hidKeyboardHost_; }

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
  case EspBleError::Timeout:
    return "TIMEOUT";
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
