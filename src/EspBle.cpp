#include "EspBle.h"

#include <esp_bt.h>

// The Arduino core releases the BLE controller's memory (~36 KB) before setup()
// runs unless a BLE library announces itself; the constructor in this header is
// how a library does that. Without it esp_bt_controller_init() crashes inside
// its own cleanup path, which is what happens the moment nothing links the
// bundled BLE wrapper any more.
#if __has_include("esp32-hal-alloc-ble-mem.h")
#include "esp32-hal-alloc-ble-mem.h"
#else
// Older cores decide with this weak hook instead.
extern "C" bool bleInUse(void)
{
  return true;
}
#endif

#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
// The NVS-backed bond store. Its initialiser has no public header in the
// ESP-IDF build, so it is declared the way the IDF's own examples do.
extern "C" void ble_store_config_init(void);
#include <host/ble_sm.h>
#include <host/ble_uuid.h>
#include <host/ble_hs_id.h>
#include <host/ble_hs_mbuf.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <host/ble_store.h>
#include <os/os_mbuf.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cctype>
#include <cinttypes>
#include <cstring>
#include <map>
#include <mutex>
#include <new>
#include <utility>

#include "EspBleHidReportMap.h"
#include "EspBleUuid.h"


namespace
{
constexpr size_t ScanQueueCapacity = 16;
// Number of connection slots the library tracks. The number of *simultaneous*
// connections is ultimately capped by the bundled NimBLE controller
// (CONFIG_BT_NIMBLE_MAX_CONNECTIONS, 3 on the precompiled esp32s3 build); a
// request beyond that fails at the backend even though a slot is free.
constexpr size_t ConnectionCapacity = 4;
constexpr size_t ConnectionEventQueueCapacity = 8;
constexpr uint16_t HidKeyboardAppearance = 0x03c1;
#if defined(CONFIG_BT_NIMBLE_MAX_BONDS)
constexpr size_t BondCapacity = CONFIG_BT_NIMBLE_MAX_BONDS;
#else
constexpr size_t BondCapacity = 16;
#endif

// NimBLE host bring-up. The host runs on its own FreeRTOS task and reports
// when the controller is ready; nothing may touch GAP before that.
volatile bool hostSynced = false;

void onHostSync()
{
  // Make sure this device has an identity address (public if the chip has one,
  // random static otherwise): every GAP procedure needs one.
  ble_hs_util_ensure_addr(0);
  hostSynced = true;
}

void onHostReset(int)
{
  // The controller restarted; sync_cb runs again once it is back.
  hostSynced = false;
}

void hostTask(void *)
{
  nimble_port_run(); // returns only once nimble_port_stop() has been called
  nimble_port_freertos_deinit();
}

// Returns false if the controller never reports in.
bool startNimbleHost()
{
  if (hostSynced) return true;
  if (nimble_port_init() != ESP_OK) return false;
  ble_hs_cfg.reset_cb = onHostReset;
  ble_hs_cfg.sync_cb = onHostSync;
  // Bonds live in NVS, so they survive a reboot; this is the store that does it.
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ble_store_config_init();
  nimble_port_freertos_init(hostTask);

  const uint32_t deadline = millis() + 5000;
  while (!hostSynced)
  {
    if (static_cast<int32_t>(millis() - deadline) >= 0) return false;
    delay(1);
  }
  return true;
}

void stopNimbleHost()
{
  nimble_port_stop();
  nimble_port_deinit();
  hostSynced = false;
}

// AD types (Core Specification Supplement, Part A). The builder writes the
// "complete" list variants; the parser also accepts the "incomplete" ones,
// which carry the same values and differ only in whether the advertiser
// promised to list everything.
constexpr uint8_t AdTypeFlags = 0x01;
constexpr uint8_t AdTypeServiceUuids16Partial = 0x02;
constexpr uint8_t AdTypeServiceUuids16 = 0x03;
constexpr uint8_t AdTypeServiceUuids32Partial = 0x04;
constexpr uint8_t AdTypeServiceUuids32 = 0x05;
constexpr uint8_t AdTypeServiceUuids128Partial = 0x06;
constexpr uint8_t AdTypeServiceUuids128 = 0x07;
constexpr uint8_t AdTypeShortenedLocalName = 0x08;
constexpr uint8_t AdTypeCompleteLocalName = 0x09;
constexpr uint8_t AdTypeTxPowerLevel = 0x0a;
constexpr uint8_t AdTypeServiceData16 = 0x16;
constexpr uint8_t AdTypeAppearance = 0x19;
constexpr uint8_t AdTypeServiceData32 = 0x20;
constexpr uint8_t AdTypeServiceData128 = 0x21;
constexpr uint8_t AdTypeManufacturerData = 0xff;

// UUID conversions between the library's text form and the stack's types. The
// text codec itself lives in EspBleUuid.h so it can be unit tested on the host.
bool parseUuid(const char *text, ble_uuid_any_t &out)
{
  EspBleUuidValue value;
  if (!espBleParseUuid(text, value)) return false;
  memset(&out, 0, sizeof(out));
  if (value.bitSize == 16)
  {
    out.u.type = BLE_UUID_TYPE_16;
    out.u16.value = static_cast<uint16_t>(espBleUuidShortValue(value));
  }
  else if (value.bitSize == 32)
  {
    out.u.type = BLE_UUID_TYPE_32;
    out.u32.value = espBleUuidShortValue(value);
  }
  else
  {
    out.u.type = BLE_UUID_TYPE_128;
    memcpy(out.u128.value, value.bytes, 16);
  }
  return true;
}

EspBleUuidValue uuidValueOf(const ble_uuid_t *uuid)
{
  EspBleUuidValue value;
  if (uuid == nullptr) return value;
  char text[16];
  if (uuid->type == BLE_UUID_TYPE_16)
  {
    snprintf(text, sizeof(text), "%04x", reinterpret_cast<const ble_uuid16_t *>(uuid)->value);
    espBleParseUuid(text, value);
    return value;
  }
  if (uuid->type == BLE_UUID_TYPE_32)
  {
    snprintf(
      text, sizeof(text), "%08" PRIx32, reinterpret_cast<const ble_uuid32_t *>(uuid)->value);
    espBleParseUuid(text, value);
    return value;
  }
  memcpy(value.bytes, reinterpret_cast<const ble_uuid128_t *>(uuid)->value, 16);
  value.bitSize = 128;
  return value;
}

// Always the 128-bit form: a 16-bit UUID expanded onto the Bluetooth base UUID.
// Every EspBle surface reports UUIDs this way, so one spelling is comparable
// everywhere (NimBLE's own ble_uuid_to_str() would print "0x180f").
String uuidToString(const ble_uuid_t *uuid)
{
  if (uuid == nullptr) return String();
  char text[37];
  espBleFormatUuid(uuidValueOf(uuid), text, sizeof(text));
  return String(text);
}

String uuidToString(const ble_uuid_any_t &uuid)
{
  return uuidToString(&uuid.u);
}

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
  EspBleUuidValue leftValue;
  EspBleUuidValue rightValue;
  if (!espBleParseUuid(left.c_str(), leftValue) || !espBleParseUuid(right, rightValue))
  {
    return false;
  }
  return espBleUuidEquals(leftValue, rightValue);
}

// "aa:bb:cc:dd:ee:ff", most significant byte first -- the spelling the whole
// API uses. ble_addr_t::val holds the bytes the other way round.
String formatAddress(const uint8_t value[6])
{
  char text[18];
  snprintf(
    text, sizeof(text), "%02x:%02x:%02x:%02x:%02x:%02x",
    value[5], value[4], value[3], value[2], value[1], value[0]);
  return String(text);
}

bool parseAddress(const char *text, uint8_t out[6])
{
  if (text == nullptr || strlen(text) != 17) return false;
  for (size_t index = 0; index < 6; ++index)
  {
    const size_t position = index * 3;
    if (index != 0 && text[position - 1] != ':') return false;
    const int high = espBleHexDigitValue(text[position]);
    const int low = espBleHexDigitValue(text[position + 1]);
    if (high < 0 || low < 0) return false;
    out[5 - index] = static_cast<uint8_t>((high << 4) | low);
  }
  return true;
}

// Raw bytes as a String. Arduino's String is length-based, so an embedded 0x00
// survives -- only c_str() readers stop early, which is the caller's business.
String stringFromBytes(const uint8_t *bytes, size_t length)
{
  String value;
  value.reserve(length);
  for (size_t index = 0; index < length; ++index)
  {
    value.concat(static_cast<char>(bytes[index]));
  }
  return value;
}

String uuidTextFromLittleEndian(const uint8_t *bytes, size_t length)
{
  EspBleUuidValue value;
  if (!espBleUuidFromLittleEndian(bytes, length, value)) return String();
  char text[37];
  espBleFormatUuid(value, text, sizeof(text));
  return String(text);
}

// One advertising report's AD structures, merged into result. An advertisement
// and its scan response are two reports describing one device, so a field is
// only written when this report carries it.
void parseAdvertisingReport(const uint8_t *data, size_t length, EspBleScanResult &result)
{
  if (data == nullptr) return;
  size_t offset = 0;
  while (offset + 1 < length)
  {
    const size_t fieldLength = data[offset];
    // A zero length is the padding that fills the rest of the report.
    if (fieldLength == 0) break;
    if (offset + 1 + fieldLength > length) break; // truncated report
    const uint8_t type = data[offset + 1];
    const uint8_t *value = data + offset + 2;
    const size_t valueLength = fieldLength - 1;
    offset += 1 + fieldLength;

    switch (type)
    {
    case AdTypeShortenedLocalName:
    case AdTypeCompleteLocalName:
    {
      char text[32];
      const size_t copied = valueLength < sizeof(text) - 1 ? valueLength : sizeof(text) - 1;
      memcpy(text, value, copied);
      text[copied] = '\0';
      result.name = text;
      break;
    }
    case AdTypeServiceUuids16Partial:
    case AdTypeServiceUuids16:
    case AdTypeServiceUuids32Partial:
    case AdTypeServiceUuids32:
    case AdTypeServiceUuids128Partial:
    case AdTypeServiceUuids128:
    {
      const size_t uuidSize =
        (type == AdTypeServiceUuids16Partial || type == AdTypeServiceUuids16) ? 2
        : (type == AdTypeServiceUuids32Partial || type == AdTypeServiceUuids32) ? 4
                                                                                : 16;
      for (size_t position = 0; position + uuidSize <= valueLength; position += uuidSize)
      {
        if (result.serviceUuidCount == EspBleScanResult::MaxServiceUuids) break;
        const String uuid = uuidTextFromLittleEndian(value + position, uuidSize);
        if (uuid.isEmpty()) continue;
        bool known = false;
        for (size_t index = 0; index < result.serviceUuidCount; ++index)
        {
          if (result.serviceUuids[index] == uuid)
          {
            known = true;
            break;
          }
        }
        // The advertisement and the scan response may list the same UUID.
        if (!known) result.serviceUuids[result.serviceUuidCount++] = uuid;
      }
      break;
    }
    case AdTypeServiceData16:
    case AdTypeServiceData32:
    case AdTypeServiceData128:
    {
      const size_t uuidSize = type == AdTypeServiceData16 ? 2 : (type == AdTypeServiceData32 ? 4 : 16);
      if (valueLength < uuidSize) break;
      if (result.serviceDataCount == EspBleScanResult::MaxServiceData) break;
      const String uuid = uuidTextFromLittleEndian(value, uuidSize);
      if (uuid.isEmpty()) break;
      EspBleServiceData &block = result.serviceData[result.serviceDataCount++];
      block.uuid = uuid;
      block.data = stringFromBytes(value + uuidSize, valueLength - uuidSize);
      break;
    }
    case AdTypeManufacturerData:
      result.manufacturerData = stringFromBytes(value, valueLength);
      break;
    case AdTypeAppearance:
      if (valueLength >= 2)
      {
        result.appearance = static_cast<uint16_t>(value[0] | (value[1] << 8));
      }
      break;
    case AdTypeTxPowerLevel:
      if (valueLength >= 1)
      {
        // 0 dBm is a legal level, so presence needs its own flag.
        result.txPowerLevel = static_cast<int8_t>(value[0]);
        result.txPowerLevelPresent = true;
      }
      break;
    default:
      // Flags and anything else this library does not surface.
      break;
    }
  }
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

// Raw GATT client discovery, bypassing the bundled wrapper.
//
// The wrapper keys remote services by UUID (BLEClient::m_servicesMap), so a peer
// exposing the same service UUID twice loses every instance after the first --
// they are unreachable, and the BLERemoteService objects leak. Discovering
// straight through the NimBLE host API keeps every instance and every
// characteristic, identified by attribute handle as the spec intends.
namespace espble_discovery
{
struct ServiceRange
{
  uint16_t startHandle = 0;
  uint16_t endHandle = 0;
  String uuid;
};

struct CharacteristicEntry
{
  uint16_t definitionHandle = 0;
  uint16_t valueHandle = 0;
  uint8_t properties = 0;
  String uuid;
};

// One discovery procedure in flight. NimBLE reports results on the host task,
// so the worker task that started it waits for `done`.
//
// Each phase keeps its own context type and the caller heap-allocates it: a
// single context holding all three arrays is several kilobytes, and the three
// nested would overflow the GATT worker task's stack.
struct Waiter
{
  volatile bool done = false;
  int status = 0;
  bool overflowed = false;
};

struct ServiceContext : Waiter
{
  ServiceRange services[EspBle::MaxDiscoveredGattServices];
  size_t count = 0;
};

struct CharacteristicContext : Waiter
{
  CharacteristicEntry characteristics[EspBle::MaxDiscoveredGattCharacteristics];
  size_t count = 0;
};

struct DescriptorContext : Waiter
{
  EspBleGattDescriptorInfo descriptors[EspBle::MaxDiscoveredGattDescriptors];
  size_t count = 0;
};

using ::uuidToString;

inline int serviceCallback(
  uint16_t connectionHandle, const struct ble_gatt_error *error,
  const struct ble_gatt_svc *service, void *argument)
{
  ServiceContext *context = static_cast<ServiceContext *>(argument);
  if (error != nullptr && error->status != 0)
  {
    // BLE_HS_EDONE is the normal end of the procedure, not a failure.
    context->status = error->status == BLE_HS_EDONE ? 0 : error->status;
    context->done = true;
    return 0;
  }
  if (service == nullptr) return 0;
  if (context->count == EspBle::MaxDiscoveredGattServices)
  {
    context->overflowed = true;
    return 0;
  }
  ServiceRange &entry = context->services[context->count++];
  entry.startHandle = service->start_handle;
  entry.endHandle = service->end_handle;
  entry.uuid = uuidToString(service->uuid);
  return 0;
}

inline int characteristicCallback(
  uint16_t connectionHandle, const struct ble_gatt_error *error,
  const struct ble_gatt_chr *characteristic, void *argument)
{
  CharacteristicContext *context = static_cast<CharacteristicContext *>(argument);
  if (error != nullptr && error->status != 0)
  {
    context->status = error->status == BLE_HS_EDONE ? 0 : error->status;
    context->done = true;
    return 0;
  }
  if (characteristic == nullptr) return 0;
  if (context->count == EspBle::MaxDiscoveredGattCharacteristics)
  {
    context->overflowed = true;
    return 0;
  }
  CharacteristicEntry &entry = context->characteristics[context->count++];
  entry.definitionHandle = characteristic->def_handle;
  entry.valueHandle = characteristic->val_handle;
  entry.properties = characteristic->properties;
  entry.uuid = uuidToString(characteristic->uuid);
  return 0;
}

inline int descriptorCallback(
  uint16_t connectionHandle, const struct ble_gatt_error *error,
  uint16_t characteristicValueHandle, const struct ble_gatt_dsc *descriptor,
  void *argument)
{
  DescriptorContext *context = static_cast<DescriptorContext *>(argument);
  if (error != nullptr && error->status != 0)
  {
    context->status = error->status == BLE_HS_EDONE ? 0 : error->status;
    context->done = true;
    return 0;
  }
  if (descriptor == nullptr) return 0;
  if (context->count == EspBle::MaxDiscoveredGattDescriptors)
  {
    context->overflowed = true;
    return 0;
  }
  EspBleGattDescriptorInfo &info = context->descriptors[context->count++];
  info.descriptorUuid = uuidToString(descriptor->uuid);
  info.handle = descriptor->handle;
  return 0;
}

// Block until the in-flight procedure reports completion. Returns false on
// timeout so a silent stall cannot hang the worker task forever.
inline bool wait(Waiter &context, uint32_t timeoutMilliseconds)
{
  const uint32_t deadline = millis() + timeoutMilliseconds;
  while (!context.done)
  {
    if (millis() >= deadline) return false;
    delay(1);
  }
  return true;
}
} // namespace espble_discovery

// Raw ATT operations for attributes the bundled wrapper cannot resolve. Its
// remote-service map is keyed by UUID, so characteristics belonging to a second
// service that repeats a UUID have no BLERemoteCharacteristic to go through;
// these talk to the peer by attribute handle directly.
namespace espble_raw
{
struct Operation
{
  volatile bool done = false;
  int status = 0;
  String value;
  // Task to wake when the host reports completion, so the worker resumes at
  // once instead of on its next poll.
  TaskHandle_t waiter = nullptr;
  // Points at the ordering flag the worker publishes its completion through.
  // Raised on the host task the moment this operation completes, so a
  // notification the same operation triggered cannot be delivered ahead of it.
  volatile bool *completionPending = nullptr;
};

inline void reportCompletion(Operation *operation)
{
  if (operation->completionPending != nullptr) *operation->completionPending = true;
  operation->done = true;
  if (operation->waiter != nullptr) xTaskNotifyGive(operation->waiter);
}

inline int readCallback(
  uint16_t connectionHandle, const struct ble_gatt_error *error,
  struct ble_gatt_attr *attribute, void *argument)
{
  Operation *operation = static_cast<Operation *>(argument);
  operation->status = error == nullptr ? 0 : error->status;
  if (operation->status == 0 && attribute != nullptr && attribute->om != nullptr)
  {
    const uint16_t length = OS_MBUF_PKTLEN(attribute->om);
    if (length > 0)
    {
      char *buffer = new (std::nothrow) char[length];
      if (buffer != nullptr)
      {
        uint16_t copied = 0;
        if (ble_hs_mbuf_to_flat(attribute->om, buffer, length, &copied) == 0)
        {
          operation->value = String(buffer, copied);
        }
        delete[] buffer;
      }
    }
  }
  reportCompletion(operation);
  return 0;
}

// Read Long: the value arrives in fragments, one callback each, ending with
// BLE_HS_EDONE. A plain read stops at one MTU, which silently truncates
// anything longer -- a HID Report Map, for instance.
inline int readLongCallback(
  uint16_t connectionHandle, const struct ble_gatt_error *error,
  struct ble_gatt_attr *attribute, void *argument)
{
  Operation *operation = static_cast<Operation *>(argument);
  const int status = error == nullptr ? 0 : error->status;
  if (status == BLE_HS_EDONE)
  {
    operation->status = 0;
    reportCompletion(operation);
    return 0;
  }
  if (status != 0)
  {
    operation->status = status;
    reportCompletion(operation);
    return 0;
  }
  if (attribute != nullptr && attribute->om != nullptr)
  {
    const uint16_t length = OS_MBUF_PKTLEN(attribute->om);
    if (length > 0)
    {
      char *buffer = new (std::nothrow) char[length];
      if (buffer != nullptr)
      {
        uint16_t copied = 0;
        if (ble_hs_mbuf_to_flat(attribute->om, buffer, length, &copied) == 0)
        {
          operation->value.concat(buffer, copied);
        }
        delete[] buffer;
      }
    }
  }
  return 0;
}

inline int writeCallback(
  uint16_t connectionHandle, const struct ble_gatt_error *error,
  struct ble_gatt_attr *attribute, void *argument)
{
  Operation *operation = static_cast<Operation *>(argument);
  operation->status = error == nullptr ? 0 : error->status;
  reportCompletion(operation);
  return 0;
}

// Block until the host reports completion. Returns false on timeout so a
// silent stall cannot hang the worker task forever. The wake-up is a task
// notification with a bounded wait, so a completion that lands before this
// call (notifications latch) is not missed either.
inline bool wait(Operation &operation, uint32_t timeoutMilliseconds)
{
  const uint32_t deadline = millis() + timeoutMilliseconds;
  while (!operation.done)
  {
    const uint32_t now = millis();
    if (now >= deadline) return false;
    const uint32_t remaining = deadline - now;
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(remaining > 20 ? 20 : remaining));
  }
  return true;
}

// One read, blocking the calling task until the peer answers. For callers that
// hold a handle already and want the value, not an event. Reads the whole
// value, however long: the host only asks for more once a fragment comes back
// full, so a short value costs the same single request as a plain read.
inline bool readHandle(
  uint16_t connectionHandle,
  uint16_t attributeHandle,
  String &value,
  uint32_t timeoutMilliseconds = 10000)
{
  Operation operation;
  operation.waiter = xTaskGetCurrentTaskHandle();
  if (ble_gattc_read_long(
        connectionHandle, attributeHandle, 0, readLongCallback, &operation) != 0 ||
      !wait(operation, timeoutMilliseconds) || operation.status != 0)
  {
    return false;
  }
  value = operation.value;
  return true;
}

// One write. Without a response there is nothing to wait for and no status to
// report, so success only means the request was accepted for transmission.
inline bool writeHandle(
  uint16_t connectionHandle,
  uint16_t attributeHandle,
  const uint8_t *data,
  size_t length,
  bool response,
  uint32_t timeoutMilliseconds = 10000)
{
  if (!response)
  {
    return ble_gattc_write_no_rsp_flat(connectionHandle, attributeHandle, data, length) == 0;
  }
  Operation operation;
  operation.waiter = xTaskGetCurrentTaskHandle();
  return ble_gattc_write_flat(
           connectionHandle, attributeHandle, data, length, writeCallback, &operation) == 0 &&
    wait(operation, timeoutMilliseconds) && operation.status == 0;
}
} // namespace espble_raw



// Complete a connection-scoped indication whose confirmation just arrived.
// Defined once EspBleGattServerImpl is complete; declared here because the GAP
// event listener above needs it.
void espBleConfirmIndication(
  EspBle *owner, uint16_t connectionHandle, uint16_t attributeHandle, int status);

// Track a peer turning Notification/Indication on or off for one of the local
// server's characteristics, and drop its state when the connection ends. Both
// are reached from the global GAP listener; defined once the server impl is.
void espBleHandleServerSubscribe(
  EspBle *owner, uint16_t connectionHandle, uint16_t attributeHandle, bool notifications,
  bool indications);
void espBleForgetServerSubscriptions(EspBle *owner, uint16_t connectionHandle);


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
    ConnParamsUpdated,
    PhyUpdated,
    SecurityChanged,
    PasskeyDisplayed,
    NumericComparison,
  };

  struct ConnectionSlot
  {
    bool used = false;
    EspBleConnection connection;
    // Set when disconnect() is called while a GATT op is in flight on this
    // connection; EspBle::drainPendingDisconnects() performs the disconnect once
    // the op completes, instead of rejecting the disconnect() call.
    bool pendingDisconnect = false;
    uint8_t pendingDisconnectReason = 0x13;
  };

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
    EspBleGattDescriptor serverDescriptor;
    EspBleGattNotification notification;
    EspBleGattSubscription serverSubscription;
    EspBleGattSendResult serverSendResult;
    EspBleMtuChanged mtuChanged;
    EspBleSecurityChanged securityChanged;
    EspBlePasskeyDisplayed passkeyDisplayed;
  };

  // GAP callback for connections this device initiates. It owns the central
  // side of a connection's life cycle, the way advertisingGapEvent() owns the
  // peripheral side; the global listener sees these events too and handles
  // everything that is the same for both roles.
  static int centralGapEvent(ble_gap_event *event, void *argument)
  {
    EspBleImpl *impl = static_cast<EspBleImpl *>(argument);
    if (event->type == BLE_GAP_EVENT_CONNECT)
    {
      EspBleScanResult target;
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        target = impl->connectTarget;
      }
      if (event->connect.status != 0)
      {
        impl->reportConnectFailure(target, event->connect.status == BLE_HS_ETIMEOUT);
        return 0;
      }
      ble_gap_conn_desc description{};
      if (ble_gap_conn_find(event->connect.conn_handle, &description) != 0)
      {
        impl->reportConnectFailure(target, false);
        return 0;
      }
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->connecting = false;
        impl->connectCancelRequested = false;
      }
      if (!impl->addConnection(
            event->connect.conn_handle,
            formatAddress(description.peer_ota_addr.val),
            static_cast<EspBleAddressType>(description.peer_ota_addr.type),
            EspBleRole::Central,
            ble_att_mtu(event->connect.conn_handle),
            description.sec_state.encrypted,
            description.sec_state.authenticated,
            description.sec_state.bonded,
            description.sec_state.key_size))
      {
        // No slot left: drop the connection rather than hold one the
        // application can never see.
        ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
      }
      // The central is the side that asks for a larger ATT MTU. The answer
      // arrives as BLE_GAP_EVENT_MTU a moment later, which is where the
      // connection's mtu is updated and onMtuChanged() fires.
      ble_gattc_exchange_mtu(event->connect.conn_handle, nullptr, nullptr);
      if (impl->securityEnabled && impl->pairOnConnect && !description.sec_state.encrypted)
      {
        ble_gap_security_initiate(event->connect.conn_handle);
      }
      return 0;
    }
    if (event->type == BLE_GAP_EVENT_PASSKEY_ACTION)
    {
      // Pairing input is answered from the connection's own callback: the
      // global listener never sees this event.
      impl->handlePasskeyAction(event->passkey.conn_handle, event->passkey.params);
      return 0;
    }
    if (event->type == BLE_GAP_EVENT_DISCONNECT)
    {
      // An attempt that never succeeded ends here too: the bundled NimBLE
      // re-attempts a failed connection a few times of its own accord
      // (BLE_GAP_EVENT_REATTEMPT_COUNT) and then reports a disconnect rather
      // than a failed connect.
      if (!impl->removeConnectionByHandle(event->disconnect.conn.conn_handle))
      {
        EspBleScanResult target;
        {
          std::lock_guard<std::mutex> lock(impl->mutex);
          target = impl->connectTarget;
        }
        impl->reportConnectFailure(target, false);
      }
    }
    return 0;
  }

  // Close out the in-flight attempt. Called from whichever event ended it;
  // only the first one reports, since a cancel may be followed by a disconnect.
  void reportConnectFailure(const EspBleScanResult &target, bool timedOut)
  {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (!connecting) return;
      connecting = false;
      timedOut = timedOut || connectCancelRequested;
      connectCancelRequested = false;
    }
    pushFailure(
      target, timedOut ? "BLE connection timed out" : "BLE connection failed",
      timedOut ? EspBleError::Timeout : EspBleError::BackendFailure);
  }

  explicit EspBleImpl(EspBle *owner)
      : owner(owner)
  {
  }

  ~EspBleImpl()
  {
    for (GattDatabaseSnapshot *database : gattDatabases) delete database;
  }

  // EspBleImpl is a friend of EspBle and EspBleGattServer, so it can hand the
  // server's implementation to the free functions in this file.
  static EspBleGattServerImpl *serverImplOf(EspBle *owner)
  {
    return owner->gattServer_.impl_;
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
    uint8_t encryptionKeySize)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (slot.used)
      {
        continue;
      }

      slot.used = true;
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
      ble_gap_conn_desc paramsDesc{};
      if (ble_gap_conn_find(handle, &paramsDesc) == 0)
      {
        slot.connection.connectionInterval = paramsDesc.conn_itvl;
        slot.connection.peripheralLatency = paramsDesc.conn_latency;
        slot.connection.supervisionTimeout = paramsDesc.supervision_timeout;
      }
      uint8_t txPhy = 0;
      uint8_t rxPhy = 0;
      if (ble_gap_read_le_phy(handle, &txPhy, &rxPhy) == 0)
      {
        slot.connection.txPhy = txPhy;
        slot.connection.rxPhy = rxPhy;
      }
      {
        // Start each connection's Passkey Entry state clean so a value left over
        // from an earlier aborted pairing is never consumed by a new one.
        std::lock_guard<std::mutex> passkeyLock(passkeyMutex);
        passkeyProvided = false;
        numericComparisonConfirmed = false;
      }

      if (localRole == EspBleRole::Central)
      {
        // Track this peer so auto-reconnect (if enabled) restores it on a drop.
        rememberDesiredLocked(peerAddress, peerAddressType);
      }

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

  // Either role: the connection is gone, whoever started it. False when no
  // slot held that handle.
  bool removeConnectionByHandle(uint16_t handle)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.connection.handle == handle)
      {
        removeConnection(slot);
        return true;
      }
    }
    return false;
  }


  // GAP callback for advertising we started ourselves. It owns the peripheral
  // side of a connection's life cycle; everything else about the connection
  // (MTU, PHY, security, indication confirmations) arrives on the global
  // listener, which sees these events too.
  static int advertisingGapEvent(ble_gap_event *event, void *argument)
  {
    EspBleImpl *impl = static_cast<EspBleImpl *>(argument);
    if (event->type == BLE_GAP_EVENT_CONNECT)
    {
      if (event->connect.status != 0) return 0; // the attempt failed; nothing was set up
      ble_gap_conn_desc description{};
      if (ble_gap_conn_find(event->connect.conn_handle, &description) != 0) return 0;
      if (!impl->addConnection(
            event->connect.conn_handle,
            formatAddress(description.peer_ota_addr.val),
            static_cast<EspBleAddressType>(description.peer_ota_addr.type),
            EspBleRole::Peripheral,
            ble_att_mtu(event->connect.conn_handle),
            description.sec_state.encrypted,
            description.sec_state.authenticated,
            description.sec_state.bonded,
            description.sec_state.key_size))
      {
        // No slot left: refuse the connection rather than hold one the
        // application can never see.
        ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
      }
      if (impl->securityEnabled && impl->pairOnConnect && !description.sec_state.encrypted)
      {
        ble_gap_security_initiate(event->connect.conn_handle);
      }
      return 0;
    }
    if (event->type == BLE_GAP_EVENT_DISCONNECT)
    {
      impl->removeConnectionByHandle(event->disconnect.conn.conn_handle);
    }
    else if (event->type == BLE_GAP_EVENT_PASSKEY_ACTION)
    {
      // Pairing input is answered from the connection's own callback: the
      // global listener is not guaranteed to see this event.
      impl->handlePasskeyAction(event->passkey.conn_handle, event->passkey.params);
    }
    return 0;
  }

  static int gapEventListenerEntry(ble_gap_event *event, void *argument)
  {
    EspBleImpl *impl = static_cast<EspBleImpl *>(argument);
    if (event->type == BLE_GAP_EVENT_DISCONNECT)
    {
      impl->stampDisconnectReason(
        event->disconnect.conn.conn_handle, event->disconnect.reason);
      espBleForgetServerSubscriptions(impl->owner, event->disconnect.conn.conn_handle);
    }
    else if (event->type == BLE_GAP_EVENT_ENC_CHANGE)
    {
      ble_gap_conn_desc description{};
      if (ble_gap_conn_find(event->enc_change.conn_handle, &description) == 0)
      {
        impl->updateSecurity(event->enc_change.conn_handle, description.sec_state);
      }
    }
    else if (event->type == BLE_GAP_EVENT_SUBSCRIBE)
    {
      // CCCD change for a local characteristic. The host reports the current
      // state, so this is both subscribe and unsubscribe.
      espBleHandleServerSubscribe(
        impl->owner, event->subscribe.conn_handle, event->subscribe.attr_handle,
        event->subscribe.cur_notify != 0, event->subscribe.cur_indicate != 0);
    }
    else if (event->type == BLE_GAP_EVENT_CONN_UPDATE)
    {
      impl->handleConnParamsUpdate(event->conn_update.conn_handle);
    }
    else if (event->type == BLE_GAP_EVENT_PHY_UPDATE_COMPLETE)
    {
      impl->handlePhyUpdate(
        event->phy_updated.conn_handle, event->phy_updated.tx_phy, event->phy_updated.rx_phy);
    }
    else if (event->type == BLE_GAP_EVENT_MTU)
    {
      // Fires on both roles when the ATT MTU is exchanged, letting a central
      // track the post-connect MTU the BLEClient callbacks do not surface.
      impl->updateMtu(event->mtu.conn_handle, event->mtu.value);
    }
    else if (event->type == BLE_GAP_EVENT_NOTIFY_TX)
    {
      // Confirmation for a connection-scoped indication sent with
      // ble_gatts_indicate_custom(). status is 0 once the peer confirms, and
      // BLE_HS_EDONE marks the intermediate "sent" report, which is not the
      // confirmation and must not complete the wait.
      if (event->notify_tx.indication && event->notify_tx.status != BLE_HS_EDONE)
      {
        espBleConfirmIndication(
          impl->owner, event->notify_tx.conn_handle, event->notify_tx.attr_handle,
          event->notify_tx.status);
      }
    }
    else if (event->type == BLE_GAP_EVENT_NOTIFY_RX)
    {
      // Only for attributes subscribed over the raw ATT path: the wrapper
      // delivers everything it knows through its own characteristic callback,
      // and matching on the subscription table keeps the two from doubling up.
      // The host sends the ATT confirmation for an indication itself.
      const uint16_t length =
        event->notify_rx.om == nullptr ? 0 : OS_MBUF_PKTLEN(event->notify_rx.om);
      if (length == 0)
      {
        impl->queueClientNotification(
          event->notify_rx.conn_handle, event->notify_rx.attr_handle, nullptr, 0,
          event->notify_rx.indication != 0);
      }
      else
      {
        std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[length]);
        uint16_t copied = 0;
        if (buffer != nullptr &&
            ble_hs_mbuf_to_flat(event->notify_rx.om, buffer.get(), length, &copied) == 0)
        {
          impl->queueClientNotification(
            event->notify_rx.conn_handle, event->notify_rx.attr_handle, buffer.get(), copied,
            event->notify_rx.indication != 0);
        }
      }
    }
    return 0;
  }

  void stampDisconnectReason(uint16_t handle, int reason)
  {
    // The backend reports HCI-originated reasons in its own error space
    // (0x0200 + the HCI code). Normalise them back to the plain HCI code, so a
    // reason read here matches the one passed to disconnect(). Values outside
    // that range are host-level errors and are passed through unchanged.
    if (reason >= 0x0200 && reason <= 0x02ff)
    {
      reason -= 0x0200;
    }
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.connection.handle == handle)
      {
        slot.connection.disconnectReason = reason;
        return;
      }
    }
  }

  void handleConnParamsUpdate(uint16_t handle)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.connection.handle == handle)
      {
        ble_gap_conn_desc desc{};
        if (ble_gap_conn_find(handle, &desc) == 0)
        {
          slot.connection.connectionInterval = desc.conn_itvl;
          slot.connection.peripheralLatency = desc.conn_latency;
          slot.connection.supervisionTimeout = desc.supervision_timeout;
        }
        Event event;
        event.type = EventType::ConnParamsUpdated;
        event.connection = slot.connection;
        pushEvent(event);
        return;
      }
    }
  }

  void handlePhyUpdate(uint16_t handle, uint8_t txPhy, uint8_t rxPhy)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.connection.handle == handle)
      {
        slot.connection.txPhy = txPhy;
        slot.connection.rxPhy = rxPhy;
        Event event;
        event.type = EventType::PhyUpdated;
        event.connection = slot.connection;
        pushEvent(event);
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
    releaseDatabaseLocked(slot.connection.id);
    purgeQueuedGattOpsLocked(slot.connection.id);
    forgetSubscribedHandlesLocked(slot.connection.id);
    slot = ConnectionSlot();
  }

  // Drop queued (not-yet-started) GATT ops for a connection that is gone, or that
  // the application has asked to disconnect, so they do not clog the single-slot
  // queue ahead of live connections. The op in flight (gattOperating) is left
  // untouched; it finishes with a backend error through its own worker, or
  // normally when the disconnect is still pending. Each dropped generic op still
  // gets a failure completion so the caller's callback contract holds; a queued
  // HID discovery is dropped quietly (the HID host's disconnect handling covers
  // its cleanup).
  void purgeQueuedGattOpsLocked(EspBleConnectionId connectionId)
  {
    size_t readIdx = gattQueueHead;
    size_t writeIdx = gattQueueHead;
    size_t survivors = 0;
    for (size_t i = 0; i < gattQueueCount; ++i)
    {
      PendingGattOp &op = gattQueue[readIdx];
      if (op.connectionId == connectionId)
      {
        if (op.operation != EspBleGattOperation::HidDiscover)
        {
          Event failure;
          failure.type = EventType::GattResult;
          failure.gattResult.operation = op.operation;
          failure.gattResult.connectionId = op.connectionId;
          failure.gattResult.serviceUuid = op.serviceUuid;
          failure.gattResult.characteristicUuid = op.characteristicUuid;
          failure.gattResult.descriptorUuid = op.descriptorUuid;
          failure.gattResult.handle = op.characteristicHandle;
          failure.gattResult.descriptorHandle = op.descriptorHandle;
          failure.gattResult.error = EspBleError::InvalidState;
          failure.gattResult.detail = "connection closed before the queued GATT operation started";
          pushEvent(failure);
        }
      }
      else
      {
        if (writeIdx != readIdx) gattQueue[writeIdx] = std::move(op);
        writeIdx = (writeIdx + 1) % GattQueueCapacity;
        ++survivors;
      }
      readIdx = (readIdx + 1) % GattQueueCapacity;
    }
    gattQueueCount = survivors;
  }

  // Role-agnostic MTU tracker. Both the server's onMtuChanged callback and the
  // global GAP listener's BLE_GAP_EVENT_MTU feed this; the value dedup makes the
  // redundant peripheral-side call (both fire for one exchange) a no-op, and it
  // lets a central observe the post-connect MTU exchange it would otherwise miss.
  void updateMtu(uint16_t connectionHandle, uint16_t mtu)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.connection.handle == connectionHandle)
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

  // Called on the backend host task when the peer requires a passkey to be
  // entered. With a static passkey it returns immediately; otherwise it blocks
  // (yielding) until the loop task supplies one via providePasskey(), or a
  // timeout elapses and pairing is rejected with 0.
  // Pairing input, for either role: every connection's GAP events are ours.
  void handlePasskeyAction(uint16_t connectionHandle, const ble_gap_passkey_params &params)
  {
    ble_sm_io io{};
    io.action = params.action;
    switch (params.action)
    {
    case BLE_SM_IOACT_DISP:
    {
      uint32_t passkey = 0;
      {
        std::lock_guard<std::mutex> lock(passkeyMutex);
        passkey = staticPasskeyEnabled ? staticPasskey : (esp_random() % 1000000);
      }
      io.passkey = passkey;
      queuePasskeyDisplayed(passkey, connectionHandle);
      break;
    }
    case BLE_SM_IOACT_INPUT:
      // Blocks the host task until the application answers, exactly as the
      // wrapper's synchronous callback did.
      io.passkey = requestPasskey();
      break;
    case BLE_SM_IOACT_NUMCMP:
      io.numcmp_accept = confirmNumericComparison(params.numcmp, connectionHandle) ? 1 : 0;
      break;
    default:
      return;
    }
    ble_sm_inject_io(connectionHandle, &io);
  }

  uint32_t requestPasskey()
  {
    {
      std::lock_guard<std::mutex> lock(passkeyMutex);
      if (staticPasskeyEnabled)
      {
        return staticPasskey;
      }
    }
    // Bounded wait so a never-answered request cannot stall the host forever.
    // The provided flag is not cleared up front: providePasskey() may run before
    // or after this request, and either order must be honored.
    for (int elapsed = 0; elapsed < 30000; ++elapsed)
    {
      {
        std::lock_guard<std::mutex> lock(passkeyMutex);
        if (passkeyProvided)
        {
          passkeyProvided = false;
          return providedPasskey;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    return 0;
  }

  // Called on the backend host task for Numeric Comparison. Surfaces the value
  // both devices display, then blocks (yielding) until the loop task confirms
  // via confirmNumericComparison(), or a timeout rejects the pairing.
  bool confirmNumericComparison(
    uint32_t pin, uint16_t connectionHandle = BLE_HS_CONN_HANDLE_NONE)
  {
    queueNumericComparison(pin, connectionHandle);
    for (int elapsed = 0; elapsed < 30000; ++elapsed)
    {
      {
        std::lock_guard<std::mutex> lock(passkeyMutex);
        if (numericComparisonConfirmed)
        {
          numericComparisonConfirmed = false;
          return numericComparisonAccept;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    return false;
  }

  // connectionHandle is the pairing connection when it is known -- it is, for
  // every connection whose GAP events we own. The wrapper's security callback
  // does not report one, so that path still has to guess.
  void queueNumericComparison(uint32_t pin, uint16_t connectionHandle = BLE_HS_CONN_HANDLE_NONE)
  {
    std::lock_guard<std::mutex> lock(mutex);
    const ConnectionSlot *selected = nullptr;
    for (const ConnectionSlot &slot : connections)
    {
      if (!slot.used)
      {
        continue;
      }
      if (slot.connection.handle == connectionHandle)
      {
        selected = &slot;
        break;
      }
      if (connectionHandle != BLE_HS_CONN_HANDLE_NONE)
      {
        continue;
      }
      if (selected == nullptr || !slot.connection.encrypted)
      {
        selected = &slot;
      }
    }
    if (selected == nullptr)
    {
      return;
    }
    Event event;
    event.type = EventType::NumericComparison;
    event.passkeyDisplayed.connection = selected->connection;
    event.passkeyDisplayed.passkey = pin;
    pushEvent(event);
  }

  // See queueNumericComparison() for what connectionHandle does.
  void queuePasskeyDisplayed(uint32_t passkey, uint16_t connectionHandle = BLE_HS_CONN_HANDLE_NONE)
  {
    std::lock_guard<std::mutex> lock(mutex);
    const ConnectionSlot *selected = nullptr;
    for (const ConnectionSlot &slot : connections)
    {
      if (!slot.used)
      {
        continue;
      }
      if (slot.connection.handle == connectionHandle)
      {
        selected = &slot;
        break;
      }
      if (connectionHandle != BLE_HS_CONN_HANDLE_NONE)
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

  void pushFailure(
    const EspBleScanResult &target,
    const char *detail,
    EspBleError error = EspBleError::BackendFailure)
  {
    std::lock_guard<std::mutex> lock(mutex);
    Event event;
    event.type = EventType::Failed;
    event.failure.peerAddress = target.address;
    event.failure.error = error;
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
    EspBleGattCharacteristic characteristic,
    const String &serviceUuid,
    const String &characteristicUuid,
    const String &value)
  {
    std::lock_guard<std::mutex> lock(mutex);
    Event event;
    event.type = EventType::ServerWrite;
    event.serverWrite.characteristic = characteristic;
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
    uint16_t connectionHandle,
    EspBleGattDescriptor descriptor,
    const String &serviceUuid,
    const String &characteristicUuid,
    const String &descriptorUuid,
    const String &value)
  {
    std::lock_guard<std::mutex> lock(mutex);
    Event event;
    event.type = EventType::ServerDescriptorWrite;
    event.serverWrite.connectionId = findPeripheralConnectionId(connectionHandle);
    event.serverDescriptor = descriptor;
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
    uint16_t characteristicHandle,
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
    event.notification.handle = characteristicHandle;
    event.notification.value = length == 0
      ? String()
      : String(reinterpret_cast<const char *>(data), length);
    event.notification.indication = indication;
    pushEvent(event);
  }

  // Every characteristic this central has subscribed to, as a Client. The
  // subscription is a CCCD write through the host API and the notifications
  // arrive as GAP events, so they are matched back to a characteristic here by
  // (connection handle, value handle) -- the only identity that stays correct
  // when a peer repeats a UUID.
  struct ClientSubscription
  {
    // Notifications for a subscription with a consumer go straight to it
    // instead of the application's notification callback: the HID Host turns
    // input reports into its own events and never surfaces them as generic
    // GATT notifications. Called on the host task, with no lock held.
    using Consumer = void (*)(
      void *owner, EspBleConnectionId connectionId, uint16_t valueHandle,
      const uint8_t *data, size_t length);

    bool used = false;
    EspBleConnectionId connectionId = 0;
    uint16_t connectionHandle = 0xffff;
    uint16_t valueHandle = 0;
    String serviceUuid;
    String characteristicUuid;
    Consumer consumer = nullptr;
    void *consumerOwner = nullptr;
  };

  static constexpr size_t ClientSubscriptionCapacity = 16;
  ClientSubscription clientSubscriptions[ClientSubscriptionCapacity];

  // Raised on the host task when a GATT operation completes, cleared once its
  // result has been published. See queueClientNotification().
  volatile bool gattCompletionPending = false;
  static constexpr size_t DeferredNotificationCapacity = 4;
  Event deferredNotifications[DeferredNotificationCapacity];
  size_t deferredNotificationCount = 0;

  // Route this attribute's notifications to an internal consumer. Used by the
  // HID Host, whose reports are its own events rather than the application's
  // generic notification callback.
  bool rememberConsumerSubscription(
    EspBleConnectionId connectionId,
    uint16_t connectionHandle,
    uint16_t valueHandle,
    ClientSubscription::Consumer consumer,
    void *consumerOwner)
  {
    std::lock_guard<std::mutex> lock(mutex);
    ClientSubscription *free = nullptr;
    for (ClientSubscription &entry : clientSubscriptions)
    {
      if (entry.used && entry.connectionHandle == connectionHandle &&
          entry.valueHandle == valueHandle)
      {
        free = &entry;
        break;
      }
      if (!entry.used && free == nullptr) free = &entry;
    }
    if (free == nullptr) return false;
    free->used = true;
    free->connectionId = connectionId;
    free->connectionHandle = connectionHandle;
    free->valueHandle = valueHandle;
    free->consumer = consumer;
    free->consumerOwner = consumerOwner;
    return true;
  }

  bool rememberSubscribedHandle(
    EspBleConnectionId connectionId,
    uint16_t connectionHandle,
    uint16_t valueHandle,
    const String &serviceUuid,
    const String &characteristicUuid)
  {
    std::lock_guard<std::mutex> lock(mutex);
    ClientSubscription *free = nullptr;
    for (ClientSubscription &entry : clientSubscriptions)
    {
      if (entry.used && entry.connectionHandle == connectionHandle &&
          entry.valueHandle == valueHandle)
      {
        return true; // already subscribed; the CCCD write just refreshed it
      }
      if (!entry.used && free == nullptr) free = &entry;
    }
    if (free == nullptr) return false;
    free->used = true;
    free->connectionId = connectionId;
    free->connectionHandle = connectionHandle;
    free->valueHandle = valueHandle;
    free->serviceUuid = serviceUuid;
    free->characteristicUuid = characteristicUuid;
    return true;
  }

  void forgetSubscribedHandle(uint16_t connectionHandle, uint16_t valueHandle)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (ClientSubscription &entry : clientSubscriptions)
    {
      if (entry.used && entry.connectionHandle == connectionHandle &&
          entry.valueHandle == valueHandle)
      {
        entry = ClientSubscription();
      }
    }
  }

  void forgetSubscribedHandlesLocked(EspBleConnectionId connectionId)
  {
    for (ClientSubscription &entry : clientSubscriptions)
    {
      if (entry.used && entry.connectionId == connectionId) entry = ClientSubscription();
    }
  }

  // Deliver a notification/indication that arrived for a raw subscription.
  // Returns false when the attribute is not one of ours, leaving the wrapper's
  // own handling as the only path.
  bool queueClientNotification(
    uint16_t connectionHandle,
    uint16_t valueHandle,
    const uint8_t *data,
    size_t length,
    bool indication)
  {
    std::unique_lock<std::mutex> lock(mutex);
    for (const ClientSubscription &entry : clientSubscriptions)
    {
      if (!entry.used || entry.connectionHandle != connectionHandle ||
          entry.valueHandle != valueHandle)
      {
        continue;
      }
      if (entry.consumer != nullptr)
      {
        // Released first: the consumer takes its own lock, and holding both
        // would fix an order this class cannot guarantee elsewhere.
        ClientSubscription::Consumer consumer = entry.consumer;
        void *consumerOwner = entry.consumerOwner;
        const EspBleConnectionId connectionId = entry.connectionId;
        lock.unlock();
        consumer(consumerOwner, connectionId, valueHandle, data, length);
        return true;
      }
      Event event;
      event.type = EventType::Notification;
      event.notification.connectionId = entry.connectionId;
      event.notification.serviceUuid = entry.serviceUuid;
      event.notification.characteristicUuid = entry.characteristicUuid;
      event.notification.handle = valueHandle;
      event.notification.value = length == 0
        ? String()
        : String(reinterpret_cast<const char *>(data), length);
      event.notification.indication = indication;
      // A GATT operation that has completed on the host task but whose result
      // the worker has not published yet: on air the response came first, so
      // hold the notification back rather than let it overtake. Peers commonly
      // notify straight from the write they were asked to perform.
      if (gattCompletionPending && deferredNotificationCount < DeferredNotificationCapacity)
      {
        deferredNotifications[deferredNotificationCount++] = event;
        return true;
      }
      pushEvent(event);
      return true;
    }
    return false;
  }

  // Publish notifications held back while a completion was in flight, in the
  // order they arrived. Called with the mutex held, right after the completion.
  void flushDeferredNotificationsLocked()
  {
    for (size_t index = 0; index < deferredNotificationCount; ++index)
    {
      pushEvent(deferredNotifications[index]);
      deferredNotifications[index] = Event();
    }
    deferredNotificationCount = 0;
  }

  void queueServerSubscription(
    uint16_t connectionHandle,
    EspBleGattCharacteristic characteristic,
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
    event.serverSubscription.characteristic = characteristic;
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

  // Enumerate the peer's whole attribute table into our own snapshot. This runs
  // straight through the NimBLE host API: the bundled wrapper's remote service
  // map is keyed by UUID, so it drops every repeat of one, and each lookup
  // through it restarts a full discovery whose allocations are never released.
  // Returns false with result.error / result.detail filled in.
  static bool discoverDatabase(EspBleImpl *impl, EspBleGattResult &result)
  {
    GattDatabaseSnapshot *database = nullptr;
    uint16_t connectionHandle = 0xffff;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      database = impl->acquireDatabaseLocked(result.connectionId);
      if (database != nullptr) database->reset(result.connectionId);
      for (const ConnectionSlot &slot : impl->connections)
      {
        if (slot.used && slot.connection.id == result.connectionId)
        {
          connectionHandle = slot.connection.handle;
          break;
        }
      }
    }
    if (database == nullptr)
    {
      result.error = EspBleError::ResourceExhausted;
      result.detail = "failed to allocate the GATT database snapshot";
      return false;
    }
    if (connectionHandle == 0xffff)
    {
      result.error = EspBleError::InvalidState;
      result.detail = "connection is not an active Central connection";
      return false;
    }

    // Heap-allocated: one context per phase is several kilobytes, far too much
    // for the GATT worker task's stack.
    std::unique_ptr<espble_discovery::ServiceContext> context(
      new (std::nothrow) espble_discovery::ServiceContext());
    if (context == nullptr)
    {
      result.error = EspBleError::ResourceExhausted;
      result.detail = "failed to allocate GATT discovery state";
      return false;
    }
    if (ble_gattc_disc_all_svcs(
          connectionHandle, espble_discovery::serviceCallback, context.get()) != 0 ||
        !espble_discovery::wait(*context, 10000) || context->status != 0)
    {
      result.error = EspBleError::BackendFailure;
      result.detail = "failed to enumerate GATT services";
      return false;
    }

    bool success = true;
    const size_t serviceCount = context->count;
    for (size_t serviceIndex = 0; serviceIndex < serviceCount && success; ++serviceIndex)
    {
      const espble_discovery::ServiceRange service = context->services[serviceIndex];
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        EspBleGattServiceInfo &info = database->services[database->serviceCount++];
        info.serviceUuid = service.uuid;
        info.handle = service.startHandle;
      }

      // Characteristics of this service, by handle range.
      std::unique_ptr<espble_discovery::CharacteristicContext> characteristics(
        new (std::nothrow) espble_discovery::CharacteristicContext());
      if (characteristics == nullptr)
      {
        success = false;
        result.error = EspBleError::ResourceExhausted;
        result.detail = "failed to allocate GATT discovery state";
        break;
      }
      if (ble_gattc_disc_all_chrs(
            connectionHandle, service.startHandle, service.endHandle,
            espble_discovery::characteristicCallback, characteristics.get()) != 0 ||
          !espble_discovery::wait(*characteristics, 10000) ||
          characteristics->status != 0)
      {
        success = false;
        result.error = EspBleError::BackendFailure;
        result.detail = "failed to enumerate GATT characteristics";
        break;
      }

      for (size_t index = 0; index < characteristics->count; ++index)
      {
        const espble_discovery::CharacteristicEntry &entry =
          characteristics->characteristics[index];
        {
          std::lock_guard<std::mutex> lock(impl->mutex);
          if (database->characteristicCount == EspBle::MaxDiscoveredGattCharacteristics)
          {
            success = false;
            result.error = EspBleError::ResourceExhausted;
            result.detail = "too many discovered GATT characteristics";
            break;
          }
          EspBleGattCharacteristicInfo &info =
            database->characteristics[database->characteristicCount++];
          info.serviceUuid = service.uuid;
          info.characteristicUuid = entry.uuid;
          info.handle = entry.valueHandle;
          info.readable = (entry.properties & BLE_GATT_CHR_PROP_READ) != 0;
          info.writable = (entry.properties & BLE_GATT_CHR_PROP_WRITE) != 0;
          info.writableWithoutResponse =
            (entry.properties & BLE_GATT_CHR_PROP_WRITE_NO_RSP) != 0;
          info.notifiable = (entry.properties & BLE_GATT_CHR_PROP_NOTIFY) != 0;
          info.indicatable = (entry.properties & BLE_GATT_CHR_PROP_INDICATE) != 0;
        }

        // Descriptors live between this characteristic's value handle and the
        // next characteristic's declaration (or the service end).
        const uint16_t descriptorEnd =
          index + 1 < characteristics->count
            ? static_cast<uint16_t>(
                characteristics->characteristics[index + 1].definitionHandle - 1)
            : service.endHandle;
        if (descriptorEnd <= entry.valueHandle) continue;

        std::unique_ptr<espble_discovery::DescriptorContext> descriptors(
          new (std::nothrow) espble_discovery::DescriptorContext());
        if (descriptors == nullptr)
        {
          success = false;
          result.error = EspBleError::ResourceExhausted;
          result.detail = "failed to allocate GATT discovery state";
          break;
        }
        if (ble_gattc_disc_all_dscs(
              connectionHandle, entry.valueHandle, descriptorEnd,
              espble_discovery::descriptorCallback, descriptors.get()) != 0 ||
            !espble_discovery::wait(*descriptors, 10000) || descriptors->status != 0)
        {
          success = false;
          result.error = EspBleError::BackendFailure;
          result.detail = "failed to enumerate GATT descriptors";
          break;
        }
        std::lock_guard<std::mutex> lock(impl->mutex);
        for (size_t d = 0; d < descriptors->count; ++d)
        {
          if (database->descriptorCount == EspBle::MaxDiscoveredGattDescriptors)
          {
            success = false;
            result.error = EspBleError::ResourceExhausted;
            result.detail = "too many discovered GATT descriptors";
            break;
          }
          EspBleGattDescriptorInfo &info = database->descriptors[database->descriptorCount++];
          info = descriptors->descriptors[d];
          info.serviceUuid = service.uuid;
          info.characteristicUuid = entry.uuid;
          info.characteristicHandle = entry.valueHandle;
        }
      }
    }
    if (context->overflowed)
    {
      success = false;
      result.error = EspBleError::ResourceExhausted;
      result.detail = "too many discovered GATT services";
    }
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      database->valid = success;
    }
    return success;
  }

  static void runGattOperation(void *argument)
  {
    EspBleImpl *impl = static_cast<EspBleImpl *>(argument);
    EspBleGattResult result;
    String writeValue;
    bool response = true;
    // Set when the subscribed characteristic shares its UUID with another on the
    // same peer, which makes it ineligible for auto-restore on reconnect.
    bool subscriptionAmbiguous = false;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      result.operation = impl->gattOperation;
      result.connectionId = impl->gattConnectionId;
      result.serviceUuid = impl->gattServiceUuid;
      result.characteristicUuid = impl->gattCharacteristicUuid;
      result.descriptorUuid = impl->gattDescriptorUuid;
      result.handle = impl->gattCharacteristicHandle;
      result.descriptorHandle = impl->gattDescriptorHandle;
      writeValue = impl->gattWriteValue;
      response = impl->gattWriteResponse;
      result.response = response;
    }

    ble_gap_conn_desc description{};
    uint16_t operationHandle = 0xffff;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      for (const ConnectionSlot &slot : impl->connections)
      {
        if (slot.used && slot.connection.id == result.connectionId &&
            slot.connection.localRole == EspBleRole::Central)
        {
          operationHandle = slot.connection.handle;
          break;
        }
      }
    }

    if (operationHandle == 0xffff || ble_gap_conn_find(operationHandle, &description) != 0)
    {
      result.error = EspBleError::InvalidState;
      result.detail = "connection is not an active Central connection";
    }
    else if (result.operation == EspBleGattOperation::DiscoverServices)
    {
      result.success = discoverDatabase(impl, result);
    }
    else
    {
      // Every operation below talks to the peer by attribute handle through the
      // NimBLE host API, resolved against our own discovery snapshot. The
      // wrapper's remote objects are deliberately never used: they cannot
      // represent a repeated UUID, and creating them leaks.
      uint16_t connectionHandle = 0xffff;
      bool discovered = false;
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        for (const ConnectionSlot &slot : impl->connections)
        {
          if (slot.used && slot.connection.id == result.connectionId)
          {
            connectionHandle = slot.connection.handle;
            break;
          }
        }
        const GattDatabaseSnapshot *database = impl->findDatabaseLocked(result.connectionId);
        discovered = database != nullptr && database->valid;
      }

      // Resolving a target needs the attribute table, so discover it once per
      // connection even when the caller never asked for it explicitly.
      if (!discovered && !discoverDatabase(impl, result))
      {
        // discoverDatabase() filled in the failure.
      }
      else
      {
        // A descriptor named by handle carries no UUIDs of its own, so the
        // owning characteristic is resolved from the snapshot below and this
        // flag is decided there rather than here.
        const bool byDescriptorHandle = result.descriptorHandle != 0;
        bool byHandle = result.handle != 0;
        uint16_t valueHandle = 0;
        uint16_t cccdHandle = 0;
        uint16_t descriptorHandle = 0;
        bool found = false;
        bool descriptorHandleFound = false;
        // True when the peer has more than one characteristic with the target's
        // UUID pair, i.e. when a UUID alone cannot name this attribute again.
        bool ambiguousUuid = false;
        {
          std::lock_guard<std::mutex> lock(impl->mutex);
          const GattDatabaseSnapshot *database = impl->findDatabaseLocked(result.connectionId);
          if (database != nullptr)
          {
            // Resolve a handle-addressed descriptor first: it yields the owning
            // characteristic's handle, which is the only way to reach the right
            // characteristic when several repeat a UUID — the case this
            // addressing mode exists for.
            for (size_t index = 0; byDescriptorHandle && index < database->descriptorCount; ++index)
            {
              const EspBleGattDescriptorInfo &descriptor = database->descriptors[index];
              if (descriptor.handle != result.descriptorHandle) continue;
              descriptorHandleFound = true;
              descriptorHandle = descriptor.handle;
              result.descriptorUuid = descriptor.descriptorUuid;
              result.handle = descriptor.characteristicHandle;
              byHandle = true;
              break;
            }
            String serviceUuid;
            String characteristicUuid;
            for (size_t index = 0; index < database->characteristicCount; ++index)
            {
              const EspBleGattCharacteristicInfo &info = database->characteristics[index];
              if (byHandle)
              {
                if (info.handle != result.handle) continue;
              }
              // Compared through the UUID codec: a caller may name a 16-bit UUID as
              // "2a19" while discovery records the 128-bit form.
              else if (!uuidEquals(info.serviceUuid, result.serviceUuid.c_str()) ||
                       !uuidEquals(info.characteristicUuid, result.characteristicUuid.c_str()))
              {
                continue;
              }
              if (found) continue;
              valueHandle = info.handle;
              serviceUuid = info.serviceUuid;
              characteristicUuid = info.characteristicUuid;
              result.handle = info.handle;
              result.readable = info.readable;
              result.writable = info.writable;
              result.writableWithoutResponse = info.writableWithoutResponse;
              result.notifiable = info.notifiable;
              result.indicatable = info.indicatable;
              found = true;
            }
            // A handle-addressed target carries no UUIDs of its own, so report
            // the ones discovery recorded. A UUID-addressed one keeps the
            // caller's spelling, which is what its callbacks compare against.
            if (found && byHandle)
            {
              result.serviceUuid = serviceUuid;
              result.characteristicUuid = characteristicUuid;
            }
            // Count the UUID pair separately: a handle-addressed target says
            // nothing about how many characteristics share its UUID.
            size_t sameUuid = 0;
            for (size_t index = 0; found && index < database->characteristicCount; ++index)
            {
              const EspBleGattCharacteristicInfo &info = database->characteristics[index];
              if (info.serviceUuid.equalsIgnoreCase(serviceUuid) &&
                  info.characteristicUuid.equalsIgnoreCase(characteristicUuid))
              {
                ++sameUuid;
              }
            }
            ambiguousUuid = sameUuid > 1;
            for (size_t index = 0; found && index < database->descriptorCount; ++index)
            {
              const EspBleGattDescriptorInfo &descriptor = database->descriptors[index];
              // Matched by the owning value handle: the UUID pair cannot pick
              // between two characteristics that repeat a UUID.
              if (descriptor.characteristicHandle != valueHandle) continue;
              if (uuidEquals(
                    descriptor.descriptorUuid, EspBle::ClientCharacteristicConfigurationUuid))
              {
                cccdHandle = descriptor.handle;
              }
              if (!byDescriptorHandle && result.descriptorUuid.length() != 0 &&
                  uuidEquals(descriptor.descriptorUuid, result.descriptorUuid.c_str()))
              {
                descriptorHandle = descriptor.handle;
              }
            }
          }
        }

        // Fill in the resolved descriptor handle for a UUID-addressed operation so
        // the application learns which attribute answered. A handle-addressed one
        // keeps the requested value even when it resolves to nothing, so a
        // failure still says which call it belongs to.
        if (descriptorHandle != 0) result.descriptorHandle = descriptorHandle;

        if (byDescriptorHandle && !descriptorHandleFound)
        {
          result.error = EspBleError::NotFound;
          result.detail = "GATT descriptor handle was not found";
        }
        else if (!found)
        {
          result.error = EspBleError::NotFound;
          result.detail = byHandle
            ? "GATT characteristic handle was not found"
            : "GATT characteristic was not found";
        }
        else
        {
          espble_raw::Operation operation;
          operation.waiter = xTaskGetCurrentTaskHandle();
          operation.completionPending = &impl->gattCompletionPending;
          switch (result.operation)
          {
          case EspBleGattOperation::Discover:
            result.success = true;
            break;
          case EspBleGattOperation::Read:
            if (!result.readable)
            {
              result.error = EspBleError::InvalidState;
              result.detail = "GATT characteristic is not readable";
            }
            // Read Long: a value longer than one MTU would otherwise come back
            // silently truncated.
            else if (ble_gattc_read_long(
                       connectionHandle, valueHandle, 0, espble_raw::readLongCallback,
                       &operation) != 0 ||
                     !espble_raw::wait(operation, 10000))
            {
              result.error = EspBleError::Timeout;
              result.detail = "GATT read timed out";
            }
            else if (operation.status != 0)
            {
              result.error = EspBleError::BackendFailure;
              result.detail = String("GATT read failed with status ") + operation.status;
            }
            else
            {
              result.value = operation.value;
              result.success = true;
            }
            break;
          case EspBleGattOperation::Write:
            if (!(response ? result.writable : result.writableWithoutResponse))
            {
              result.error = EspBleError::InvalidState;
              result.detail = response
                ? "GATT characteristic does not support write with response"
                : "GATT characteristic does not support write without response";
            }
            else if (!response)
            {
              // Fire and forget: the peer never answers, so there is nothing to
              // wait for and no status to report.
              result.success = ble_gattc_write_no_rsp_flat(
                connectionHandle, valueHandle, writeValue.c_str(), writeValue.length()) == 0;
              if (!result.success)
              {
                result.error = EspBleError::BackendFailure;
                result.detail = "GATT write without response failed";
              }
            }
            else if (ble_gattc_write_flat(
                       connectionHandle, valueHandle, writeValue.c_str(), writeValue.length(),
                       espble_raw::writeCallback, &operation) != 0 ||
                     !espble_raw::wait(operation, 10000))
            {
              result.error = EspBleError::Timeout;
              result.detail = "GATT write timed out";
            }
            else if (operation.status != 0)
            {
              result.error = EspBleError::BackendFailure;
              result.detail = String("GATT write failed with status ") + operation.status;
            }
            else
            {
              result.success = true;
            }
            break;
          case EspBleGattOperation::ReadDescriptor:
          case EspBleGattOperation::WriteDescriptor:
            if (descriptorHandle == 0)
            {
              result.error = EspBleError::NotFound;
              result.detail = "GATT descriptor was not found";
            }
            else if (result.operation == EspBleGattOperation::ReadDescriptor)
            {
              if (ble_gattc_read_long(
                    connectionHandle, descriptorHandle, 0, espble_raw::readLongCallback,
                    &operation) != 0 ||
                  !espble_raw::wait(operation, 10000))
              {
                result.error = EspBleError::Timeout;
                result.detail = "GATT descriptor read timed out";
              }
              else if (operation.status != 0)
              {
                result.error = EspBleError::BackendFailure;
                result.detail =
                  String("GATT descriptor read failed with status ") + operation.status;
              }
              else
              {
                result.value = operation.value;
                result.success = true;
              }
            }
            else if (!response)
            {
              result.success = ble_gattc_write_no_rsp_flat(
                connectionHandle, descriptorHandle, writeValue.c_str(),
                writeValue.length()) == 0;
              if (!result.success)
              {
                result.error = EspBleError::BackendFailure;
                result.detail = "GATT descriptor write without response failed";
              }
            }
            else if (ble_gattc_write_flat(
                       connectionHandle, descriptorHandle, writeValue.c_str(),
                       writeValue.length(), espble_raw::writeCallback, &operation) != 0 ||
                     !espble_raw::wait(operation, 10000))
            {
              result.error = EspBleError::Timeout;
              result.detail = "GATT descriptor write timed out";
            }
            else if (operation.status != 0)
            {
              result.error = EspBleError::BackendFailure;
              result.detail =
                String("GATT descriptor write failed with status ") + operation.status;
            }
            else
            {
              result.success = true;
            }
            break;
          case EspBleGattOperation::Subscribe:
          case EspBleGattOperation::Unsubscribe:
          {
            const bool subscribing = result.operation == EspBleGattOperation::Subscribe;
            const bool notifications = response;
            if (subscribing &&
                ((notifications && !result.notifiable) || (!notifications && !result.indicatable)))
            {
              result.error = EspBleError::InvalidState;
              result.detail = notifications
                ? "GATT characteristic does not support notifications"
                : "GATT characteristic does not support indications";
              break;
            }
            if (cccdHandle == 0)
            {
              result.error = EspBleError::NotFound;
              result.detail = "GATT characteristic has no Client Characteristic "
                              "Configuration Descriptor";
              break;
            }
            // Subscribing is a CCCD write: bit 0 enables Notification, bit 1
            // Indication, and 0x0000 turns both off again.
            const uint16_t configuration =
              !subscribing ? 0x0000 : (notifications ? 0x0001 : 0x0002);
            const uint8_t payload[2] = {
              static_cast<uint8_t>(configuration & 0xff),
              static_cast<uint8_t>(configuration >> 8)};
            // Register before the write, so a notification that races the write
            // response is still delivered.
            if (subscribing &&
                !impl->rememberSubscribedHandle(
                  result.connectionId, connectionHandle, valueHandle, result.serviceUuid,
                  result.characteristicUuid))
            {
              result.error = EspBleError::ResourceExhausted;
              result.detail = "too many GATT client subscriptions";
              break;
            }
            if (ble_gattc_write_flat(
                  connectionHandle, cccdHandle, payload, sizeof(payload),
                  espble_raw::writeCallback, &operation) != 0 ||
                !espble_raw::wait(operation, 10000))
            {
              result.error = EspBleError::Timeout;
              result.detail = "GATT CCCD write timed out";
            }
            else if (operation.status != 0)
            {
              result.error = EspBleError::BackendFailure;
              result.detail = String("GATT CCCD write failed with status ") + operation.status;
            }
            else
            {
              result.success = true;
              result.subscribedToNotifications = subscribing && notifications;
              result.subscribedToIndications = subscribing && !notifications;
            }
            if (!result.success || !subscribing)
            {
              impl->forgetSubscribedHandle(connectionHandle, valueHandle);
            }
            // Auto-restore on reconnect is keyed by UUID, so it can only be
            // offered when the UUID names exactly one characteristic.
            subscriptionAmbiguous = ambiguousUuid;
            break;
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
        if (result.operation == EspBleGattOperation::DiscoverServices)
        {
          GattDatabaseSnapshot *database =
            impl->findDatabaseLocked(result.connectionId);
          if (database != nullptr) database->valid = result.success;
        }
        // Remember/forget the subscription so it can be auto-restored on the
        // next connection to this peer (keyed by peer address, not connection).
        if (result.success && !subscriptionAmbiguous &&
            result.operation == EspBleGattOperation::Subscribe)
        {
          impl->recordSubscriptionLocked(
            impl->connectionAddressLocked(result.connectionId),
            result.serviceUuid, result.characteristicUuid,
            result.subscribedToNotifications);
        }
        else if (result.success && !subscriptionAmbiguous &&
                 result.operation == EspBleGattOperation::Unsubscribe)
        {
          impl->forgetSubscriptionLocked(
            impl->connectionAddressLocked(result.connectionId),
            result.serviceUuid, result.characteristicUuid);
        }
        Event event;
        event.type = EventType::GattResult;
        event.gattResult = result;
        impl->pushEvent(event);
      }
      else if (result.operation == EspBleGattOperation::DiscoverServices)
      {
        GattDatabaseSnapshot *database =
          impl->findDatabaseLocked(result.connectionId);
        if (database != nullptr) database->reset(result.connectionId);
      }
      // The completion is out; anything held back behind it goes next.
      impl->gattCompletionPending = false;
      impl->flushDeferredNotificationsLocked();
      impl->gattOperating = false;
      impl->gattTask = nullptr;
    }
  }

  // vTaskDelete() never returns, so the body runs in its own function: the destructors
  // of its locals (Strings hold heap buffers) must run before the task ends, or every
  // operation leaks them.
  static void gattTaskEntry(void *argument)
  {
    runGattOperation(argument);
    vTaskDelete(nullptr);
  }


  EspBle *owner;
  mutable std::mutex mutex;
  // Global GAP event listener used to capture information the backend's
  // BLEServer/BLEClient callbacks do not surface: each disconnection's reason
  // code and connection parameter updates. NimBLE invokes global listeners
  // before the connection-specific callback, so a disconnect reason is stamped
  // onto the connection slot before removeConnection() reads it while building
  // the Disconnected event.
  ble_gap_event_listener gapEventListener = {};
  bool gapEventListenerRegistered = false;
  // Own address type passed to every GAP procedure we start (public, random
  // static or RPA), chosen at begin() from EspBleConfig::ownAddressType.
  uint8_t ownAddressType = BLE_OWN_ADDR_PUBLIC;
  // Set once ble_gatts_start() has run; the attribute table cannot change after.
  bool gattServerStarted = false;
  // Set once the mandatory GAP/GATT services have been registered.
  bool peripheralPrepared = false;
  ConnectionSlot connections[ConnectionCapacity];
  Event events[ConnectionEventQueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  EspBleConnectionId nextConnectionId = 1;
  bool connecting = false;
  EspBleScanResult connectTarget;
  uint32_t connectTimeoutMilliseconds = 10000;
  // When the attempt must be given up on. The host's own duration would do the
  // job, but the bundled NimBLE re-attempts underneath it and stretches the
  // wait well past what the caller asked for, so the deadline is enforced here.
  uint32_t connectDeadlineMilliseconds = 0;
  bool connectCancelRequested = false;

  bool securityEnabled = false;
  // Start pairing as soon as a peer connects (EspBleSecurityConfig::pairOnConnect).
  bool pairOnConnect = false;
  // Passkey Entry (input side). When no static passkey is configured, the
  // backend's synchronous onPassKeyRequest() blocks here until the application
  // supplies one via providePasskey() from the loop task, enabling interactive
  // runtime passkey entry.
  std::mutex passkeyMutex;
  bool staticPasskeyEnabled = false;
  uint32_t staticPasskey = 0;
  bool passkeyProvided = false;
  uint32_t providedPasskey = 0;
  // Numeric Comparison response (also guarded by passkeyMutex).
  bool numericComparisonConfirmed = false;
  bool numericComparisonAccept = false;
  bool gattOperating = false;
  TaskHandle_t gattTask = nullptr;
  EspBleGattOperation gattOperation = EspBleGattOperation::Discover;
  EspBleConnectionId gattConnectionId = 0;
  String gattServiceUuid;
  String gattCharacteristicUuid;
  String gattDescriptorUuid;
  uint16_t gattCharacteristicHandle = 0; // when non-zero, target by handle
  // When non-zero, the descriptor operation names the descriptor by handle. The
  // owning characteristic is then resolved from the discovery snapshot rather
  // than from the caller's UUIDs.
  uint16_t gattDescriptorHandle = 0;
  String gattWriteValue;
  bool gattWriteResponse = true;
  uint32_t gattStartMilliseconds = 0;
  uint32_t gattTimeoutMilliseconds = 10000;
  bool gattTimedOut = false;
  // Per-connection discovery cache. Each connection keeps its own snapshot so a
  // discovery on one connection does not evict another's, and query methods
  // (discoveredService/Characteristic/Descriptor) resolve against the caller's
  // connectionId. Snapshots are allocated lazily on first discovery and freed
  // when the connection drops. Capacity matches the connection capacity, so an
  // active connection always finds a free slot.
  GattDatabaseSnapshot *gattDatabases[ConnectionCapacity] = {};

  GattDatabaseSnapshot *findDatabaseLocked(EspBleConnectionId connectionId)
  {
    if (connectionId == 0) return nullptr;
    for (GattDatabaseSnapshot *database : gattDatabases)
    {
      if (database != nullptr && database->connectionId == connectionId) return database;
    }
    return nullptr;
  }

  // Return the snapshot for connectionId, allocating one in a free slot if none
  // exists yet. Returns nullptr only when allocation fails or no slot is free.
  GattDatabaseSnapshot *acquireDatabaseLocked(EspBleConnectionId connectionId)
  {
    GattDatabaseSnapshot *existing = findDatabaseLocked(connectionId);
    if (existing != nullptr) return existing;
    for (GattDatabaseSnapshot *&database : gattDatabases)
    {
      if (database == nullptr)
      {
        database = new (std::nothrow) GattDatabaseSnapshot();
        if (database != nullptr) database->reset(connectionId);
        return database;
      }
    }
    return nullptr;
  }

  void releaseDatabaseLocked(EspBleConnectionId connectionId)
  {
    for (GattDatabaseSnapshot *&database : gattDatabases)
    {
      if (database != nullptr && database->connectionId == connectionId)
      {
        delete database;
        database = nullptr;
      }
    }
  }

  // Persistent (auto-restored) client subscriptions. A successful subscribe is
  // recorded keyed by peer address so it can be re-issued the next time this
  // central connects to the same peer; a successful unsubscribe forgets it.
  // Records survive disconnects by design (that is what "persistent" means).
  static constexpr size_t PersistentSubscriptionCapacity = 16;
  struct PersistentSubscription
  {
    bool used = false;
    String peerAddress;
    String serviceUuid;
    String characteristicUuid;
    bool notifications = true;
  };
  PersistentSubscription persistentSubscriptions[PersistentSubscriptionCapacity];
  bool persistentSubscriptionsEnabled = true;
  // Counts subscriptions that could not be recorded because the registry was
  // full. Non-zero means some subscriptions will not be restored on reconnect.
  size_t droppedPersistentSubscriptions = 0;

  String connectionAddressLocked(EspBleConnectionId connectionId)
  {
    for (const ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.connection.id == connectionId) return slot.connection.peerAddress;
    }
    return String();
  }

  void recordSubscriptionLocked(
    const String &peerAddress,
    const String &serviceUuid,
    const String &characteristicUuid,
    bool notifications)
  {
    if (!persistentSubscriptionsEnabled || peerAddress.length() == 0) return;
    PersistentSubscription *free = nullptr;
    for (PersistentSubscription &entry : persistentSubscriptions)
    {
      if (!entry.used)
      {
        if (free == nullptr) free = &entry;
        continue;
      }
      if (entry.peerAddress.equalsIgnoreCase(peerAddress) &&
          entry.serviceUuid.equalsIgnoreCase(serviceUuid) &&
          entry.characteristicUuid.equalsIgnoreCase(characteristicUuid))
      {
        entry.notifications = notifications; // refresh (dedup by peer+service+char)
        return;
      }
    }
    if (free == nullptr)
    {
      // Registry full: leave existing records intact but count the loss so the
      // application can tell some subscriptions will not be restored.
      ++droppedPersistentSubscriptions;
      return;
    }
    free->used = true;
    free->peerAddress = peerAddress;
    free->serviceUuid = serviceUuid;
    free->characteristicUuid = characteristicUuid;
    free->notifications = notifications;
  }

  void forgetSubscriptionLocked(
    const String &peerAddress,
    const String &serviceUuid,
    const String &characteristicUuid)
  {
    for (PersistentSubscription &entry : persistentSubscriptions)
    {
      if (entry.used && entry.peerAddress.equalsIgnoreCase(peerAddress) &&
          entry.serviceUuid.equalsIgnoreCase(serviceUuid) &&
          entry.characteristicUuid.equalsIgnoreCase(characteristicUuid))
      {
        entry = PersistentSubscription();
        return;
      }
    }
  }

  // Copy the persistent subscriptions recorded for connectionId's peer into out
  // (up to max), returning the count. Used to auto-restore them on connect.
  size_t collectPersistentSubscriptionsLocked(
    EspBleConnectionId connectionId, PersistentSubscription *out, size_t max)
  {
    if (!persistentSubscriptionsEnabled) return 0;
    const String peerAddress = connectionAddressLocked(connectionId);
    if (peerAddress.length() == 0) return 0;
    size_t count = 0;
    for (const PersistentSubscription &entry : persistentSubscriptions)
    {
      if (!entry.used || !entry.peerAddress.equalsIgnoreCase(peerAddress)) continue;
      if (count >= max) break;
      out[count++] = entry;
    }
    return count;
  }

  // Auto-reconnect state: the set of Central peers the library should keep
  // connected. A peer is remembered when this central connects to it (while
  // auto-reconnect is on) and forgotten when the application disconnect()s it.
  // update() reconnects any remembered peer that is not currently connected.
  static constexpr uint32_t ReconnectIntervalMilliseconds = 2000;
  struct DesiredConnection
  {
    bool used = false;
    String address;
    EspBleAddressType addressType = EspBleAddressType::Public;
    uint32_t nextAttemptMs = 0;
  };
  DesiredConnection desiredConnections[ConnectionCapacity];
  bool autoReconnectEnabled = false;

  bool isCentralConnectedToLocked(const String &address)
  {
    for (const ConnectionSlot &slot : connections)
    {
      if (slot.used && slot.connection.localRole == EspBleRole::Central &&
          slot.connection.peerAddress.equalsIgnoreCase(address))
      {
        return true;
      }
    }
    return false;
  }

  void rememberDesiredLocked(const String &address, EspBleAddressType addressType)
  {
    if (!autoReconnectEnabled || address.length() == 0) return;
    DesiredConnection *free = nullptr;
    for (DesiredConnection &entry : desiredConnections)
    {
      if (!entry.used)
      {
        if (free == nullptr) free = &entry;
        continue;
      }
      if (entry.address.equalsIgnoreCase(address)) return; // already tracked
    }
    if (free == nullptr) return;
    free->used = true;
    free->address = address;
    free->addressType = addressType;
    free->nextAttemptMs = 0;
  }

  void forgetDesiredLocked(const String &address)
  {
    for (DesiredConnection &entry : desiredConnections)
    {
      if (entry.used && entry.address.equalsIgnoreCase(address))
      {
        entry = DesiredConnection();
        return;
      }
    }
  }

  void clearDesiredLocked()
  {
    for (DesiredConnection &entry : desiredConnections) entry = DesiredConnection();
  }

  // Pending GATT client operations. Callers enqueue and the loop task pumps the
  // queue one at a time (a single ATT transaction runs at once, shared with HID
  // Host discovery via gattOperating). This makes read/write/subscribe/discover
  // "call any time" — they queue instead of failing when one is in flight.
  static constexpr size_t GattQueueCapacity = 8;
  struct PendingGattOp
  {
    EspBleGattOperation operation = EspBleGattOperation::Discover;
    EspBleConnectionId connectionId = 0;
    String serviceUuid;
    String characteristicUuid;
    String descriptorUuid;
    uint16_t characteristicHandle = 0;
    uint16_t descriptorHandle = 0;
    String writeValue;
    bool response = true;
    uint32_t timeoutMilliseconds = 10000;
  };
  PendingGattOp gattQueue[GattQueueCapacity];
  size_t gattQueueHead = 0;
  size_t gattQueueCount = 0;
};

struct EspBleGattServerImpl
{
  struct ServiceDefinition
  {
    String uuid;
    ble_uuid_any_t nativeUuid{};
    // Entry in the NimBLE service table; null until realize() registers it.
    ble_gatt_svc_def *def = nullptr;
  };

  struct CharacteristicDefinition
  {
    size_t serviceIndex = 0;
    String serviceUuid;
    String uuid;
    ble_uuid_any_t nativeUuid{};
    EspBleGattCharacteristicConfig config;
    String value;
    ble_gatt_chr_def *def = nullptr;
    // Attribute handle of the value, filled in by the host when the GATT server
    // starts (ble_gatt_chr_def::val_handle points here).
    uint16_t valueHandle = 0;
  };

  struct DescriptorDefinition
  {
    size_t characteristicIndex = 0;
    String serviceUuid;
    String characteristicUuid;
    String uuid;
    ble_uuid_any_t nativeUuid{};
    EspBleGattDescriptorConfig config;
    String value;
    ble_gatt_dsc_def *def = nullptr;
  };

  // CCCD state per (connection, characteristic). The host reports every change
  // as a GAP subscribe event; a broadcast notify goes only to the peers listed
  // here. Matched by attribute handle, which stays correct when a UUID repeats.
  struct SubscriptionSlot
  {
    bool used = false;
    uint16_t connectionHandle = 0xffff;
    uint16_t valueHandle = 0;
    bool notifications = false;
    bool indications = false;
  };
  static constexpr size_t SubscriptionCapacity = 12;

  // Attribute access on the NimBLE host task. `argument` is this server, and
  // the definition is identified by the ble_gatt_chr_def / ble_gatt_dsc_def the
  // host passes back: pointer identity works where a UUID would be ambiguous.
  static int accessCallback(
    uint16_t connectionHandle,
    uint16_t attributeHandle,
    ble_gatt_access_ctxt *context,
    void *argument)
  {
    return static_cast<EspBleGattServerImpl *>(argument)->handleAccess(
      connectionHandle, attributeHandle, context);
  }

  int handleAccess(
    uint16_t connectionHandle, uint16_t, ble_gatt_access_ctxt *context)
  {
    if (context->op == BLE_GATT_ACCESS_OP_READ_CHR ||
        context->op == BLE_GATT_ACCESS_OP_READ_DSC)
    {
      if (context->op == BLE_GATT_ACCESS_OP_READ_CHR)
      {
        // Before the lock: the callback is expected to answer with setValue(),
        // which takes the same mutex.
        notifyReadRequest(connectionHandle, context);
      }
      // Values are written from the loop task; hold the mutex so a read on the
      // host task cannot observe a half-updated String.
      std::lock_guard<std::mutex> lock(mutex);
      const String *value = findValueLocked(context);
      if (value == nullptr) return BLE_ATT_ERR_UNLIKELY;
      if (value->length() == 0) return 0;
      return os_mbuf_append(
               context->om, value->c_str(), static_cast<uint16_t>(value->length())) == 0
        ? 0
        : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR &&
        context->op != BLE_GATT_ACCESS_OP_WRITE_DSC)
    {
      return BLE_ATT_ERR_UNLIKELY;
    }

    const uint16_t length = context->om == nullptr ? 0 : OS_MBUF_PKTLEN(context->om);
    String value;
    if (length != 0)
    {
      std::unique_ptr<char[]> buffer(new (std::nothrow) char[length]);
      if (buffer == nullptr) return BLE_ATT_ERR_INSUFFICIENT_RES;
      uint16_t copied = 0;
      if (ble_hs_mbuf_to_flat(context->om, buffer.get(), length, &copied) != 0)
      {
        return BLE_ATT_ERR_UNLIKELY;
      }
      value = String(buffer.get(), copied);
    }

    EspBleGattCharacteristic characteristicHandle;
    EspBleGattDescriptor descriptorHandle;
    String serviceUuid;
    String characteristicUuid;
    String descriptorUuid;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (context->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
      {
        for (size_t index = 0; index < characteristicCount; ++index)
        {
          CharacteristicDefinition &definition = characteristics[index];
          if (definition.def == nullptr || definition.def != context->chr) continue;
          definition.value = value;
          characteristicHandle.id = static_cast<uint16_t>(index + 1);
          serviceUuid = definition.serviceUuid;
          characteristicUuid = definition.uuid;
          break;
        }
        if (!characteristicHandle.valid()) return BLE_ATT_ERR_UNLIKELY;
      }
      else
      {
        for (size_t index = 0; index < descriptorCount; ++index)
        {
          DescriptorDefinition &definition = descriptors[index];
          if (definition.def == nullptr || definition.def != context->dsc) continue;
          // The configured maximum is what the application sized its storage
          // for, so refuse anything longer instead of truncating silently.
          if (value.length() > definition.config.maximumLength)
          {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
          }
          definition.value = value;
          descriptorHandle.id = static_cast<uint16_t>(index + 1);
          serviceUuid = definition.serviceUuid;
          characteristicUuid = definition.characteristicUuid;
          descriptorUuid = definition.uuid;
          break;
        }
        if (!descriptorHandle.valid()) return BLE_ATT_ERR_UNLIKELY;
      }
    }

    EspBleImpl *ownerImpl = server->owner_->impl_;
    if (ownerImpl != nullptr)
    {
      if (characteristicHandle.valid())
      {
        ownerImpl->queueServerWrite(
          connectionHandle, characteristicHandle, serviceUuid, characteristicUuid, value);
      }
      else
      {
        ownerImpl->queueServerDescriptorWrite(
          connectionHandle, descriptorHandle, serviceUuid, characteristicUuid, descriptorUuid,
          value);
      }
    }
    return 0;
  }

  // Hand a read request to the application so it can publish the value now.
  // Runs on the host task with no lock held.
  void notifyReadRequest(uint16_t connectionHandle, const ble_gatt_access_ctxt *context)
  {
    EspBleGattReadRequest request;
    {
      std::lock_guard<std::mutex> lock(mutex);
      for (size_t index = 0; index < characteristicCount; ++index)
      {
        const CharacteristicDefinition &definition = characteristics[index];
        if (definition.def == nullptr || definition.def != context->chr) continue;
        request.characteristic.id = static_cast<uint16_t>(index + 1);
        request.serviceUuid = definition.serviceUuid;
        request.characteristicUuid = definition.uuid;
        break;
      }
      if (!request.characteristic.valid()) return;
    }
    EspBleImpl *ownerImpl = server->owner_->impl_;
    if (ownerImpl != nullptr)
    {
      request.connectionId = ownerImpl->findPeripheralConnectionId(connectionHandle);
    }
    server->dispatchRead(request);
  }

  // Held value behind the attribute the host is reading. Call with the mutex.

  const String *findValueLocked(const ble_gatt_access_ctxt *context) const
  {
    if (context->op == BLE_GATT_ACCESS_OP_READ_CHR)
    {
      for (size_t index = 0; index < characteristicCount; ++index)
      {
        const CharacteristicDefinition &definition = characteristics[index];
        if (definition.def != nullptr && definition.def == context->chr)
        {
          return &definition.value;
        }
      }
      return nullptr;
    }
    for (size_t index = 0; index < descriptorCount; ++index)
    {
      const DescriptorDefinition &definition = descriptors[index];
      if (definition.def != nullptr && definition.def == context->dsc)
      {
        return &definition.value;
      }
    }
    return nullptr;
  }

  // A peer turned Notification or Indication on or off for one of our
  // characteristics. Called from the global GAP listener on the host task.
  void handleSubscribe(
    uint16_t connectionHandle, uint16_t attributeHandle, bool notifications, bool indications)
  {
    EspBleGattCharacteristic handle;
    String serviceUuid;
    String characteristicUuid;
    {
      std::lock_guard<std::mutex> lock(mutex);
      for (size_t index = 0; index < characteristicCount; ++index)
      {
        const CharacteristicDefinition &definition = characteristics[index];
        if (definition.valueHandle == 0 || definition.valueHandle != attributeHandle) continue;
        handle.id = static_cast<uint16_t>(index + 1);
        serviceUuid = definition.serviceUuid;
        characteristicUuid = definition.uuid;
        break;
      }
      if (!handle.valid()) return; // not one of ours (HID and the GAP service have their own)
      recordSubscriptionLocked(connectionHandle, attributeHandle, notifications, indications);
    }
    if (server->owner_->impl_ != nullptr)
    {
      const uint16_t subscriptionValue =
        static_cast<uint16_t>((notifications ? 0x0001 : 0) | (indications ? 0x0002 : 0));
      server->owner_->impl_->queueServerSubscription(
        connectionHandle, handle, serviceUuid, characteristicUuid, subscriptionValue);
    }
  }

  void recordSubscriptionLocked(
    uint16_t connectionHandle, uint16_t valueHandle, bool notifications, bool indications)
  {
    SubscriptionSlot *free = nullptr;
    for (SubscriptionSlot &slot : subscriptions)
    {
      if (slot.used && slot.connectionHandle == connectionHandle &&
          slot.valueHandle == valueHandle)
      {
        if (!notifications && !indications)
        {
          slot = SubscriptionSlot();
          return;
        }
        slot.notifications = notifications;
        slot.indications = indications;
        return;
      }
      if (!slot.used && free == nullptr) free = &slot;
    }
    if (!notifications && !indications) return;
    // Out of slots: the peer is subscribed on air but we cannot remember it, so
    // its broadcasts would silently go nowhere. Count it like a dropped event.
    if (free == nullptr)
    {
      if (server->owner_->impl_ != nullptr) ++server->owner_->impl_->droppedEvents;
      return;
    }
    free->used = true;
    free->connectionHandle = connectionHandle;
    free->valueHandle = valueHandle;
    free->notifications = notifications;
    free->indications = indications;
  }

  void forgetSubscriptions(uint16_t connectionHandle)
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (SubscriptionSlot &slot : subscriptions)
    {
      if (slot.used && slot.connectionHandle == connectionHandle) slot = SubscriptionSlot();
    }
  }

  bool subscribed(uint16_t connectionHandle, uint16_t valueHandle, bool indication) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (const SubscriptionSlot &slot : subscriptions)
    {
      if (!slot.used || slot.connectionHandle != connectionHandle ||
          slot.valueHandle != valueHandle)
      {
        continue;
      }
      return indication ? slot.indications : slot.notifications;
    }
    return false;
  }

  explicit EspBleGattServerImpl(EspBleGattServer *server) : server(server) {}

  // One send. The value handle identifies the attribute; connectionId 0 means
  // "every subscribed peer". Runs on its own task because an indication waits
  // for the peer's confirmation.
  static void runSend(void *argument)
  {
    EspBleGattServerImpl *impl = static_cast<EspBleGattServerImpl *>(argument);
    EspBleGattSendResult result;
    uint16_t valueHandle = 0;
    EspBleConnectionId targetConnectionId = 0;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      valueHandle = impl->sendValueHandle;
      result.connectionId = impl->sendConnectionId;
      result.characteristic = impl->sendCharacteristic;
      result.serviceUuid = impl->sendServiceUuid;
      result.characteristicUuid = impl->sendCharacteristicUuid;
      result.value = impl->sendValue;
      result.indication = impl->sendIndication;
      targetConnectionId = impl->sendConnectionId;
    }

    // Collect the peripheral connections this send targets.
    uint16_t connectionHandles[ConnectionCapacity];
    size_t connectionCount = 0;
    bool anyPeripheral = false;
    EspBle *owner = impl->server->owner_;
    if (owner->impl_ != nullptr)
    {
      std::lock_guard<std::mutex> lock(owner->impl_->mutex);
      for (const EspBleImpl::ConnectionSlot &slot : owner->impl_->connections)
      {
        if (!slot.used || slot.connection.localRole != EspBleRole::Peripheral) continue;
        anyPeripheral = true;
        if (targetConnectionId != 0 && slot.connection.id != targetConnectionId) continue;
        if (connectionCount < ConnectionCapacity)
        {
          connectionHandles[connectionCount++] = slot.connection.handle;
        }
      }
    }

    if (valueHandle == 0)
    {
      result.success = false;
      result.error = EspBleError::NotFound;
      result.detail = "GATT characteristic was not registered";
    }
    else if (connectionCount == 0)
    {
      result.success = false;
      result.error = EspBleError::NotFound;
      result.detail = targetConnectionId != 0
        ? (anyPeripheral ? "target connection is not connected" : "no connected GATT Client")
        : "no connected GATT Client";
    }
    else
    {
      size_t sent = 0;
      size_t skipped = 0;
      for (size_t index = 0; index < connectionCount && result.detail.length() == 0; ++index)
      {
        const uint16_t connectionHandle = connectionHandles[index];
        // A broadcast reaches whoever subscribed. A connection-scoped send is an
        // explicit instruction, so it is not filtered.
        if (targetConnectionId == 0 &&
            !impl->subscribed(connectionHandle, valueHandle, result.indication))
        {
          ++skipped;
          continue;
        }
        impl->sendOne(connectionHandle, valueHandle, result);
        if (result.detail.length() != 0) break;
        ++sent;
      }
      if (result.detail.length() == 0)
      {
        if (sent != 0)
        {
          result.success = true;
        }
        else
        {
          result.success = false;
          result.error = EspBleError::InvalidState;
          result.detail = skipped != 0 ? "no subscribed GATT Client" : "no connected GATT Client";
        }
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
  }

  // vTaskDelete() never returns, so the body runs in its own function: the destructors
  // of its locals (Strings hold heap buffers) must run before the task ends, or every
  // operation leaks them.
  static void sendTaskEntry(void *argument)
  {
    runSend(argument);
    vTaskDelete(nullptr);
  }


  // Notify or indicate one connection. Fills result.error/detail on failure and
  // leaves them untouched on success, which is what the caller loops on.
  void sendOne(uint16_t connectionHandle, uint16_t valueHandle, EspBleGattSendResult &result)
  {
    os_mbuf *value = ble_hs_mbuf_from_flat(
      reinterpret_cast<const uint8_t *>(result.value.c_str()), result.value.length());
    if (value == nullptr)
    {
      result.success = false;
      result.error = EspBleError::ResourceExhausted;
      result.detail = "failed to allocate notify buffer";
      return;
    }
    if (!result.indication)
    {
      const int backendCode = ble_gatts_notify_custom(connectionHandle, valueHandle, value);
      if (backendCode != 0)
      {
        result.success = false;
        result.error = EspBleError::BackendFailure;
        result.detail = String("notify failed with backend code ") + backendCode;
      }
      return;
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      indicateConnectionHandle = connectionHandle;
      indicateAttributeHandle = valueHandle;
      indicateConfirmed = false;
      indicateStatus = 0;
    }
    const int backendCode = ble_gatts_indicate_custom(connectionHandle, valueHandle, value);
    if (backendCode != 0)
    {
      result.success = false;
      result.error = EspBleError::BackendFailure;
      result.detail = String("indicate failed with backend code ") + backendCode;
    }
    else
    {
      // The peer has 30 s to confirm per the spec; give up a little after that
      // rather than holding the send queue forever. The confirmation arrives as
      // BLE_GAP_EVENT_NOTIFY_TX on the global GAP listener.
      const uint32_t deadline = millis() + 31000;
      bool confirmed = false;
      int status = 0;
      while (millis() < deadline)
      {
        {
          std::lock_guard<std::mutex> lock(mutex);
          confirmed = indicateConfirmed;
          status = indicateStatus;
        }
        if (confirmed) break;
        delay(1);
      }
      if (!confirmed)
      {
        result.success = false;
        result.error = EspBleError::Timeout;
        result.detail = "indication confirmation timed out";
      }
      else if (status != 0)
      {
        result.success = false;
        result.error = EspBleError::BackendFailure;
        result.detail = String("indication confirmation failed with status ") + status;
      }
    }
    {
      std::lock_guard<std::mutex> lock(mutex);
      indicateConnectionHandle = 0xffff;
      indicateAttributeHandle = 0;
    }
  }

  EspBleGattServer *server;
  mutable std::mutex mutex;
  ServiceDefinition services[EspBleGattServer::MaxServices];
  size_t serviceCount = 0;
  CharacteristicDefinition characteristics[EspBleGattServer::MaxCharacteristics];
  size_t characteristicCount = 0;
  DescriptorDefinition descriptors[EspBleGattServer::MaxDescriptors];
  size_t descriptorCount = 0;
  // NimBLE keeps pointers to these tables for as long as the GATT server runs,
  // so they live here rather than on realize()'s stack. Each service's
  // characteristic run and each characteristic's descriptor run is terminated by
  // a zeroed entry, hence the extra slots.
  ble_gatt_svc_def serviceDefs[EspBleGattServer::MaxServices + 1] = {};
  ble_gatt_chr_def characteristicDefs[EspBleGattServer::MaxCharacteristics +
                                      EspBleGattServer::MaxServices] = {};
  ble_gatt_dsc_def descriptorDefs[EspBleGattServer::MaxDescriptors +
                                  EspBleGattServer::MaxCharacteristics] = {};
  SubscriptionSlot subscriptions[SubscriptionCapacity];
  bool realized = false;
  bool sending = false;
  TaskHandle_t sendTask = nullptr;
  uint16_t sendValueHandle = 0;
  EspBleGattCharacteristic sendCharacteristic;
  // Connection-scoped indication in flight: the confirmation arrives as a
  // BLE_GAP_EVENT_NOTIFY_TX on the global GAP listener, which matches on these.
  uint16_t indicateConnectionHandle = 0xffff;
  uint16_t indicateAttributeHandle = 0;
  bool indicateConfirmed = false;
  int indicateStatus = 0;
  String sendServiceUuid;
  String sendCharacteristicUuid;
  String sendValue;
  bool sendIndication = false;

  // Internal send FIFO: notify()/indicate() enqueue here instead of rejecting
  // when a send is already in flight. EspBle::pumpSendQueue() dequeues one at a
  // time from update() and runs it through sendTaskEntry, mirroring the GATT
  // client queue. connectionId 0 means broadcast.
  struct SendRequest
  {
    EspBleConnectionId connectionId = 0;
    uint16_t valueHandle = 0;
    EspBleGattCharacteristic characteristic;
    String serviceUuid;
    String characteristicUuid;
    String value;
    bool indication = false;
  };
  static constexpr size_t SendQueueCapacity = 8;
  SendRequest sendQueue[SendQueueCapacity];
  size_t sendQueueHead = 0;
  size_t sendQueueCount = 0;
  EspBleConnectionId sendConnectionId = 0;
};

void espBleHandleServerSubscribe(
  EspBle *owner, uint16_t connectionHandle, uint16_t attributeHandle, bool notifications,
  bool indications)
{
  EspBleGattServerImpl *server = EspBleImpl::serverImplOf(owner);
  if (server == nullptr) return;
  server->handleSubscribe(connectionHandle, attributeHandle, notifications, indications);
}

void espBleForgetServerSubscriptions(EspBle *owner, uint16_t connectionHandle)
{
  EspBleGattServerImpl *server = EspBleImpl::serverImplOf(owner);
  if (server == nullptr) return;
  server->forgetSubscriptions(connectionHandle);
}

void espBleConfirmIndication(
  EspBle *owner, uint16_t connectionHandle, uint16_t attributeHandle, int status)
{
  EspBleGattServerImpl *server = EspBleImpl::serverImplOf(owner);
  if (server == nullptr) return;
  std::lock_guard<std::mutex> lock(server->mutex);
  if (server->indicateAttributeHandle == 0 ||
      server->indicateConnectionHandle != connectionHandle ||
      server->indicateAttributeHandle != attributeHandle)
  {
    return;
  }
  server->indicateStatus = status;
  server->indicateConfirmed = true;
}

struct EspBleHidDeviceManagerImpl
{
  static constexpr size_t OutputQueueCapacity = 8;
  static constexpr size_t ProfileCount = 6;
  static constexpr size_t MaxVendorReportSize = 64;
  static constexpr size_t MaxCustomReports = 4;
  static constexpr size_t CustomReportMapCapacity = 256;

  struct VendorReportEntry
  {
    EspBleConnectionId connectionId = 0;
    uint8_t reportId = 0;
    uint8_t reportType = 0;
    uint8_t data[MaxVendorReportSize] = {};
    size_t length = 0;
  };

  // A custom (user-declared) HID report composed into the same HID service.
  struct CustomReport
  {
    uint8_t reportId = 0;
    uint8_t reportType = 0; // ESP_BLE_HID_REPORT_TYPE_*
    uint16_t size = 0;      // report payload size in bytes
    uint16_t valueHandle = 0;
    uint8_t value[MaxVendorReportSize] = {};
    size_t length = 0;
    ble_gatt_chr_def *characteristic = nullptr;
    ble_gatt_dsc_def descriptors[2] = {};
  };

  // CCCD subscription state per connection, tracked from GAP subscribe
  // events. Reports are only notified to subscribed peers.
  struct SubscriptionSlot
  {
    bool used = false;
    uint16_t connectionHandle = 0xffff;
    uint8_t inputNotifications = 0;
    bool batteryNotifications = false;
    bool bootKeyboardInputNotifications = false;
    uint8_t customInputNotifications = 0; // bitmask by custom report slot index
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
      // Protocol Mode resets to Report Protocol Mode for the next connection.
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->protocolMode = EspBleHidKeyboard::ReportProtocolMode;
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
    const bool bootKeyboard =
      bootKeyboardInputValueHandle != 0 && attributeHandle == bootKeyboardInputValueHandle;
    int customSlot = -1;
    for (size_t index = 0; index < customReportCount; ++index)
    {
      if (customReports[index].valueHandle != 0 &&
          attributeHandle == customReports[index].valueHandle)
      {
        customSlot = static_cast<int>(index);
        break;
      }
    }
    if (reportId == 0 && !bootKeyboard && customSlot < 0 &&
        (attributeHandle != batteryValueHandle || batteryValueHandle == 0))
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
          candidate.customInputNotifications = 0;
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
    else if (bootKeyboard)
    {
      slot->bootKeyboardInputNotifications = notifications;
    }
    else if (customSlot >= 0)
    {
      const uint8_t mask = static_cast<uint8_t>(1u << customSlot);
      if (notifications) slot->customInputNotifications |= mask;
      else slot->customInputNotifications &= static_cast<uint8_t>(~mask);
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

  bool subscribedBootKeyboard(uint16_t connectionHandle) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (const SubscriptionSlot &slot : subscriptions)
    {
      if (slot.used && slot.connectionHandle == connectionHandle)
      {
        return slot.bootKeyboardInputNotifications;
      }
    }
    return false;
  }

  bool subscribedCustom(uint16_t connectionHandle, size_t slotIndex) const
  {
    std::lock_guard<std::mutex> lock(mutex);
    for (const SubscriptionSlot &slot : subscriptions)
    {
      if (slot.used && slot.connectionHandle == connectionHandle)
      {
        return (slot.customInputNotifications &
                static_cast<uint8_t>(1u << slotIndex)) != 0;
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
      if (context->chr == protocolModeCharacteristic)
      {
        return appendValue(context->om, &protocolMode, 1);
      }
      if (context->chr == bootKeyboardInputCharacteristic)
      {
        return appendValue(context->om, bootKeyboardInput, sizeof(bootKeyboardInput));
      }
      if (context->chr == bootKeyboardOutputCharacteristic)
      {
        return appendValue(context->om, &bootKeyboardOutput, 1);
      }
      for (size_t index = 0; index < customReportCount; ++index)
      {
        if (context->chr == customReports[index].characteristic)
        {
          return appendValue(context->om, customReports[index].value,
                             customReports[index].length);
        }
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
      if (context->chr == protocolModeCharacteristic)
      {
        uint16_t length = 0;
        uint8_t value = 0;
        if (ble_hs_mbuf_to_flat(context->om, &value, 1, &length) != 0 || length != 1)
        {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        if (value != EspBleHidKeyboard::BootProtocolMode &&
            value != EspBleHidKeyboard::ReportProtocolMode)
        {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        {
          std::lock_guard<std::mutex> lock(mutex);
          protocolMode = value;
        }
        queueProtocolMode(connectionHandle, value);
        return 0;
      }
      if (context->chr == bootKeyboardOutputCharacteristic)
      {
        uint16_t length = 0;
        uint8_t value = 0;
        if (ble_hs_mbuf_to_flat(context->om, &value, 1, &length) != 0 || length != 1)
        {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        {
          std::lock_guard<std::mutex> lock(mutex);
          bootKeyboardOutput = value;
        }
        queueOutputReport(connectionHandle, value);
        return 0;
      }
      for (size_t index = 0; index < customReportCount; ++index)
      {
        if (context->chr != customReports[index].characteristic) continue;
        uint8_t value[MaxVendorReportSize] = {};
        uint16_t length = 0;
        if (ble_hs_mbuf_to_flat(context->om, value, sizeof(value), &length) != 0 ||
            length != customReports[index].size)
        {
          return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        {
          std::lock_guard<std::mutex> lock(mutex);
          memcpy(customReports[index].value, value, length);
          customReports[index].length = length;
        }
        queueCustomReport(connectionHandle, customReports[index].reportId,
                          customReports[index].reportType, value, length);
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
      for (size_t index = 0; index < customReportCount; ++index)
      {
        if (context->dsc == &customReports[index].descriptors[0])
        {
          const uint8_t reference[] = {customReports[index].reportId,
                                       customReports[index].reportType};
          return appendValue(context->om, reference, sizeof(reference));
        }
      }
    }
    return BLE_ATT_ERR_UNLIKELY;
  }

  void queueOutputReport(uint16_t connectionHandle, uint8_t leds)
  {
    EspBleHidKeyboardOutputReport report;
    report.setLeds(leds);
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
    // Both protocol modes reach this point (the Report-protocol LED Output
    // Report and the Boot Keyboard Output Report store their byte in different
    // fields), so recording ledState here needs no protocol-mode branch.
    // Updated before the queue rather than at dispatch: ledState() is meant to
    // be polled, so a queue overflow that drops the callback must not leave it
    // stale. The cost is that it can be one update() ahead of the callback.
    ledState = report;
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
    report.reportId = ESP_BLE_HID_REPORT_ID_VENDOR;
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

  void queueCustomReport(
    uint16_t connectionHandle, uint8_t reportId, uint8_t reportType,
    const uint8_t *data, size_t length)
  {
    VendorReportEntry report;
    report.reportId = reportId;
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
      ++droppedCustomEvents;
      return;
    }
    std::lock_guard<std::mutex> lock(mutex);
    if (customEventCount == OutputQueueCapacity)
    {
      ++droppedCustomEvents;
      return;
    }
    const size_t tail = (customEventHead + customEventCount) % OutputQueueCapacity;
    customEvents[tail] = report;
    ++customEventCount;
  }

  void queueProtocolMode(uint16_t connectionHandle, uint8_t mode)
  {
    EspBleConnectionId connectionId = 0;
    if (device->owner_->impl_ != nullptr)
    {
      std::lock_guard<std::mutex> connectionLock(device->owner_->impl_->mutex);
      connectionId = device->owner_->impl_->findPeripheralConnectionId(connectionHandle);
    }
    // Protocol Mode is latched state, not a stream: keep only the latest write.
    std::lock_guard<std::mutex> lock(mutex);
    protocolModeEventPending = true;
    protocolModeEventValue = mode;
    protocolModeEventConnectionId = connectionId;
  }

  EspBleHidKeyboard *device;
  mutable std::mutex mutex;
  EspBleHidDeviceConfig config;
  bool configured = false;
  uint8_t profileMask = 0;
  uint8_t mouseButtonCount = 5;
  uint8_t vendorReportSize = 63;
  bool keyboardNkro = false;
  bool realized = false;
  uint16_t inputValueHandles[ProfileCount] = {};
  uint16_t outputValueHandle = 0;
  uint16_t batteryValueHandle = 0;
  uint16_t bootKeyboardInputValueHandle = 0;
  uint8_t inputValues[ProfileCount][MaxVendorReportSize] = {};
  uint8_t inputLengths[ProfileCount] = {8, 4, 11, 2, 1, 63};
  uint8_t outputValue = 0;
  // The LED Output Report a host last wrote, in either protocol mode, with the
  // connection it came from. Serves ledState().
  EspBleHidKeyboardOutputReport ledState;
  bool bootProtocolEnabled = false;
  uint8_t protocolMode = EspBleHidKeyboard::ReportProtocolMode;
  uint8_t bootKeyboardInput[8] = {};
  uint8_t bootKeyboardOutput = 0;
  bool protocolModeEventPending = false;
  uint8_t protocolModeEventValue = EspBleHidKeyboard::ReportProtocolMode;
  EspBleConnectionId protocolModeEventConnectionId = 0;
  uint8_t vendorOutputValue[MaxVendorReportSize] = {};
  size_t vendorOutputLength = 0;
  uint8_t vendorFeatureValue[MaxVendorReportSize] = {};
  size_t vendorFeatureLength = 0;
  uint8_t batteryLevel = 100;
  uint8_t hidInformation[4] = {0x11, 0x01, 0x00, 0x01};
  uint8_t pnpId[7] = {};
  uint8_t reportMap[640] = {};
  size_t reportMapLength = 0;
  ble_uuid16_t hidServiceUuid = BLE_UUID16_INIT(0x1812);
  ble_uuid16_t deviceInformationServiceUuid = BLE_UUID16_INIT(0x180a);
  ble_uuid16_t batteryServiceUuid = BLE_UUID16_INIT(0x180f);
  ble_uuid16_t hidInformationUuid = BLE_UUID16_INIT(0x2a4a);
  ble_uuid16_t reportMapUuid = BLE_UUID16_INIT(0x2a4b);
  ble_uuid16_t hidControlPointUuid = BLE_UUID16_INIT(0x2a4c);
  ble_uuid16_t protocolModeUuid = BLE_UUID16_INIT(0x2a4e);
  ble_uuid16_t bootKeyboardInputUuid = BLE_UUID16_INIT(0x2a22);
  ble_uuid16_t bootKeyboardOutputUuid = BLE_UUID16_INIT(0x2a32);
  ble_uuid16_t reportUuid = BLE_UUID16_INIT(0x2a4d);
  ble_uuid16_t reportReferenceUuid = BLE_UUID16_INIT(0x2908);
  ble_uuid16_t manufacturerUuid = BLE_UUID16_INIT(0x2a29);
  ble_uuid16_t pnpIdUuid = BLE_UUID16_INIT(0x2a50);
  ble_uuid16_t batteryLevelUuid = BLE_UUID16_INIT(0x2a19);
  ble_gatt_dsc_def inputDescriptors[ProfileCount][2] = {};
  ble_gatt_dsc_def outputDescriptors[2] = {};
  ble_gatt_dsc_def vendorOutputDescriptors[2] = {};
  ble_gatt_dsc_def vendorFeatureDescriptors[2] = {};
  ble_gatt_chr_def hidCharacteristics[24] = {};
  ble_gatt_chr_def *inputCharacteristics[ProfileCount] = {};
  ble_gatt_chr_def *outputCharacteristic = nullptr;
  ble_gatt_chr_def *vendorOutputCharacteristic = nullptr;
  ble_gatt_chr_def *vendorFeatureCharacteristic = nullptr;
  ble_gatt_chr_def *protocolModeCharacteristic = nullptr;
  ble_gatt_chr_def *bootKeyboardInputCharacteristic = nullptr;
  ble_gatt_chr_def *bootKeyboardOutputCharacteristic = nullptr;
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
  CustomReport customReports[MaxCustomReports];
  size_t customReportCount = 0;
  bool customConfigured = false;
  uint8_t customReportMap[CustomReportMapCapacity] = {};
  size_t customReportMapLength = 0;
  VendorReportEntry customEvents[OutputQueueCapacity];
  size_t customEventHead = 0;
  size_t customEventCount = 0;
  size_t droppedCustomEvents = 0;
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
    bool keyboardBitmap = false;
    bool keyboardHasModifiers = false;
    uint16_t keyboardModifierBitOffset = 0;
    uint16_t keyboardBitmapBitOffset = 0;
    uint16_t keyboardBitmapBitCount = 0;
    uint16_t keyboardBitmapUsageMinimum = 0;
    EspBleHidReportField fields[MaxFieldsPerReport];
    size_t fieldCount = 0;
  };

  struct Connection
  {
    bool used = false;
    EspBleConnectionId connectionId = 0;
    // Attribute handles, not wrapper objects: a peer may expose several Report
    // characteristics with the same UUID, which only a handle can tell apart.
    // Zero means the peer does not have that report.
    uint16_t connectionHandle = 0xffff;
    uint8_t reportId = 0;
    uint16_t inputReport = 0;
    uint16_t outputReport = 0;
    bool outputReportNoResponse = false;
    uint16_t vendorOutputReport = 0;
    bool vendorOutputReportNoResponse = false;
    uint16_t vendorFeatureReport = 0;
    uint16_t inputReports[MaxInputReports] = {};
    EspBleHidReportKind inputKinds[MaxInputReports] = {};
    uint8_t inputReportIds[MaxInputReports] = {};
    ReportFormat inputFormats[MaxInputReports];
    size_t inputReportCount = 0;
    uint8_t mouseButtons = 0;
    uint16_t consumerUsage = 0;
    uint8_t systemUsage = 0;
    uint8_t gamepadData[64] = {};
    size_t gamepadLength = 0;
    uint8_t bitmap[EspBleHidKeyboardState::BitmapSize] = {};
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
    ReportFormat format;
    bool formatFound = false;
    {
      std::lock_guard<std::mutex> lock(mutex);
      Connection *connection = findConnection(connectionId);
      if (connection != nullptr)
      {
        for (size_t index = 0; index < connection->inputReportCount; ++index)
        {
          if (connection->inputKinds[index] == EspBleHidReportKind::Keyboard &&
              connection->inputReportIds[index] == reportId)
          {
            format = connection->inputFormats[index];
            formatFound = true;
            break;
          }
        }
      }
    }
    if (data == nullptr || !formatFound || format.inputBitLength == 0 ||
        length != (format.inputBitLength + 7) / 8)
    {
      // Count instead of vanishing: an unexpected report length otherwise
      // shows up as "discovery succeeded but no keys arrive".
      std::lock_guard<std::mutex> lock(mutex);
      ++invalidInputReports;
      return;
    }
    if (!format.keyboardBitmap)
    {
      if (length != 8)
      {
        std::lock_guard<std::mutex> lock(mutex);
        ++invalidInputReports;
        return;
      }
      for (size_t index = 0; index < 6; ++index)
      {
        const uint8_t usage = data[index + 2];
        if (usage >= 0x01 && usage <= 0x03) return;
      }
    }

    Event event;
    event.type = EventType::State;
    event.state.connectionId = connectionId;
    event.state.reportId = reportId;
    if (format.keyboardBitmap)
    {
      if (format.keyboardHasModifiers)
      {
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
          const size_t sourceBit = format.keyboardModifierBitOffset + bit;
          if ((data[sourceBit >> 3] & static_cast<uint8_t>(1u << (sourceBit & 7))) != 0)
            event.state.modifiers |= static_cast<uint8_t>(1u << bit);
        }
      }
      for (uint16_t bit = 0; bit < format.keyboardBitmapBitCount; ++bit)
      {
        const uint16_t usage = format.keyboardBitmapUsageMinimum + bit;
        const size_t sourceBit = format.keyboardBitmapBitOffset + bit;
        if (usage < 256 &&
            (data[sourceBit >> 3] & static_cast<uint8_t>(1u << (sourceBit & 7))) != 0)
          event.state.bitmap[usage >> 3] |= static_cast<uint8_t>(1u << (usage & 7));
      }
    }
    else
    {
      event.state.modifiers = data[0];
      for (size_t index = 0; index < 6; ++index)
      {
        const uint8_t usage = data[index + 2];
        if (usage >= 0x04)
          event.state.bitmap[usage >> 3] |= static_cast<uint8_t>(1u << (usage & 7));
      }
    }
    for (uint8_t bit = 0; bit < 8; ++bit)
      if ((event.state.modifiers & static_cast<uint8_t>(1u << bit)) != 0)
      {
        const uint8_t usage = static_cast<uint8_t>(0xe0 + bit);
        event.state.bitmap[usage >> 3] |= static_cast<uint8_t>(1u << (usage & 7));
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
        event.state.changedBitmap[index] = connection->bitmap[index] ^ event.state.bitmap[index];
        changed = changed || event.state.changedBitmap[index] != 0;
      }
      if (!changed)
      {
        return;
      }
      memcpy(connection->bitmap, event.state.bitmap, sizeof(connection->bitmap));
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

  // One Report characteristic as the attribute table describes it, before its
  // Report Reference descriptor says what it is for.
  struct ReportCandidate
  {
    uint16_t valueHandle = 0;
    uint16_t referenceHandle = 0;
    uint16_t configurationHandle = 0;
    bool notifiable = false;
    bool writable = false;
    bool writableWithoutResponse = false;
  };

  // Notifications for a subscribed input report, straight from the host task.
  static void inputReportConsumer(
    void *owner,
    EspBleConnectionId connectionId,
    uint16_t valueHandle,
    const uint8_t *data,
    size_t length)
  {
    EspBleHidKeyboardHostImpl *impl = static_cast<EspBleHidKeyboardHostImpl *>(owner);
    uint8_t reportId = 0;
    EspBleHidReportKind kind = EspBleHidReportKind::Unknown;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      Connection *connection = impl->findConnection(connectionId);
      if (connection == nullptr) return;
      bool found = false;
      for (size_t index = 0; index < connection->inputReportCount; ++index)
      {
        if (connection->inputReports[index] != valueHandle) continue;
        reportId = connection->inputReportIds[index];
        kind = connection->inputKinds[index];
        found = true;
        break;
      }
      if (!found) return;
    }
    if (kind == EspBleHidReportKind::Keyboard)
    {
      impl->queueInputReport(connectionId, reportId, data, length);
    }
    else
    {
      impl->queueRawReport(connectionId, reportId, kind, data, length);
    }
  }

  static void runDiscovery(void *argument)
  {
    EspBleHidKeyboardHostImpl *impl = static_cast<EspBleHidKeyboardHostImpl *>(argument);
    EspBleImpl *owner = impl->host->owner_->impl_;
    EspBleHidKeyboardHostDiscovery result;
    uint16_t connectionHandle = 0xffff;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      result.connectionId = impl->discoveryConnectionId;
    }
    {
      std::lock_guard<std::mutex> lock(owner->mutex);
      for (const EspBleImpl::ConnectionSlot &slot : owner->connections)
      {
        if (slot.used && slot.connection.id == result.connectionId &&
            slot.connection.localRole == EspBleRole::Central)
        {
          connectionHandle = slot.connection.handle;
          break;
        }
      }
    }

    if (connectionHandle == 0xffff)
    {
      result.error = EspBleError::InvalidState;
      result.detail = "connection is not an active Central connection";
      publishDiscovery(impl, result);
      return;
    }

    // The attribute table is discovered once per connection and shared with the
    // generic GATT client, so a HID Host and an application both looking at the
    // same peer walk it once.
    bool discovered = false;
    {
      std::lock_guard<std::mutex> lock(owner->mutex);
      const EspBleImpl::GattDatabaseSnapshot *database =
        owner->findDatabaseLocked(result.connectionId);
      discovered = database != nullptr && database->valid;
    }
    if (!discovered)
    {
      EspBleGattResult discovery;
      discovery.connectionId = result.connectionId;
      if (!EspBleImpl::discoverDatabase(owner, discovery))
      {
        result.error = discovery.error;
        result.detail = discovery.detail;
        publishDiscovery(impl, result);
        return;
      }
    }

    // Copy what is needed out of the snapshot, so the ATT reads below run
    // without holding the owner's lock.
    uint16_t reportMapHandle = 0;
    uint16_t hidInformationHandle = 0;
    uint16_t batteryHandle = 0;
    ReportCandidate candidates[MaxInputReports * 3];
    size_t candidateCount = 0;
    {
      std::lock_guard<std::mutex> lock(owner->mutex);
      const EspBleImpl::GattDatabaseSnapshot *database =
        owner->findDatabaseLocked(result.connectionId);
      if (database == nullptr)
      {
        result.error = EspBleError::InvalidState;
        result.detail = "the GATT database snapshot went away";
        publishDiscovery(impl, result);
        return;
      }
      for (size_t index = 0; index < database->characteristicCount; ++index)
      {
        const EspBleGattCharacteristicInfo &info = database->characteristics[index];
        if (uuidEquals(info.serviceUuid, "180f") &&
            uuidEquals(info.characteristicUuid, "2a19") && info.readable)
        {
          batteryHandle = info.handle;
          continue;
        }
        if (!uuidEquals(info.serviceUuid, "1812")) continue;
        if (uuidEquals(info.characteristicUuid, "2a4b"))
        {
          reportMapHandle = info.handle;
        }
        else if (uuidEquals(info.characteristicUuid, "2a4a"))
        {
          hidInformationHandle = info.handle;
        }
        else if (uuidEquals(info.characteristicUuid, "2a4d") &&
                 candidateCount < (sizeof(candidates) / sizeof(candidates[0])))
        {
          ReportCandidate &candidate = candidates[candidateCount++];
          candidate.valueHandle = info.handle;
          candidate.notifiable = info.notifiable;
          candidate.writable = info.writable;
          candidate.writableWithoutResponse = info.writableWithoutResponse;
          for (size_t d = 0; d < database->descriptorCount; ++d)
          {
            const EspBleGattDescriptorInfo &descriptor = database->descriptors[d];
            // Matched by the characteristic's value handle: a UUID cannot tell
            // two Report characteristics apart.
            if (descriptor.characteristicHandle != info.handle) continue;
            if (uuidEquals(descriptor.descriptorUuid, "2908"))
            {
              candidate.referenceHandle = descriptor.handle;
            }
            else if (uuidEquals(
                       descriptor.descriptorUuid, EspBle::ClientCharacteristicConfigurationUuid))
            {
              candidate.configurationHandle = descriptor.handle;
            }
          }
        }
      }
    }

    if (reportMapHandle == 0)
    {
      result.error = EspBleError::NotFound;
      result.detail = "HID Service was not found";
      publishDiscovery(impl, result);
      return;
    }

    String reportMapValue;
    espble_raw::readHandle(connectionHandle, reportMapHandle, reportMapValue);
    const EspBleHidReportMapInfo mapInfo = espBleParseHidReportMap(
      reinterpret_cast<const uint8_t *>(reportMapValue.c_str()), reportMapValue.length());
    if (mapInfo.count == 0)
    {
      result.error = EspBleError::InvalidState;
      result.detail = "HID Report Map has no supported input report";
      publishDiscovery(impl, result);
      return;
    }

    if (hidInformationHandle != 0)
    {
      String hidInformationValue;
      if (espble_raw::readHandle(connectionHandle, hidInformationHandle, hidInformationValue) &&
          hidInformationValue.length() >= 3)
      {
        result.hasCountryCode = true;
        result.countryCode = static_cast<uint8_t>(hidInformationValue[2]);
      }
    }

    uint16_t inputReports[MaxInputReports] = {};
    EspBleHidReportKind inputKinds[MaxInputReports] = {};
    uint8_t inputReportIds[MaxInputReports] = {};
    uint16_t inputConfigurations[MaxInputReports] = {};
    size_t inputReportCount = 0;
    uint16_t outputReports[MaxInputReports] = {};
    bool outputNoResponse[MaxInputReports] = {};
    uint8_t outputReportIds[MaxInputReports] = {};
    size_t outputReportCount = 0;
    uint16_t featureReports[MaxInputReports] = {};
    uint8_t featureReportIds[MaxInputReports] = {};
    size_t featureReportCount = 0;
    bool keyboardInputFound = false;

    for (size_t index = 0; index < candidateCount; ++index)
    {
      const ReportCandidate &candidate = candidates[index];
      String reference;
      if (candidate.referenceHandle != 0 &&
          espble_raw::readHandle(connectionHandle, candidate.referenceHandle, reference) &&
          reference.length() == 2)
      {
        const uint8_t reportId = static_cast<uint8_t>(reference[0]);
        const uint8_t reportType = static_cast<uint8_t>(reference[1]);
        const EspBleHidReportKind kind = mapInfo.kindForReportId(reportId);
        if (reportType == 1 && candidate.notifiable &&
            kind != EspBleHidReportKind::Unknown && inputReportCount < MaxInputReports)
        {
          inputReports[inputReportCount] = candidate.valueHandle;
          inputConfigurations[inputReportCount] = candidate.configurationHandle;
          inputKinds[inputReportCount] = kind;
          inputReportIds[inputReportCount++] = reportId;
          if (kind == EspBleHidReportKind::Keyboard)
          {
            result.reportId = reportId;
            keyboardInputFound = true;
          }
        }
        else if (reportType == 2 && outputReportCount < MaxInputReports)
        {
          outputReports[outputReportCount] = candidate.valueHandle;
          outputNoResponse[outputReportCount] = candidate.writableWithoutResponse;
          outputReportIds[outputReportCount++] = reportId;
        }
        else if (reportType == 3 && featureReportCount < MaxInputReports)
        {
          featureReports[featureReportCount] = candidate.valueHandle;
          featureReportIds[featureReportCount++] = reportId;
        }
      }
      else if (mapInfo.count == 1 && !mapInfo.entries[0].hasReportId)
      {
        // No Report Reference descriptor: acceptable when the Report Map
        // declares no report IDs (single-report keyboards).
        if (candidate.notifiable && inputReportCount == 0)
        {
          inputReports[0] = candidate.valueHandle;
          inputConfigurations[0] = candidate.configurationHandle;
          inputKinds[0] = mapInfo.entries[0].kind;
          inputReportIds[0] = 0;
          inputReportCount = 1;
          result.reportId = 0;
          keyboardInputFound = mapInfo.entries[0].kind == EspBleHidReportKind::Keyboard;
        }
        else if ((candidate.writable || candidate.writableWithoutResponse) &&
                 outputReportCount < MaxInputReports)
        {
          outputReports[outputReportCount] = candidate.valueHandle;
          outputNoResponse[outputReportCount] = candidate.writableWithoutResponse;
          outputReportIds[outputReportCount++] = 0;
        }
      }
    }

    if (inputReportCount == 0)
    {
      result.error = EspBleError::NotFound;
      result.detail = "supported HID Input Report was not found";
      publishDiscovery(impl, result);
      return;
    }

    uint16_t outputReport = 0;
    bool outputReportNoResponse = false;
    uint16_t vendorOutputReport = 0;
    bool vendorOutputReportNoResponse = false;
    uint16_t vendorFeatureReport = 0;
    for (size_t index = 0; index < outputReportCount; ++index)
    {
      if (keyboardInputFound && outputReportIds[index] == result.reportId)
      {
        outputReport = outputReports[index];
        outputReportNoResponse = outputNoResponse[index];
      }
      if (outputReportIds[index] == ESP_BLE_HID_REPORT_ID_VENDOR)
      {
        vendorOutputReport = outputReports[index];
        vendorOutputReportNoResponse = outputNoResponse[index];
      }
    }
    for (size_t index = 0; index < featureReportCount; ++index)
    {
      if (featureReportIds[index] == ESP_BLE_HID_REPORT_ID_VENDOR)
        vendorFeatureReport = featureReports[index];
    }
    result.hasOutputReport = outputReport != 0;

    if (batteryHandle != 0)
    {
      String batteryValue;
      if (espble_raw::readHandle(connectionHandle, batteryHandle, batteryValue) &&
          batteryValue.length() >= 1)
      {
        result.batteryLevel = static_cast<uint8_t>(batteryValue[0]);
        result.hasBatteryLevel = true;
      }
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
        connection->connectionHandle = connectionHandle;
        connection->reportId = result.reportId;
        connection->inputReport = 0;
        connection->outputReport = outputReport;
        connection->outputReportNoResponse = outputReportNoResponse;
        connection->vendorOutputReport = vendorOutputReport;
        connection->vendorOutputReportNoResponse = vendorOutputReportNoResponse;
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
            {
              const EspBleHidReportMapEntry &entry = mapInfo.entries[entryIndex];
              format.inputBitLength = entry.inputBitLength;
              format.keyboardBitmap = entry.keyboardBitmap;
              format.keyboardHasModifiers = entry.keyboardHasModifiers;
              format.keyboardModifierBitOffset = entry.keyboardModifierBitOffset;
              format.keyboardBitmapBitOffset = entry.keyboardBitmapBitOffset;
              format.keyboardBitmapBitCount = entry.keyboardBitmapBitCount;
              format.keyboardBitmapUsageMinimum = entry.keyboardBitmapUsageMinimum;
            }
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
        // Subscribing is a CCCD write; the host then routes the peer's
        // notifications here by attribute handle.
        static const uint8_t enableNotifications[2] = {0x01, 0x00};
        result.success = inputConfigurations[index] != 0 &&
          espble_raw::writeHandle(
            connectionHandle, inputConfigurations[index], enableNotifications,
            sizeof(enableNotifications), true) &&
          owner->rememberConsumerSubscription(
            result.connectionId, connectionHandle, inputReports[index],
            &EspBleHidKeyboardHostImpl::inputReportConsumer, impl);
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

    publishDiscovery(impl, result);
  }

  // Report the outcome and release both the HID Host's discovery slot and the
  // owner's GATT gate, whichever way discovery ended.
  static void publishDiscovery(
    EspBleHidKeyboardHostImpl *impl, const EspBleHidKeyboardHostDiscovery &result)
  {
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
  }

  // vTaskDelete() never returns, so the body runs in its own function: the destructors
  // of its locals (Strings hold heap buffers) must run before the task ends, or every
  // operation leaks them.
  static void discoveryTaskEntry(void *argument)
  {
    runDiscovery(argument);
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
  // An advertisement and its scan response arrive as two reports. A scannable
  // advertiser is therefore held here until its response arrives, so the
  // application sees one complete result instead of two partial ones.
  struct Pending
  {
    bool used = false;
    uint8_t address[6] = {};
    uint8_t addressType = 0;
    EspBleScanResult result;
  };
  // Enough for the advertisers in radio range at one moment; when it overflows
  // the oldest is reported as it stands rather than dropped.
  static constexpr size_t PendingCapacity = 8;

  explicit EspBleScannerImpl(EspBleScanner *scanner) : scanner(scanner) {}

  // Queue one finished result for dispatch from update(). Called on the host
  // task, so the queue is the only thing shared with the application.
  void publish(EspBleScanResult &&result)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (count == ScanQueueCapacity)
    {
      ++dropped;
      return;
    }
    const size_t tail = (head + count) % ScanQueueCapacity;
    queue[tail] = std::move(result);
    ++count;
  }

  Pending *findPending(const uint8_t address[6], uint8_t addressType)
  {
    for (Pending &entry : pending)
    {
      if (entry.used && entry.addressType == addressType &&
          memcmp(entry.address, address, 6) == 0)
      {
        return &entry;
      }
    }
    return nullptr;
  }

  void handleReport(const ble_gap_disc_desc &report)
  {
    const bool isScanResponse = report.event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP;
    Pending *entry = findPending(report.addr.val, report.addr.type);

    if (isScanResponse)
    {
      if (entry == nullptr)
      {
        // A scan response with no advertisement to attach it to: report what it
        // carries rather than discard it.
        EspBleScanResult result;
        result.address = formatAddress(report.addr.val);
        result.addressType = static_cast<EspBleAddressType>(report.addr.type);
        result.rssi = report.rssi;
        parseAdvertisingReport(report.data, report.length_data, result);
        publish(std::move(result));
        return;
      }
      entry->result.rssi = report.rssi;
      parseAdvertisingReport(report.data, report.length_data, entry->result);
      entry->used = false;
      publish(std::move(entry->result));
      entry->result = EspBleScanResult();
      return;
    }

    EspBleScanResult result;
    result.address = formatAddress(report.addr.val);
    result.addressType = static_cast<EspBleAddressType>(report.addr.type);
    result.rssi = report.rssi;
    // The PDU type says what the advertiser accepts; the payload cannot.
    result.connectable = report.event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
      report.event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND;
    result.scannable = report.event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
      report.event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_IND;
    parseAdvertisingReport(report.data, report.length_data, result);

    if (!activeScan || !result.scannable)
    {
      // Nothing more is coming: a passive scan never asks, and a
      // non-scannable advertiser has nothing to answer with.
      publish(std::move(result));
      return;
    }

    if (entry == nullptr)
    {
      for (Pending &candidate : pending)
      {
        if (!candidate.used)
        {
          entry = &candidate;
          break;
        }
      }
    }
    if (entry == nullptr)
    {
      // Table full: publish the oldest as it stands and take its slot.
      entry = &pending[pendingNext];
      pendingNext = (pendingNext + 1) % PendingCapacity;
      publish(std::move(entry->result));
    }
    entry->used = true;
    entry->addressType = report.addr.type;
    memcpy(entry->address, report.addr.val, 6);
    entry->result = std::move(result);
  }

  // Scannable advertisers that never answered the scan request are reported
  // when the scan ends, so they are not lost entirely.
  void flushPending()
  {
    for (Pending &entry : pending)
    {
      if (!entry.used) continue;
      entry.used = false;
      publish(std::move(entry.result));
      entry.result = EspBleScanResult();
    }
  }

  static int gapEvent(ble_gap_event *event, void *argument)
  {
    EspBleScannerImpl *impl = static_cast<EspBleScannerImpl *>(argument);
    if (event->type == BLE_GAP_EVENT_DISC)
    {
      impl->handleReport(event->disc);
    }
    else if (event->type == BLE_GAP_EVENT_DISC_COMPLETE)
    {
      impl->flushPending();
    }
    return 0;
  }

  mutable std::mutex mutex;
  EspBleScanResult queue[ScanQueueCapacity];
  size_t head = 0;
  size_t count = 0;
  size_t dropped = 0;
  // Touched only on the host task, inside the GAP callback.
  Pending pending[PendingCapacity];
  size_t pendingNext = 0;
  bool activeScan = true;
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

bool EspBleScanResult::hasServiceData() const
{
  return serviceDataCount != 0;
}

bool EspBleScanResult::serviceDataFor(const char *uuid, String &data) const
{
  if (uuid == nullptr || uuid[0] == '\0')
  {
    return false;
  }
  for (size_t index = 0; index < serviceDataCount; ++index)
  {
    if (uuidEquals(serviceData[index].uuid, uuid))
    {
      data = serviceData[index].data;
      return true;
    }
  }
  return false;
}

bool EspBleScanResult::hasAppearance() const
{
  return appearance != 0;
}

bool EspBleScanResult::hasTxPowerLevel() const
{
  return txPowerLevelPresent;
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

void EspBleAdvertisingData::clear()
{
  name_ = "";
  manufacturerData_ = "";
  for (EspBleServiceData &block : serviceData_)
  {
    block = EspBleServiceData();
  }
  serviceDataCount_ = 0;
  serviceUuidCount_ = 0;
  appearance_ = 0;
  txPowerIncluded_ = false;
}

void EspBleAdvertisingData::setName(const char *name)
{
  name_ = name == nullptr ? "" : name;
}

bool EspBleAdvertisingData::addServiceUuid(const char *uuid)
{
  if (uuid == nullptr || uuid[0] == '\0')
  {
    return false;
  }
  for (size_t index = 0; index < serviceUuidCount_; ++index)
  {
    if (uuidEquals(serviceUuids_[index], uuid))
    {
      return true;
    }
  }
  if (serviceUuidCount_ == MaxServiceUuids)
  {
    return false;
  }

  serviceUuids_[serviceUuidCount_++] = uuid;
  return true;
}

void EspBleAdvertisingData::setManufacturerData(const uint8_t *data, size_t length)
{
  if (data == nullptr || length == 0)
  {
    manufacturerData_ = "";
    return;
  }
  manufacturerData_ = String(reinterpret_cast<const char *>(data), length);
}

bool EspBleAdvertisingData::addServiceData(const char *uuid, const uint8_t *data, size_t length)
{
  if (uuid == nullptr || uuid[0] == '\0')
  {
    return false;
  }

  size_t slot = serviceDataCount_;
  for (size_t index = 0; index < serviceDataCount_; ++index)
  {
    if (uuidEquals(serviceData_[index].uuid, uuid))
    {
      slot = index;
      break;
    }
  }

  if (data == nullptr || length == 0)
  {
    // Removing a block that was never added is not an error.
    if (slot == serviceDataCount_)
    {
      return true;
    }
    for (size_t next = slot + 1; next < serviceDataCount_; ++next)
    {
      serviceData_[next - 1] = serviceData_[next];
    }
    serviceData_[--serviceDataCount_] = EspBleServiceData();
    return true;
  }

  if (slot == serviceDataCount_)
  {
    if (serviceDataCount_ == MaxServiceData)
    {
      return false;
    }
    ++serviceDataCount_;
  }
  serviceData_[slot].uuid = uuid;
  serviceData_[slot].data = String(reinterpret_cast<const char *>(data), length);
  return true;
}

void EspBleAdvertisingData::setAppearance(uint16_t appearance)
{
  appearance_ = appearance;
}

void EspBleAdvertisingData::setTxPowerIncluded(bool included)
{
  txPowerIncluded_ = included;
}

bool EspBleAdvertisingData::isEmpty() const
{
  return name_.isEmpty() && manufacturerData_.isEmpty() && serviceDataCount_ == 0 &&
    serviceUuidCount_ == 0 && appearance_ == 0 && !txPowerIncluded_;
}

EspBleAdvertising::EspBleAdvertising(EspBle *owner) : owner_(owner) {}

void EspBleAdvertising::clear()
{
  data_.clear();
  scanResponseData_.clear();
  filterPolicy_ = EspBleAdvertisingFilterPolicy::Any;
  scanResponseEnabled_ = true;
  connectable_ = true;
  intervalMinMs_ = 0;
  intervalMaxMs_ = 0;
}

EspBleAdvertisingData &EspBleAdvertising::data()
{
  return data_;
}

EspBleAdvertisingData &EspBleAdvertising::scanResponse()
{
  return scanResponseData_;
}

void EspBleAdvertising::setName(const char *name)
{
  data_.setName(name);
}

bool EspBleAdvertising::addServiceUuid(const char *uuid)
{
  if (uuid == nullptr || uuid[0] == '\0')
  {
    owner_->setError(EspBleError::InvalidArgument, "service UUID is empty");
    return false;
  }
  if (!data_.addServiceUuid(uuid))
  {
    owner_->setError(EspBleError::ResourceExhausted, "too many advertising service UUIDs");
    return false;
  }
  owner_->clearError();
  return true;
}

void EspBleAdvertising::setManufacturerData(const uint8_t *data, size_t length)
{
  data_.setManufacturerData(data, length);
}

bool EspBleAdvertising::addServiceData(const char *uuid, const uint8_t *data, size_t length)
{
  if (uuid == nullptr || uuid[0] == '\0')
  {
    owner_->setError(EspBleError::InvalidArgument, "service data UUID is required");
    return false;
  }
  if (!data_.addServiceData(uuid, data, length))
  {
    owner_->setError(EspBleError::ResourceExhausted, "too many advertising service data blocks");
    return false;
  }
  owner_->clearError();
  return true;
}

void EspBleAdvertising::setAppearance(uint16_t appearance)
{
  data_.setAppearance(appearance);
}

void EspBleAdvertising::setScanResponseEnabled(bool enabled)
{
  scanResponseEnabled_ = enabled;
}

void EspBleAdvertising::setFilterPolicy(EspBleAdvertisingFilterPolicy policy)
{
  filterPolicy_ = policy;
}

EspBleAdvertisingFilterPolicy EspBleAdvertising::filterPolicy() const
{
  return filterPolicy_;
}

void EspBleAdvertising::setConnectable(bool connectable)
{
  connectable_ = connectable;
}

bool EspBleAdvertising::setInterval(uint16_t minMilliseconds, uint16_t maxMilliseconds)
{
  // BLE advertising interval range is 20 ms .. 10.24 s.
  if (minMilliseconds < 20 || maxMilliseconds > 10240 || minMilliseconds > maxMilliseconds)
  {
    owner_->setError(EspBleError::InvalidArgument,
      "advertising interval must be 20..10240 ms with min <= max");
    return false;
  }
  intervalMinMs_ = minMilliseconds;
  intervalMaxMs_ = maxMilliseconds;
  owner_->clearError();
  return true;
}

bool EspBleAdvertising::buildPayload(
  const EspBleAdvertisingData &source,
  Payload &destination,
  bool includeFlags,
  const char *payloadName) const
{
  const auto fail = [this, payloadName](const char *field) {
    String detail(field);
    detail += " does not fit in the 31-byte ";
    detail += payloadName;
    detail += " payload";
    owner_->setError(EspBleError::InvalidArgument, detail.c_str());
    return false;
  };

  // One AD structure: length, type, value. Fails when the 31-byte budget is
  // exhausted, which is what the caller reports per field.
  const auto append =
    [&destination](uint8_t type, const uint8_t *value, size_t valueLength) {
      if (valueLength > 254) return false;
      if (destination.length + 2 + valueLength > Payload::Capacity) return false;
      destination.bytes[destination.length++] = static_cast<uint8_t>(valueLength + 1);
      destination.bytes[destination.length++] = type;
      if (valueLength != 0)
      {
        memcpy(destination.bytes + destination.length, value, valueLength);
        destination.length += valueLength;
      }
      return true;
    };

  if (includeFlags)
  {
    // Flags are only valid in the advertising payload, never in a scan response.
    const uint8_t flags = 0x06; // General Discoverable, BR/EDR not supported.
    if (!append(AdTypeFlags, &flags, 1)) return fail("flags");
  }
  if (source.txPowerIncluded_)
  {
    const int8_t power = owner_->txPower();
    const uint8_t value = static_cast<uint8_t>(power == INT8_MIN ? 0 : power);
    if (!append(AdTypeTxPowerLevel, &value, 1)) return fail("Tx Power Level");
  }
  if (source.appearance_ != 0)
  {
    const uint8_t value[2] = {
      static_cast<uint8_t>(source.appearance_ & 0xff),
      static_cast<uint8_t>(source.appearance_ >> 8)};
    if (!append(AdTypeAppearance, value, sizeof(value))) return fail("appearance");
  }
  if (source.serviceUuidCount_ > 0)
  {
    // CSS Part A 1.1: a data type must not occur more than once in a payload,
    // so all UUIDs of one size share a single "Complete List" AD structure.
    uint8_t uuids16[EspBleAdvertisingData::MaxServiceUuids * 2];
    uint8_t uuids32[EspBleAdvertisingData::MaxServiceUuids * 4];
    uint8_t uuids128[EspBleAdvertisingData::MaxServiceUuids * 16];
    size_t length16 = 0;
    size_t length32 = 0;
    size_t length128 = 0;
    for (size_t index = 0; index < source.serviceUuidCount_; ++index)
    {
      EspBleUuidValue uuid;
      if (!espBleParseUuid(source.serviceUuids_[index].c_str(), uuid))
      {
        owner_->setError(EspBleError::InvalidArgument, "advertised service UUID is malformed");
        return false;
      }
      // Every list is little-endian, and the expanded value already holds the
      // short forms in that order.
      if (uuid.bitSize == 16)
      {
        memcpy(uuids16 + length16, uuid.bytes + 12, 2);
        length16 += 2;
      }
      else if (uuid.bitSize == 32)
      {
        memcpy(uuids32 + length32, uuid.bytes + 12, 4);
        length32 += 4;
      }
      else
      {
        memcpy(uuids128 + length128, uuid.bytes, 16);
        length128 += 16;
      }
    }
    if (length16 != 0 && !append(AdTypeServiceUuids16, uuids16, length16))
    {
      return fail("service UUIDs");
    }
    if (length32 != 0 && !append(AdTypeServiceUuids32, uuids32, length32))
    {
      return fail("service UUIDs");
    }
    if (length128 != 0 && !append(AdTypeServiceUuids128, uuids128, length128))
    {
      return fail("service UUIDs");
    }
  }
  if (!source.manufacturerData_.isEmpty())
  {
    if (!append(
          AdTypeManufacturerData,
          reinterpret_cast<const uint8_t *>(source.manufacturerData_.c_str()),
          source.manufacturerData_.length()))
    {
      return fail("manufacturer data");
    }
  }
  for (size_t index = 0; index < source.serviceDataCount_; ++index)
  {
    const EspBleServiceData &block = source.serviceData_[index];
    EspBleUuidValue uuid;
    if (!espBleParseUuid(block.uuid.c_str(), uuid))
    {
      owner_->setError(EspBleError::InvalidArgument, "service data UUID is malformed");
      return false;
    }
    // The UUID leads the block, little-endian, and its size picks the AD type.
    uint8_t value[16 + 24];
    size_t uuidLength = 16;
    uint8_t type = AdTypeServiceData128;
    if (uuid.bitSize == 16)
    {
      uuidLength = 2;
      type = AdTypeServiceData16;
      memcpy(value, uuid.bytes + 12, 2);
    }
    else if (uuid.bitSize == 32)
    {
      uuidLength = 4;
      type = AdTypeServiceData32;
      memcpy(value, uuid.bytes + 12, 4);
    }
    else
    {
      memcpy(value, uuid.bytes, 16);
    }
    const size_t dataLength = block.data.length();
    if (uuidLength + dataLength > sizeof(value)) return fail("service data");
    memcpy(value + uuidLength, block.data.c_str(), dataLength);
    if (!append(type, value, uuidLength + dataLength)) return fail("service data");
  }
  if (!source.name_.isEmpty())
  {
    if (!append(
          AdTypeCompleteLocalName,
          reinterpret_cast<const uint8_t *>(source.name_.c_str()),
          source.name_.length()))
    {
      return fail("name");
    }
  }
  return true;
}

bool EspBleAdvertising::setDirectedTarget(
  const char *address, EspBleAddressType addressType, bool highDuty)
{
  if (!isValidBleAddress(address))
  {
    owner_->setError(EspBleError::InvalidArgument, "peer address is malformed");
    return false;
  }
  directed_ = true;
  directedHighDuty_ = highDuty;
  directedAddress_ = address;
  directedAddressType_ = addressType;
  owner_->clearError();
  return true;
}

void EspBleAdvertising::clearDirectedTarget()
{
  directed_ = false;
  directedHighDuty_ = false;
  directedAddress_ = String();
}

bool EspBleAdvertising::setChannelMap(uint8_t channelMask)
{
  if ((channelMask & ~static_cast<uint8_t>(EspBleAdvertisingChannelAll)) != 0)
  {
    owner_->setError(EspBleError::InvalidArgument, "advertising channel mask is invalid");
    return false;
  }
  channelMask_ = channelMask;
  owner_->clearError();
  return true;
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
  // The attribute table is fixed once the GATT server runs, so start it before
  // the first advertisement rather than at begin(): services may be registered
  // up to that point.
  if (!owner_->startGattServer())
  {
    return false;
  }

  ble_gap_adv_stop();

  ble_addr_t directTarget{};
  if (directed_)
  {
    directTarget.type = static_cast<uint8_t>(directedAddressType_);
    if (!parseAddress(directedAddress_.c_str(), directTarget.val))
    {
      owner_->setError(EspBleError::InvalidArgument, "peer address is malformed");
      return false;
    }
  }
  else
  {
    // Where the device name goes. With scan response enabled and no explicit
    // scan response payload, the name is moved there so it does not consume the
    // advertising payload's 31 bytes. Any explicit scan response content takes
    // over that placement entirely.
    const bool autoNameInScanResponse =
      scanResponseEnabled_ && scanResponseData_.isEmpty() && !data_.name_.isEmpty();

    EspBleAdvertisingData primary = data_;
    if (autoNameInScanResponse)
    {
      primary.name_ = "";
    }

    Payload advertisingPayload;
    if (!buildPayload(primary, advertisingPayload, true, "advertising"))
    {
      return false;
    }
    if (ble_gap_adv_set_data(advertisingPayload.bytes, advertisingPayload.length) != 0)
    {
      owner_->setError(EspBleError::BackendFailure, "failed to set advertising data");
      return false;
    }

    Payload responsePayload;
    if (scanResponseEnabled_)
    {
      EspBleAdvertisingData responseSource = scanResponseData_;
      if (autoNameInScanResponse)
      {
        responseSource.setName(data_.name_.c_str());
      }
      if (!responseSource.isEmpty() &&
          !buildPayload(responseSource, responsePayload, false, "scan response"))
      {
        return false;
      }
    }
    // Only published when there is something to send: setting scan response
    // data at all makes a non-connectable advertisement scannable, which a
    // beacon must not be.
    if (responsePayload.length != 0 &&
        ble_gap_adv_rsp_set_data(responsePayload.bytes, responsePayload.length) != 0)
    {
      owner_->setError(EspBleError::BackendFailure, "failed to set scan response data");
      return false;
    }
  }

  ble_gap_adv_params parameters{};
  if (directed_)
  {
    parameters.conn_mode = BLE_GAP_CONN_MODE_DIR;
    parameters.high_duty_cycle = directedHighDuty_ ? 1 : 0;
  }
  else if (connectable_)
  {
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
  }
  else
  {
    parameters.conn_mode = BLE_GAP_CONN_MODE_NON;
  }
  // The discovery mode picks the PDU type for a non-connectable advertiser:
  // general is scannable (ADV_SCAN_IND, so scanners may send a scan request),
  // non-discoverable is a pure broadcast (ADV_NONCONN_IND). A beacon with no
  // scan response has nothing to answer with, so it advertises as the latter.
  const bool scannable = connectable_ || (scanResponseEnabled_ && !scanResponseData_.isEmpty());
  parameters.disc_mode = scannable ? BLE_GAP_DISC_MODE_GEN : BLE_GAP_DISC_MODE_NON;
  // Advertising interval: convert milliseconds to 0.625 ms units. A directed
  // high duty cycle advertisement has its timing fixed by the controller.
  if (intervalMinMs_ != 0 && intervalMaxMs_ != 0)
  {
    parameters.itvl_min =
      static_cast<uint16_t>((static_cast<uint32_t>(intervalMinMs_) * 8) / 5);
    parameters.itvl_max =
      static_cast<uint16_t>((static_cast<uint32_t>(intervalMaxMs_) * 8) / 5);
  }
  parameters.channel_map = channelMask_;
  switch (filterPolicy_)
  {
  case EspBleAdvertisingFilterPolicy::ScanRequestFromAcceptList:
    parameters.filter_policy = BLE_HCI_ADV_FILT_SCAN;
    break;
  case EspBleAdvertisingFilterPolicy::ConnectionFromAcceptList:
    parameters.filter_policy = BLE_HCI_ADV_FILT_CONN;
    break;
  case EspBleAdvertisingFilterPolicy::Both:
    parameters.filter_policy = BLE_HCI_ADV_FILT_BOTH;
    break;
  case EspBleAdvertisingFilterPolicy::Any:
  default:
    parameters.filter_policy = BLE_HCI_ADV_FILT_NONE;
    break;
  }

  const int32_t duration = durationSeconds == 0
    ? BLE_HS_FOREVER
    : static_cast<int32_t>(durationSeconds * 1000);
  const int backendCode = ble_gap_adv_start(
    owner_->impl_->ownAddressType,
    directed_ ? &directTarget : nullptr,
    duration,
    &parameters,
    EspBleImpl::advertisingGapEvent,
    owner_->impl_);
  if (backendCode != 0)
  {
    owner_->setError(
      EspBleError::BackendFailure,
      (String("failed to start advertising, backend code ") + backendCode).c_str());
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
  const int backendCode = ble_gap_adv_stop();
  // BLE_HS_EALREADY simply means it was not advertising.
  if (backendCode != 0 && backendCode != BLE_HS_EALREADY)
  {
    owner_->setError(EspBleError::BackendFailure, "failed to stop advertising");
    return false;
  }
  owner_->clearError();
  return true;
}

bool EspBleAdvertising::isAdvertising() const
{
  return owner_->initialized() && ble_gap_adv_active() != 0;
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

  // A scan already running would keep its own parameters, so replace it.
  if (ble_gap_disc_active()) ble_gap_disc_cancel();
  impl_->flushPending();
  impl_->activeScan = config.active;

  ble_gap_disc_params parameters{};
  // The controller counts interval and window in 0.625 ms units.
  parameters.itvl = static_cast<uint16_t>((config.intervalMilliseconds * 1000) / 625);
  parameters.window = static_cast<uint16_t>((config.windowMilliseconds * 1000) / 625);
  parameters.filter_policy = config.acceptListOnly ? BLE_HCI_SCAN_FILT_USE_WL
                                                   : BLE_HCI_SCAN_FILT_NO_WL;
  parameters.limited = 0;
  parameters.passive = config.active ? 0 : 1;
  // Controller-side duplicate filtering: the same advertiser is reported once
  // per scan rather than every interval.
  parameters.filter_duplicates = config.wantDuplicates ? 0 : 1;

  const int32_t duration = config.durationSeconds == 0
    ? BLE_HS_FOREVER
    : static_cast<int32_t>(config.durationSeconds * 1000);
  const int status = ble_gap_disc(
    owner_->impl_->ownAddressType, duration, &parameters, &EspBleScannerImpl::gapEvent, impl_);
  if (status != 0)
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
  // Cancelling a scan that is not running reports BLE_HS_EALREADY, which is the
  // requested state rather than a failure.
  const int status = ble_gap_disc_cancel();
  if (status != 0 && status != BLE_HS_EALREADY)
  {
    owner_->setError(EspBleError::BackendFailure, "failed to stop scan");
    return false;
  }
  if (impl_ != nullptr) impl_->flushPending();
  owner_->clearError();
  return true;
}

bool EspBleScanner::isScanning() const
{
  return owner_->initialized() && ble_gap_disc_active() != 0;
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

EspBleGattService EspBleGattServer::addService(const char *serviceUuid)
{
  if (owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "GATT services must be configured before begin");
    return EspBleGattService();
  }
  if (serviceUuid == nullptr || serviceUuid[0] == '\0')
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT service UUID is empty");
    return EspBleGattService();
  }
  if (impl_ == nullptr)
  {
    impl_ = new EspBleGattServerImpl(this);
    if (impl_ == nullptr)
    {
      owner_->setError(EspBleError::ResourceExhausted, "failed to allocate GATT Server state");
      return EspBleGattService();
    }
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->serviceCount == MaxServices)
  {
    owner_->setError(EspBleError::ResourceExhausted, "too many GATT services");
    return EspBleGattService();
  }

  // Repeating a UUID creates a second instance rather than returning the first:
  // the spec allows a device to expose the same service more than once.
  const size_t index = impl_->serviceCount++;
  impl_->services[index].uuid = serviceUuid;
  owner_->clearError();
  EspBleGattService handle;
  handle.id = static_cast<uint16_t>(index + 1);
  return handle;
}

EspBleGattCharacteristic EspBleGattServer::addCharacteristic(
  EspBleGattService service,
  const char *characteristicUuid,
  const EspBleGattCharacteristicConfig &config)
{
  if (owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "GATT characteristics must be configured before begin");
    return EspBleGattCharacteristic();
  }
  if (characteristicUuid == nullptr || characteristicUuid[0] == '\0')
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT characteristic UUID is empty");
    return EspBleGattCharacteristic();
  }
  if (!config.readable && !config.writable && !config.writableWithoutResponse &&
      !config.notifiable && !config.indicatable)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT characteristic has no properties");
    return EspBleGattCharacteristic();
  }
  if (((config.encryptedRead || config.authenticatedRead) && !config.readable) ||
      ((config.encryptedWrite || config.authenticatedWrite) &&
       !config.writable && !config.writableWithoutResponse))
  {
    owner_->setError(
      EspBleError::InvalidArgument,
      "secured GATT access requires the corresponding read or write property");
    return EspBleGattCharacteristic();
  }
  if (impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT service handle is invalid");
    return EspBleGattCharacteristic();
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!service.valid() || service.id > impl_->serviceCount)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT service handle is invalid");
    return EspBleGattCharacteristic();
  }
  if (impl_->characteristicCount == MaxCharacteristics)
  {
    owner_->setError(EspBleError::ResourceExhausted, "too many GATT characteristics");
    return EspBleGattCharacteristic();
  }

  // One service may expose several characteristics with the same UUID, as the
  // spec allows (HID Reports are the everyday case): the attribute table is
  // built here, and every operation names its target by handle.
  const size_t serviceIndex = static_cast<size_t>(service.id - 1);
  const size_t index = impl_->characteristicCount++;
  auto &definition = impl_->characteristics[index];
  definition.serviceIndex = serviceIndex;
  definition.serviceUuid = impl_->services[serviceIndex].uuid;
  definition.uuid = characteristicUuid;
  definition.config = config;
  owner_->clearError();
  EspBleGattCharacteristic handle;
  handle.id = static_cast<uint16_t>(index + 1);
  return handle;
}

EspBleGattDescriptor EspBleGattServer::addDescriptor(
  EspBleGattCharacteristic characteristic,
  const char *descriptorUuid,
  const EspBleGattDescriptorConfig &config)
{
  if (owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "GATT descriptors must be configured before begin");
    return EspBleGattDescriptor();
  }
  if (descriptorUuid == nullptr || descriptorUuid[0] == '\0' || config.maximumLength == 0)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid GATT descriptor arguments");
    return EspBleGattDescriptor();
  }
  if (!config.readable && !config.writable)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT descriptor has no access permissions");
    return EspBleGattDescriptor();
  }
  // The Client Characteristic Configuration Descriptor is owned by the stack: it
  // is added automatically for a notifiable or indicatable characteristic, and it
  // tracks each peer's subscription. A second, application-managed copy would
  // shadow it, so set config.notifiable / config.indicatable instead.
  if (uuidEquals(String(EspBle::ClientCharacteristicConfigurationUuid), descriptorUuid) ||
      uuidEquals(String("2902"), descriptorUuid))
  {
    owner_->setError(
      EspBleError::InvalidArgument,
      "the Client Characteristic Configuration Descriptor is managed by the stack; "
      "use notifiable / indicatable instead");
    return EspBleGattDescriptor();
  }
  if ((config.encryptedRead || config.authenticatedRead) && !config.readable)
  {
    owner_->setError(EspBleError::InvalidArgument, "secured descriptor read requires readable access");
    return EspBleGattDescriptor();
  }
  if ((config.encryptedWrite || config.authenticatedWrite) && !config.writable)
  {
    owner_->setError(EspBleError::InvalidArgument, "secured descriptor write requires writable access");
    return EspBleGattDescriptor();
  }
  if (impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT characteristic handle is invalid");
    return EspBleGattDescriptor();
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!characteristic.valid() || characteristic.id > impl_->characteristicCount)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT characteristic handle is invalid");
    return EspBleGattDescriptor();
  }
  const size_t characteristicIndex = static_cast<size_t>(characteristic.id - 1);
  for (size_t index = 0; index < impl_->descriptorCount; ++index)
  {
    const auto &definition = impl_->descriptors[index];
    // One characteristic may not carry the same descriptor UUID twice: unlike
    // services and characteristics, a descriptor is looked up by UUID within
    // its characteristic and a duplicate would be unreachable.
    if (definition.characteristicIndex == characteristicIndex &&
        uuidEquals(definition.uuid, descriptorUuid))
    {
      owner_->setError(EspBleError::InvalidArgument, "GATT descriptor already exists");
      return EspBleGattDescriptor();
    }
  }
  if (impl_->descriptorCount == MaxDescriptors)
  {
    owner_->setError(EspBleError::ResourceExhausted, "too many GATT descriptors");
    return EspBleGattDescriptor();
  }

  const size_t index = impl_->descriptorCount++;
  auto &definition = impl_->descriptors[index];
  definition.characteristicIndex = characteristicIndex;
  definition.serviceUuid = impl_->characteristics[characteristicIndex].serviceUuid;
  definition.characteristicUuid = impl_->characteristics[characteristicIndex].uuid;
  definition.uuid = descriptorUuid;
  definition.config = config;
  owner_->clearError();
  EspBleGattDescriptor handle;
  handle.id = static_cast<uint16_t>(index + 1);
  return handle;
}

bool EspBleGattServer::setValue(
  EspBleGattCharacteristic characteristic, const uint8_t *data, size_t length)
{
  if (data == nullptr && length != 0)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid GATT value arguments");
    return false;
  }
  if (impl_ == nullptr || !characteristic.valid())
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT characteristic handle is invalid");
    return false;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (characteristic.id > impl_->characteristicCount)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT characteristic handle is invalid");
    return false;
  }
  auto &definition = impl_->characteristics[characteristic.id - 1];
  definition.value =
    length == 0 ? String() : String(reinterpret_cast<const char *>(data), length);
  owner_->clearError();
  return true;
}

bool EspBleGattServer::setValue(EspBleGattCharacteristic characteristic, const String &value)
{
  return setValue(
    characteristic, reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
}

bool EspBleGattServer::value(EspBleGattCharacteristic characteristic, String &value) const
{
  if (impl_ == nullptr || !characteristic.valid())
  {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (characteristic.id > impl_->characteristicCount)
  {
    return false;
  }
  value = impl_->characteristics[characteristic.id - 1].value;
  return true;
}

bool EspBleGattServer::setDescriptorValue(
  EspBleGattDescriptor descriptor, const uint8_t *data, size_t length)
{
  if (data == nullptr && length != 0)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid GATT descriptor value arguments");
    return false;
  }
  if (impl_ == nullptr || !descriptor.valid())
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT descriptor handle is invalid");
    return false;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (descriptor.id > impl_->descriptorCount)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT descriptor handle is invalid");
    return false;
  }
  auto &definition = impl_->descriptors[descriptor.id - 1];
  if (length > definition.config.maximumLength)
  {
    owner_->setError(EspBleError::InvalidArgument, "GATT descriptor value exceeds maximumLength");
    return false;
  }
  definition.value =
    length == 0 ? String() : String(reinterpret_cast<const char *>(data), length);
  owner_->clearError();
  return true;
}

bool EspBleGattServer::setDescriptorValue(EspBleGattDescriptor descriptor, const String &value)
{
  return setDescriptorValue(
    descriptor, reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
}

bool EspBleGattServer::descriptorValue(EspBleGattDescriptor descriptor, String &value) const
{
  if (impl_ == nullptr || !descriptor.valid())
  {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (descriptor.id > impl_->descriptorCount)
  {
    return false;
  }
  value = impl_->descriptors[descriptor.id - 1].value;
  return true;
}

bool EspBleGattServer::notify(
  EspBleGattCharacteristic characteristic, const uint8_t *data, size_t length)
{
  return send(0, characteristic, data, length, false);
}

bool EspBleGattServer::notify(EspBleGattCharacteristic characteristic, const String &value)
{
  return send(
    0, characteristic, reinterpret_cast<const uint8_t *>(value.c_str()), value.length(), false);
}

bool EspBleGattServer::indicate(
  EspBleGattCharacteristic characteristic, const uint8_t *data, size_t length)
{
  return send(0, characteristic, data, length, true);
}

bool EspBleGattServer::indicate(EspBleGattCharacteristic characteristic, const String &value)
{
  return send(
    0, characteristic, reinterpret_cast<const uint8_t *>(value.c_str()), value.length(), true);
}

bool EspBleGattServer::notify(
  EspBleConnectionId connectionId,
  EspBleGattCharacteristic characteristic,
  const uint8_t *data,
  size_t length)
{
  return send(connectionId, characteristic, data, length, false);
}

bool EspBleGattServer::notify(
  EspBleConnectionId connectionId, EspBleGattCharacteristic characteristic, const String &value)
{
  return send(
    connectionId,
    characteristic,
    reinterpret_cast<const uint8_t *>(value.c_str()),
    value.length(),
    false);
}

bool EspBleGattServer::indicate(
  EspBleConnectionId connectionId,
  EspBleGattCharacteristic characteristic,
  const uint8_t *data,
  size_t length)
{
  return send(connectionId, characteristic, data, length, true);
}

bool EspBleGattServer::indicate(
  EspBleConnectionId connectionId, EspBleGattCharacteristic characteristic, const String &value)
{
  return send(
    connectionId,
    characteristic,
    reinterpret_cast<const uint8_t *>(value.c_str()),
    value.length(),
    true);
}

bool EspBleGattServer::send(
  EspBleConnectionId connectionId,
  EspBleGattCharacteristic characteristic,
  const uint8_t *data,
  size_t length,
  bool indication)
{
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (!characteristic.valid() || (data == nullptr && length != 0) || impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid GATT send arguments");
    return false;
  }

  // MTU guard. A connection-scoped send is bounded by its own connection's
  // payload; a broadcast is bounded by the smallest subscriber payload so the
  // backend never silently truncates for anyone.
  if (connectionId != 0)
  {
    bool found = false;
    size_t maximumPayload = 0;
    {
      std::lock_guard<std::mutex> lock(owner_->impl_->mutex);
      for (const EspBleImpl::ConnectionSlot &slot : owner_->impl_->connections)
      {
        if (slot.used && slot.connection.localRole == EspBleRole::Peripheral &&
            slot.connection.id == connectionId)
        {
          found = true;
          maximumPayload = slot.connection.maximumNotificationPayload();
          break;
        }
      }
    }
    if (!found)
    {
      owner_->setError(EspBleError::NotFound, "target connection is not a connected peripheral");
      return false;
    }
    if (length > maximumPayload)
    {
      owner_->setError(EspBleError::InvalidArgument, "GATT send value exceeds negotiated MTU payload");
      return false;
    }
  }
  else
  {
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
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    if (characteristic.id > impl_->characteristicCount)
    {
      owner_->setError(EspBleError::InvalidArgument, "GATT characteristic handle is invalid");
      return false;
    }
    EspBleGattServerImpl::CharacteristicDefinition *found =
      &impl_->characteristics[characteristic.id - 1];
    if (found->def == nullptr)
    {
      owner_->setError(EspBleError::NotFound, "GATT characteristic was not registered");
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
    if (impl_->sendQueueCount >= EspBleGattServerImpl::SendQueueCapacity)
    {
      owner_->setError(EspBleError::ResourceExhausted, "GATT Server send queue is full");
      return false;
    }

    // Enqueue instead of rejecting when a send is already in flight; the value
    // is captured per request so interleaved sends keep their own payloads.
    found->value = length == 0
      ? String()
      : String(reinterpret_cast<const char *>(data), length);
    const size_t tail =
      (impl_->sendQueueHead + impl_->sendQueueCount) % EspBleGattServerImpl::SendQueueCapacity;
    EspBleGattServerImpl::SendRequest &request = impl_->sendQueue[tail];
    request.connectionId = connectionId;
    request.valueHandle = found->valueHandle;
    request.characteristic = characteristic;
    request.serviceUuid = found->serviceUuid;
    request.characteristicUuid = found->uuid;
    request.value = found->value;
    request.indication = indication;
    ++impl_->sendQueueCount;
  }
  owner_->clearError();
  return true;
}

void EspBle::pumpSendQueue()
{
  if (gattServer_.impl_ == nullptr) return;
  EspBleGattServerImpl *impl = gattServer_.impl_;

  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    // One send in flight at a time (single backend send path); the next request
    // starts once the running task clears `sending` (observed a later update()).
    if (impl->sending || impl->sendQueueCount == 0) return;
    EspBleGattServerImpl::SendRequest &request = impl->sendQueue[impl->sendQueueHead];
    impl->sendConnectionId = request.connectionId;
    impl->sendValueHandle = request.valueHandle;
    impl->sendCharacteristic = request.characteristic;
    impl->sendServiceUuid = request.serviceUuid;
    impl->sendCharacteristicUuid = request.characteristicUuid;
    impl->sendValue = request.value;
    impl->sendIndication = request.indication;
    impl->sendQueueHead = (impl->sendQueueHead + 1) % EspBleGattServerImpl::SendQueueCapacity;
    --impl->sendQueueCount;
    impl->sending = true;
  }

  TaskHandle_t task = nullptr;
  const BaseType_t created = xTaskCreate(
    EspBleGattServerImpl::sendTaskEntry,
    "espble-gatt-send",
    4096,
    impl,
    1,
    &task);
  if (created != pdPASS)
  {
    // Report the failure so the caller's onSent still fires, then clear busy so
    // the queue keeps draining on the next update().
    EspBleGattSendResult failure;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      failure.connectionId = impl->sendConnectionId;
      failure.characteristic = impl->sendCharacteristic;
      failure.serviceUuid = impl->sendServiceUuid;
      failure.characteristicUuid = impl->sendCharacteristicUuid;
      failure.value = impl->sendValue;
      failure.indication = impl->sendIndication;
      failure.success = false;
      failure.error = EspBleError::ResourceExhausted;
      failure.detail = "failed to create GATT Server send task";
      impl->sending = false;
      impl->sendTask = nullptr;
    }
    if (impl_ != nullptr)
    {
      impl_->queueServerSendResult(failure);
    }
    return;
  }
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (impl->sending)
    {
      impl->sendTask = task;
    }
  }
}

void EspBleGattServer::onWritten(WriteCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  writtenListeners_.setPrimary(std::move(callback));
}

void EspBleGattServer::onRead(ReadCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  readCallback_ = std::move(callback);
}

void EspBleGattServer::onDescriptorWritten(DescriptorWriteCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  descriptorWrittenListeners_.setPrimary(std::move(callback));
}

void EspBleGattServer::onSubscriptionChanged(SubscriptionCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  subscriptionListeners_.setPrimary(std::move(callback));
}

void EspBleGattServer::onSent(SendCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  sentListeners_.setPrimary(std::move(callback));
}

EspBleListenerId EspBleGattServer::allocateListenerIdLocked()
{
  const EspBleListenerId id = nextListenerId_;
  nextListenerId_ = (id == 0xffffffffu) ? 1 : id + 1;
  return id;
}

EspBleListenerId EspBleGattServer::addWrittenListener(WriteCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  const EspBleListenerId id = writtenListeners_.add(std::move(callback), allocateListenerIdLocked());
  if (id == EspBleInvalidListenerId)
    owner_->setError(EspBleError::ResourceExhausted, "too many GATT Server listeners");
  else
    owner_->clearError();
  return id;
}

EspBleListenerId EspBleGattServer::addDescriptorWrittenListener(DescriptorWriteCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  const EspBleListenerId id = descriptorWrittenListeners_.add(std::move(callback), allocateListenerIdLocked());
  if (id == EspBleInvalidListenerId)
    owner_->setError(EspBleError::ResourceExhausted, "too many GATT Server listeners");
  else
    owner_->clearError();
  return id;
}

EspBleListenerId EspBleGattServer::addSubscriptionChangedListener(SubscriptionCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  const EspBleListenerId id = subscriptionListeners_.add(std::move(callback), allocateListenerIdLocked());
  if (id == EspBleInvalidListenerId)
    owner_->setError(EspBleError::ResourceExhausted, "too many GATT Server listeners");
  else
    owner_->clearError();
  return id;
}

EspBleListenerId EspBleGattServer::addSentListener(SendCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  const EspBleListenerId id = sentListeners_.add(std::move(callback), allocateListenerIdLocked());
  if (id == EspBleInvalidListenerId)
    owner_->setError(EspBleError::ResourceExhausted, "too many GATT Server listeners");
  else
    owner_->clearError();
  return id;
}

bool EspBleGattServer::removeListener(EspBleListenerId listenerId)
{
  if (listenerId == EspBleInvalidListenerId)
  {
    owner_->setError(EspBleError::InvalidArgument, "listener ID is invalid");
    return false;
  }
  std::lock_guard<std::mutex> lock(listenerMutex_);
  const bool removed =
    writtenListeners_.remove(listenerId) ||
    descriptorWrittenListeners_.remove(listenerId) ||
    subscriptionListeners_.remove(listenerId) ||
    sentListeners_.remove(listenerId);
  if (!removed)
  {
    owner_->setError(EspBleError::NotFound, "listener ID was not found");
    return false;
  }
  owner_->clearError();
  return true;
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

  // Build the NimBLE attribute table directly. Two services may share a UUID and
  // so may two characteristics, and only pointer identity distinguishes them, so
  // every definition gets its own table entry that the access callback matches on.
  size_t characteristicSlot = 0;
  size_t descriptorSlot = 0;
  for (size_t serviceIndex = 0; serviceIndex < impl_->serviceCount; ++serviceIndex)
  {
    auto &serviceDefinition = impl_->services[serviceIndex];
    if (!parseUuid(serviceDefinition.uuid.c_str(), serviceDefinition.nativeUuid))
    {
      owner_->setError(EspBleError::InvalidArgument, "GATT service UUID is malformed");
      return false;
    }
    ble_gatt_svc_def &service = impl_->serviceDefs[serviceIndex];
    service.type = BLE_GATT_SVC_TYPE_PRIMARY;
    service.uuid = &serviceDefinition.nativeUuid.u;
    service.includes = nullptr;
    serviceDefinition.def = &service;

    const size_t firstCharacteristic = characteristicSlot;
    size_t serviceCharacteristics = 0;
    for (size_t characteristicIndex = 0;
         characteristicIndex < impl_->characteristicCount;
         ++characteristicIndex)
    {
      auto &characteristicDefinition = impl_->characteristics[characteristicIndex];
      // Match by index, not UUID: two services may carry the same UUID.
      if (characteristicDefinition.serviceIndex != serviceIndex) continue;
      if (!parseUuid(characteristicDefinition.uuid.c_str(), characteristicDefinition.nativeUuid))
      {
        owner_->setError(EspBleError::InvalidArgument, "GATT characteristic UUID is malformed");
        return false;
      }

      ble_gatt_chr_def &characteristic = impl_->characteristicDefs[characteristicSlot++];
      ++serviceCharacteristics;
      const auto &config = characteristicDefinition.config;
      ble_gatt_chr_flags flags = 0;
      if (config.readable) flags |= BLE_GATT_CHR_F_READ;
      if (config.writable) flags |= BLE_GATT_CHR_F_WRITE;
      if (config.writableWithoutResponse) flags |= BLE_GATT_CHR_F_WRITE_NO_RSP;
      // Notify and Indicate make the host add and manage the CCCD itself.
      if (config.notifiable) flags |= BLE_GATT_CHR_F_NOTIFY;
      if (config.indicatable) flags |= BLE_GATT_CHR_F_INDICATE;
      if (config.encryptedRead) flags |= BLE_GATT_CHR_F_READ_ENC;
      if (config.encryptedWrite) flags |= BLE_GATT_CHR_F_WRITE_ENC;
      if (config.authenticatedRead) flags |= BLE_GATT_CHR_F_READ_AUTHEN;
      if (config.authenticatedWrite) flags |= BLE_GATT_CHR_F_WRITE_AUTHEN;
      characteristic.uuid = &characteristicDefinition.nativeUuid.u;
      characteristic.access_cb = EspBleGattServerImpl::accessCallback;
      characteristic.arg = impl_;
      characteristic.flags = flags;
      characteristic.min_key_size = 0;
      characteristic.val_handle = &characteristicDefinition.valueHandle;
      characteristicDefinition.valueHandle = 0;
      characteristicDefinition.def = &characteristic;

      const size_t firstDescriptor = descriptorSlot;
      size_t characteristicDescriptors = 0;
      for (size_t descriptorIndex = 0;
           descriptorIndex < impl_->descriptorCount;
           ++descriptorIndex)
      {
        auto &descriptorDefinition = impl_->descriptors[descriptorIndex];
        if (descriptorDefinition.characteristicIndex != characteristicIndex) continue;
        if (!parseUuid(descriptorDefinition.uuid.c_str(), descriptorDefinition.nativeUuid))
        {
          owner_->setError(EspBleError::InvalidArgument, "GATT descriptor UUID is malformed");
          return false;
        }
        ble_gatt_dsc_def &descriptor = impl_->descriptorDefs[descriptorSlot++];
        ++characteristicDescriptors;
        const EspBleGattDescriptorConfig &descriptorConfig = descriptorDefinition.config;
        uint8_t attributeFlags = 0;
        if (descriptorConfig.readable) attributeFlags |= BLE_ATT_F_READ;
        if (descriptorConfig.writable) attributeFlags |= BLE_ATT_F_WRITE;
        if (descriptorConfig.encryptedRead) attributeFlags |= BLE_ATT_F_READ_ENC;
        if (descriptorConfig.encryptedWrite) attributeFlags |= BLE_ATT_F_WRITE_ENC;
        if (descriptorConfig.authenticatedRead) attributeFlags |= BLE_ATT_F_READ_AUTHEN;
        if (descriptorConfig.authenticatedWrite) attributeFlags |= BLE_ATT_F_WRITE_AUTHEN;
        descriptor.uuid = &descriptorDefinition.nativeUuid.u;
        descriptor.att_flags = attributeFlags;
        descriptor.min_key_size = 0;
        descriptor.access_cb = EspBleGattServerImpl::accessCallback;
        descriptor.arg = impl_;
        descriptorDefinition.def = &descriptor;
      }
      if (characteristicDescriptors != 0)
      {
        // Terminator for this characteristic's descriptor run.
        impl_->descriptorDefs[descriptorSlot++] = ble_gatt_dsc_def{};
        characteristic.descriptors = &impl_->descriptorDefs[firstDescriptor];
      }
      else
      {
        characteristic.descriptors = nullptr;
      }
    }
    if (serviceCharacteristics != 0)
    {
      impl_->characteristicDefs[characteristicSlot++] = ble_gatt_chr_def{};
      service.characteristics = &impl_->characteristicDefs[firstCharacteristic];
    }
    else
    {
      service.characteristics = nullptr;
    }
  }
  impl_->serviceDefs[impl_->serviceCount] = ble_gatt_svc_def{};

  int backendCode = ble_gatts_count_cfg(impl_->serviceDefs);
  if (backendCode == 0)
  {
    backendCode = ble_gatts_add_svcs(impl_->serviceDefs);
  }
  if (backendCode != 0)
  {
    owner_->setError(
      EspBleError::BackendFailure,
      (String("failed to register GATT services, backend code ") + backendCode).c_str());
    return false;
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
    impl_->services[index].def = nullptr;
  }
  for (size_t index = 0; index < impl_->characteristicCount; ++index)
  {
    impl_->characteristics[index].def = nullptr;
    impl_->characteristics[index].valueHandle = 0;
  }
  for (size_t index = 0; index < impl_->descriptorCount; ++index)
  {
    impl_->descriptors[index].def = nullptr;
  }
  for (EspBleGattServerImpl::SubscriptionSlot &slot : impl_->subscriptions)
  {
    slot = EspBleGattServerImpl::SubscriptionSlot();
  }
  impl_->realized = false;
}

void EspBleGattServer::dispatchWrite(const EspBleGattWrite &write)
{
  std::shared_ptr<WriteCallback> callbacks[decltype(writtenListeners_)::Capacity];
  size_t count = 0;
  {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    count = writtenListeners_.snapshot(callbacks);
  }
  for (size_t i = 0; i < count; ++i) (*callbacks[i])(write);
}

void EspBleGattServer::dispatchRead(const EspBleGattReadRequest &request)
{
  ReadCallback callback;
  {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    callback = readCallback_;
  }
  if (callback) callback(request);
}

void EspBleGattServer::dispatchDescriptorWrite(const EspBleGattDescriptorWrite &write)
{
  std::shared_ptr<DescriptorWriteCallback> callbacks[decltype(descriptorWrittenListeners_)::Capacity];
  size_t count = 0;
  {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    count = descriptorWrittenListeners_.snapshot(callbacks);
  }
  for (size_t i = 0; i < count; ++i) (*callbacks[i])(write);
}

void EspBleGattServer::dispatchSubscription(const EspBleGattSubscription &subscription)
{
  std::shared_ptr<SubscriptionCallback> callbacks[decltype(subscriptionListeners_)::Capacity];
  size_t count = 0;
  {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    count = subscriptionListeners_.snapshot(callbacks);
  }
  for (size_t i = 0; i < count; ++i) (*callbacks[i])(subscription);
}

void EspBleGattServer::dispatchSendResult(const EspBleGattSendResult &result)
{
  std::shared_ptr<SendCallback> callbacks[decltype(sentListeners_)::Capacity];
  size_t count = 0;
  {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    count = sentListeners_.snapshot(callbacks);
  }
  for (size_t i = 0; i < count; ++i) (*callbacks[i])(result);
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
  impl_->keyboardNkro = nkroEnabled_;
  impl_->inputLengths[ESP_BLE_HID_REPORT_ID_KEYBOARD - 1] = nkroEnabled_ ? 29 : 8;
  impl_->bootProtocolEnabled = config.bootProtocol;
  return true;
}

void EspBleHidKeyboard::enableNkro(bool enable)
{
  if (owner_->initialized() || (impl_ != nullptr && impl_->configured))
  {
    owner_->setError(EspBleError::InvalidState, "NKRO mode must be selected before configure");
    return;
  }
  nkroEnabled_ = enable;
  nkroState_.clear();
  owner_->clearError();
}

bool EspBleHidKeyboard::nkroEnabled() const
{
  return nkroEnabled_;
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

bool EspBleHidKeyboard::configureCustom(const EspBleHidDeviceConfig &config)
{
  if (owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "HID Device must be configured before begin");
    return false;
  }
  if (config.initialBatteryLevel > 100)
  {
    owner_->setError(EspBleError::InvalidArgument, "HID battery level must be at most 100");
    return false;
  }
  if (impl_ == nullptr)
  {
    impl_ = new EspBleHidDeviceManagerImpl(this);
    if (impl_ == nullptr)
    {
      owner_->setError(EspBleError::ResourceExhausted, "failed to allocate HID Device state");
      return false;
    }
  }
  const bool firstConfiguration = !impl_->configured;
  if (firstConfiguration)
  {
    impl_->config = config;
  }
  impl_->customConfigured = true;
  impl_->configured = true;
  if (firstConfiguration && !owner_->advertising().addServiceUuid("1812"))
  {
    impl_->configured = false;
    impl_->customConfigured = false;
    return false;
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
  static const uint8_t nkroKeyboardMap[] = {
    0x05,0x01, 0x09,0x06, 0xa1,0x01, 0x85,0x01,
    0x05,0x07, 0x19,0xe0, 0x29,0xe7, 0x15,0x00, 0x25,0x01,
    0x75,0x01, 0x95,0x08, 0x81,0x02,
    0x05,0x08, 0x19,0x01, 0x29,0x05, 0x95,0x05, 0x75,0x01,
    0x91,0x02, 0x95,0x01, 0x75,0x03, 0x91,0x01,
    0x05,0x07, 0x19,0x00, 0x29,0xdf, 0x15,0x00, 0x25,0x01,
    0x75,0x01, 0x95,0xe0, 0x81,0x02, 0xc0
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
  const MapPart keyboardPart = impl_->keyboardNkro
    ? MapPart{nkroKeyboardMap, sizeof(nkroKeyboardMap)}
    : MapPart{keyboardMap, sizeof(keyboardMap)};
  const MapPart maps[] = {keyboardPart, {mouseMap,sizeof(mouseMap)},
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
  // Append the user's custom Report Descriptor after the built-in profile maps.
  if (impl_->customReportMapLength > 0 &&
      impl_->reportMapLength + impl_->customReportMapLength <= sizeof(impl_->reportMap))
  {
    memcpy(impl_->reportMap + impl_->reportMapLength,
           impl_->customReportMap, impl_->customReportMapLength);
    impl_->reportMapLength += impl_->customReportMapLength;
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
  // HID over GATT Boot Protocol support for the keyboard profile: a Protocol
  // Mode characteristic plus dedicated 8-byte Boot Keyboard Input / Output
  // reports the Host uses while in Boot Protocol Mode (e.g. a BIOS). Opt-in, so
  // ordinary keyboards do not enlarge every host's discovery.
  if ((impl_->profileMask & 0x01) != 0 && impl_->bootProtocolEnabled)
  {
    ble_gatt_chr_def &protocolModeChr = impl_->hidCharacteristics[characteristicIndex++];
    protocolModeChr.uuid = &impl_->protocolModeUuid.u;
    protocolModeChr.access_cb = EspBleHidDeviceManagerImpl::accessCallback;
    protocolModeChr.arg = impl_;
    protocolModeChr.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP |
      encryptedRead | encryptedWrite;
    impl_->protocolModeCharacteristic = &protocolModeChr;

    ble_gatt_chr_def &bootInput = impl_->hidCharacteristics[characteristicIndex++];
    bootInput.uuid = &impl_->bootKeyboardInputUuid.u;
    bootInput.access_cb = EspBleHidDeviceManagerImpl::accessCallback;
    bootInput.arg = impl_;
    bootInput.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | encryptedRead;
    bootInput.val_handle = &impl_->bootKeyboardInputValueHandle;
    impl_->bootKeyboardInputCharacteristic = &bootInput;

    ble_gatt_chr_def &bootOutput = impl_->hidCharacteristics[characteristicIndex++];
    bootOutput.uuid = &impl_->bootKeyboardOutputUuid.u;
    bootOutput.access_cb = EspBleHidDeviceManagerImpl::accessCallback;
    bootOutput.arg = impl_;
    bootOutput.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
      BLE_GATT_CHR_F_WRITE_NO_RSP | encryptedRead | encryptedWrite;
    impl_->bootKeyboardOutputCharacteristic = &bootOutput;
  }
  // Custom (user-declared) reports composed into the same HID service. Each is a
  // Report characteristic (0x2A4D) with a Report Reference descriptor carrying
  // its report ID and type; inputs are notifiable, outputs/features writable.
  for (size_t index = 0; index < impl_->customReportCount; ++index)
  {
    EspBleHidDeviceManagerImpl::CustomReport &report = impl_->customReports[index];
    report.descriptors[0].uuid = &impl_->reportReferenceUuid.u;
    report.descriptors[0].att_flags = descriptorFlags;
    report.descriptors[0].access_cb = EspBleHidDeviceManagerImpl::accessCallback;
    report.descriptors[0].arg = impl_;
    ble_gatt_chr_def &characteristic = impl_->hidCharacteristics[characteristicIndex++];
    characteristic.uuid = &impl_->reportUuid.u;
    characteristic.access_cb = EspBleHidDeviceManagerImpl::accessCallback;
    characteristic.arg = impl_;
    characteristic.descriptors = report.descriptors;
    if (report.reportType == ESP_BLE_HID_REPORT_TYPE_INPUT)
    {
      characteristic.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | encryptedRead;
      characteristic.val_handle = &report.valueHandle;
    }
    else if (report.reportType == ESP_BLE_HID_REPORT_TYPE_OUTPUT)
    {
      characteristic.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
        BLE_GATT_CHR_F_WRITE_NO_RSP | encryptedRead | encryptedWrite;
    }
    else // feature
    {
      characteristic.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
        encryptedRead | encryptedWrite;
    }
    report.characteristic = &characteristic;
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
  if (!owner_->startGattServer())
  {
    return false;
  }
  bool handlesRegistered = true;
  for (uint8_t index = 0; index < EspBleHidDeviceManagerImpl::ProfileCount; ++index)
  {
    if ((impl_->profileMask & static_cast<uint8_t>(1u << index)) != 0 &&
        impl_->inputValueHandles[index] == 0) handlesRegistered = false;
  }
  if ((impl_->profileMask & 0x01) != 0 && impl_->outputValueHandle == 0)
    handlesRegistered = false;
  if ((impl_->profileMask & 0x01) != 0 && impl_->bootProtocolEnabled &&
      impl_->bootKeyboardInputValueHandle == 0)
    handlesRegistered = false;
  for (size_t index = 0; index < impl_->customReportCount; ++index)
  {
    if (impl_->customReports[index].reportType == ESP_BLE_HID_REPORT_TYPE_INPUT &&
        impl_->customReports[index].valueHandle == 0)
      handlesRegistered = false;
  }
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

bool EspBleHidKeyboard::sendHeldNkroState()
{
  uint8_t value[1 + EspBleHidKeyboardNkroReport::BitmapSize] = {nkroState_.modifiers};
  memcpy(value + 1, nkroState_.bitmap, sizeof(nkroState_.bitmap));
  return sendRawReport(ESP_BLE_HID_REPORT_ID_KEYBOARD, value, sizeof(value));
}

const EspBleHidKeyboardNkroReport &EspBleHidKeyboard::heldState() const
{
  return nkroState_;
}

EspBleHidKeyboardOutputReport EspBleHidKeyboard::ledState() const
{
  if (impl_ == nullptr) return EspBleHidKeyboardOutputReport();
  // By value, not by reference: unlike nkroState_ (written only from the send
  // paths on the caller's task), this is written from the stack task when the
  // host writes the report, so it is copied out under the lock.
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->ledState;
}

bool EspBleHidKeyboard::sendReport(const EspBleHidKeyboardReport &report)
{
  if (nkroEnabled_)
  {
    nkroState_.clear();
    nkroState_.modifiers = report.modifiers;
    // A 6KRO report carries modifiers in its own byte, so key slots holding a
    // modifier usage are dropped rather than routed, as they always were.
    for (uint8_t usage : report.keys)
      if (usage != 0 && usage <= EspBleHidKeyboardNkroReport::MaxBitmapUsage)
        nkroState_.press(usage);
    return sendHeldNkroState();
  }
  uint8_t value[8] = {report.modifiers, 0};
  memcpy(value + 2, report.keys, sizeof(report.keys));
  return sendRawReport(ESP_BLE_HID_REPORT_ID_KEYBOARD, value, sizeof(value));
}

bool EspBleHidKeyboard::sendReport(const EspBleHidKeyboardNkroReport &report)
{
  if (!nkroEnabled_)
  {
    owner_->setError(
      EspBleError::InvalidState,
      "NKRO reports need enableNkro() before configure()");
    return false;
  }
  // Replace the incremental state so a later pressUsage() / releaseUsage() sees
  // what the Host was actually told.
  nkroState_ = report;
  return sendHeldNkroState();
}

bool EspBleHidKeyboard::useBootKeyboard(uint8_t reportId) const
{
  // In Boot Protocol Mode the keyboard report travels over the dedicated 8-byte
  // Boot Keyboard Input Report instead of the Report-protocol characteristic.
  if (reportId != ESP_BLE_HID_REPORT_ID_KEYBOARD || impl_ == nullptr ||
      impl_->bootKeyboardInputValueHandle == 0)
  {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->protocolMode == EspBleHidKeyboard::BootProtocolMode;
}

size_t EspBleHidKeyboard::eligibleConnections(
  uint8_t reportId,
  int customSlot,
  bool useBoot,
  uint16_t *handles,
  size_t capacity,
  bool &anyPeripheral) const
{
  anyPeripheral = false;
  size_t connectionCount = 0;
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
        if (connectionCount >= capacity) break;
        handles[connectionCount++] = slot.connection.handle;
      }
    }
  }
  // Only notify peers that subscribed to the Input Report CCCD.
  size_t eligibleCount = 0;
  for (size_t index = 0; index < connectionCount; ++index)
  {
    bool eligible = false;
    if (customSlot >= 0)
    {
      eligible = impl_->subscribedCustom(handles[index], static_cast<size_t>(customSlot));
    }
    else
    {
      eligible = useBoot
        ? impl_->subscribedBootKeyboard(handles[index])
        : impl_->subscribed(handles[index], reportId);
    }
    if (eligible)
    {
      handles[eligibleCount++] = handles[index];
    }
  }
  return eligibleCount;
}

bool EspBleHidKeyboard::readyFor(uint8_t reportId, int customSlot) const
{
  if (!owner_->initialized() || impl_ == nullptr || !impl_->realized) return false;
  if (customSlot >= 0)
  {
    if (static_cast<size_t>(customSlot) >= impl_->customReportCount) return false;
    if (impl_->customReports[customSlot].valueHandle == 0) return false;
  }
  else if (reportId < 1 || reportId > EspBleHidDeviceManagerImpl::ProfileCount ||
           impl_->inputValueHandles[reportId - 1] == 0)
  {
    return false;
  }
  uint16_t connectionHandles[ConnectionCapacity] = {};
  bool anyPeripheral = false;
  return eligibleConnections(
           reportId,
           customSlot,
           useBootKeyboard(reportId),
           connectionHandles,
           ConnectionCapacity,
           anyPeripheral) != 0;
}

bool EspBleHidKeyboard::ready() const
{
  return readyFor(ESP_BLE_HID_REPORT_ID_KEYBOARD, -1);
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

  const bool useBoot = useBootKeyboard(reportId);

  uint16_t connectionHandles[ConnectionCapacity] = {};
  bool anyPeripheral = false;
  size_t connectionCount = eligibleConnections(
    reportId, -1, useBoot, connectionHandles, ConnectionCapacity, anyPeripheral);
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
  size_t notifyLength = length;
  uint16_t notifyHandle = impl_->inputValueHandles[reportIndex];
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (useBoot)
    {
      // The Boot Keyboard Input Report is always the fixed 8-byte layout
      // [modifiers, reserved, keycode1..6]. A Report-protocol 6KRO report
      // already matches; an NKRO bitmap is down-converted to keycodes.
      uint8_t boot[8] = {};
      if (length == 8)
      {
        memcpy(boot, data, 8);
      }
      else
      {
        boot[0] = data[0]; // modifiers
        size_t keyIndex = 2;
        for (size_t byte = 1; byte < length && keyIndex < 8; ++byte)
        {
          for (uint8_t bit = 0; bit < 8; ++bit)
          {
            if ((data[byte] & static_cast<uint8_t>(1u << bit)) == 0) continue;
            if (keyIndex >= 8)
            {
              memset(boot + 2, 0x01, 6); // rollover: too many keys
              break;
            }
            boot[keyIndex++] = static_cast<uint8_t>(((byte - 1) << 3) + bit);
          }
        }
      }
      memcpy(impl_->bootKeyboardInput, boot, sizeof(boot));
      memcpy(inputValue, boot, sizeof(boot));
      notifyLength = sizeof(boot);
      notifyHandle = impl_->bootKeyboardInputValueHandle;
    }
    else
    {
      memcpy(impl_->inputValues[reportIndex], data, length);
      impl_->inputLengths[reportIndex] = static_cast<uint8_t>(length);
      if (length < sizeof(impl_->inputValues[reportIndex]))
      {
        memset(impl_->inputValues[reportIndex] + length, 0,
               sizeof(impl_->inputValues[reportIndex]) - length);
      }
      memcpy(inputValue, impl_->inputValues[reportIndex], length);
    }
  }

  bool sent = false;
  int lastBackendCode = 0;
  for (size_t index = 0; index < connectionCount; ++index)
  {
    os_mbuf *value = ble_hs_mbuf_from_flat(inputValue, notifyLength);
    if (value == nullptr)
    {
      lastBackendCode = BLE_HS_ENOMEM;
      continue;
    }
    const int backendCode = ble_gatts_notify_custom(
      connectionHandles[index], notifyHandle, value);
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

int EspBleHidKeyboard::customInputSlot(uint8_t reportId) const
{
  if (impl_ == nullptr) return -1;
  for (size_t index = 0; index < impl_->customReportCount; ++index)
  {
    if (impl_->customReports[index].reportType == ESP_BLE_HID_REPORT_TYPE_INPUT &&
        impl_->customReports[index].reportId == reportId)
    {
      return static_cast<int>(index);
    }
  }
  return -1;
}

bool EspBleHidKeyboard::sendCustomInput(uint8_t reportId, const uint8_t *data, size_t length)
{
  if (!owner_->initialized() || impl_ == nullptr || !impl_->realized)
  {
    owner_->setError(EspBleError::InvalidState, "HID Custom Device is not initialized");
    return false;
  }
  const int slot = customInputSlot(reportId);
  if (slot < 0 || impl_->customReports[slot].valueHandle == 0)
  {
    owner_->setError(EspBleError::NotFound, "unknown custom HID input report");
    return false;
  }
  EspBleHidDeviceManagerImpl::CustomReport &report = impl_->customReports[slot];
  if (data == nullptr || length == 0 || length != report.size)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid custom HID input report length");
    return false;
  }

  uint16_t connectionHandles[ConnectionCapacity] = {};
  bool anyPeripheral = false;
  const size_t connectionCount = eligibleConnections(
    reportId, slot, false, connectionHandles, ConnectionCapacity, anyPeripheral);
  if (connectionCount == 0)
  {
    owner_->setError(
      EspBleError::InvalidState,
      anyPeripheral ? "no subscribed HID Host" : "no connected HID Host");
    return false;
  }

  uint8_t value[EspBleHidDeviceManagerImpl::MaxVendorReportSize] = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    memcpy(report.value, data, length);
    report.length = length;
    memcpy(value, data, length);
  }

  bool sent = false;
  int lastBackendCode = 0;
  for (size_t index = 0; index < connectionCount; ++index)
  {
    os_mbuf *buffer = ble_hs_mbuf_from_flat(value, length);
    if (buffer == nullptr)
    {
      lastBackendCode = BLE_HS_ENOMEM;
      continue;
    }
    const int backendCode =
      ble_gatts_notify_custom(connectionHandles[index], report.valueHandle, buffer);
    if (backendCode == 0) sent = true;
    else lastBackendCode = backendCode;
  }
  if (!sent)
  {
    owner_->setError(
      EspBleError::BackendFailure,
      (String("failed to notify custom HID input report, backend code ") + lastBackendCode).c_str());
    return false;
  }
  owner_->clearError();
  return true;
}

bool EspBleHidKeyboard::pressUsage(uint8_t usage, uint8_t modifiers, uint32_t)
{
  if (nkroEnabled_)
  {
    // press() reports a usage this report cannot represent; keep the error
    // detail the incremental API has always returned for it.
    if (!nkroState_.press(usage))
    {
      owner_->setError(EspBleError::InvalidArgument, "NKRO keyboard usage must be at most 0xe7");
      return false;
    }
    nkroState_.modifiers |= modifiers;
    return sendHeldNkroState();
  }
  EspBleHidKeyboardReport report;
  report.modifiers = modifiers;
  report.keys[0] = usage;
  return sendReport(report);
}

bool EspBleHidKeyboard::releaseUsage(uint8_t usage)
{
  if (!nkroEnabled_) return releaseAll();
  if (!nkroState_.release(usage))
  {
    owner_->setError(EspBleError::InvalidArgument, "NKRO keyboard usage must be at most 0xe7");
    return false;
  }
  return sendHeldNkroState();
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
  nkroState_.clear();
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

uint8_t EspBleHidKeyboard::protocolMode() const
{
  if (impl_ == nullptr) return ReportProtocolMode;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->protocolMode;
}

void EspBleHidKeyboard::onProtocolMode(ProtocolModeCallback callback)
{
  protocolModeCallback_ = std::move(callback);
}

bool EspBleHidKeyboard::configured() const
{
  return impl_ != nullptr && impl_->configured;
}

void EspBleHidKeyboard::resetBackend()
{
  nkroState_.clear();
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
  impl_->bootKeyboardInputValueHandle = 0;
  memset(impl_->inputValues, 0, sizeof(impl_->inputValues));
  impl_->outputValue = 0;
  // Do not carry a previous host's LED state into the next connection.
  impl_->ledState = EspBleHidKeyboardOutputReport();
  impl_->protocolMode = ReportProtocolMode;
  memset(impl_->bootKeyboardInput, 0, sizeof(impl_->bootKeyboardInput));
  impl_->bootKeyboardOutput = 0;
  impl_->protocolModeCharacteristic = nullptr;
  impl_->bootKeyboardInputCharacteristic = nullptr;
  impl_->bootKeyboardOutputCharacteristic = nullptr;
  // Keep the custom report declarations (report id/type/size/map) so a later
  // begin() re-registers them; only the backend-assigned state is cleared.
  for (size_t index = 0; index < impl_->customReportCount; ++index)
  {
    impl_->customReports[index].valueHandle = 0;
    impl_->customReports[index].characteristic = nullptr;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->protocolModeEventPending = false;
  impl_->outputHead = 0;
  impl_->outputCount = 0;
  impl_->vendorReportHead = 0;
  impl_->vendorReportCount = 0;
  impl_->vendorOutputLength = 0;
  impl_->vendorFeatureLength = 0;
  impl_->customEventHead = 0;
  impl_->customEventCount = 0;
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

void EspBleHidKeyboard::dispatchPendingProtocolMode()
{
  if (impl_ == nullptr || !protocolModeCallback_)
  {
    return;
  }
  uint8_t mode = ReportProtocolMode;
  EspBleConnectionId connectionId = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->protocolModeEventPending)
    {
      return;
    }
    impl_->protocolModeEventPending = false;
    mode = impl_->protocolModeEventValue;
    connectionId = impl_->protocolModeEventConnectionId;
  }
  protocolModeCallback_(mode, connectionId);
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

bool EspBleHidMouse::ready() const
{ return owner_->hidKeyboardDevice_.readyFor(ESP_BLE_HID_REPORT_ID_MOUSE, -1); }
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

bool EspBleHidConsumerControl::ready() const
{ return owner_->hidKeyboardDevice_.readyFor(ESP_BLE_HID_REPORT_ID_CONSUMER_CONTROL, -1); }
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

bool EspBleHidSystemControl::ready() const
{ return owner_->hidKeyboardDevice_.readyFor(ESP_BLE_HID_REPORT_ID_SYSTEM_CONTROL, -1); }
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

bool EspBleHidGamepad::ready() const
{ return owner_->hidKeyboardDevice_.readyFor(ESP_BLE_HID_REPORT_ID_GAMEPAD, -1); }
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

bool EspBleHidVendor::ready() const
{ return owner_->hidKeyboardDevice_.readyFor(ESP_BLE_HID_REPORT_ID_VENDOR, -1); }

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

bool EspBleHidCustom::configure(const EspBleHidDeviceConfig &config)
{
  configured_ = owner_->hidKeyboardDevice_.configureCustom(config);
  return configured_;
}

bool EspBleHidCustom::configured() const { return configured_; }

bool EspBleHidCustom::ready(uint8_t reportId) const
{
  const int slot = owner_->hidKeyboardDevice_.customInputSlot(reportId);
  if (slot < 0) return false;
  return owner_->hidKeyboardDevice_.readyFor(reportId, slot);
}

bool EspBleHidCustom::setReportMap(const uint8_t *descriptor, size_t length)
{
  if (!configured_ || owner_->hidKeyboardDevice_.impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidState, "call hidCustom().configure() first");
    return false;
  }
  EspBleHidDeviceManagerImpl *impl = owner_->hidKeyboardDevice_.impl_;
  if (descriptor == nullptr || length == 0 ||
      length > EspBleHidDeviceManagerImpl::CustomReportMapCapacity)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid custom HID report descriptor");
    return false;
  }
  memcpy(impl->customReportMap, descriptor, length);
  impl->customReportMapLength = length;
  owner_->clearError();
  return true;
}

bool EspBleHidCustom::addReport(uint8_t reportId, uint8_t reportType, uint16_t sizeBytes)
{
  if (!configured_ || owner_->hidKeyboardDevice_.impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidState, "call hidCustom().configure() first");
    return false;
  }
  EspBleHidDeviceManagerImpl *impl = owner_->hidKeyboardDevice_.impl_;
  if (reportId == 0 || sizeBytes == 0 ||
      sizeBytes > EspBleHidDeviceManagerImpl::MaxVendorReportSize)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid custom HID report id or size");
    return false;
  }
  // Report IDs 1..6 are reserved for the built-in profiles when one is enabled.
  if (reportId <= EspBleHidDeviceManagerImpl::ProfileCount &&
      (impl->profileMask & static_cast<uint8_t>(1u << (reportId - 1))) != 0)
  {
    owner_->setError(EspBleError::InvalidArgument,
      "custom HID report id conflicts with an enabled built-in profile");
    return false;
  }
  for (size_t index = 0; index < impl->customReportCount; ++index)
  {
    if (impl->customReports[index].reportId == reportId &&
        impl->customReports[index].reportType == reportType)
    {
      owner_->setError(EspBleError::InvalidArgument, "duplicate custom HID report");
      return false;
    }
  }
  if (impl->customReportCount == EspBleHidDeviceManagerImpl::MaxCustomReports)
  {
    owner_->setError(EspBleError::ResourceExhausted, "too many custom HID reports");
    return false;
  }
  EspBleHidDeviceManagerImpl::CustomReport &report =
    impl->customReports[impl->customReportCount++];
  report.reportId = reportId;
  report.reportType = reportType;
  report.size = sizeBytes;
  report.length = sizeBytes;
  owner_->clearError();
  return true;
}

bool EspBleHidCustom::addInputReport(uint8_t reportId, uint16_t sizeBytes)
{
  return addReport(reportId, ESP_BLE_HID_REPORT_TYPE_INPUT, sizeBytes);
}

bool EspBleHidCustom::addOutputReport(uint8_t reportId, uint16_t sizeBytes)
{
  return addReport(reportId, ESP_BLE_HID_REPORT_TYPE_OUTPUT, sizeBytes);
}

bool EspBleHidCustom::addFeatureReport(uint8_t reportId, uint16_t sizeBytes)
{
  return addReport(reportId, ESP_BLE_HID_REPORT_TYPE_FEATURE, sizeBytes);
}

bool EspBleHidCustom::sendInput(uint8_t reportId, const uint8_t *data, size_t length)
{
  return owner_->hidKeyboardDevice_.sendCustomInput(reportId, data, length);
}

void EspBleHidCustom::onOutputReport(ReportCallback callback)
{
  outputCallback_ = std::move(callback);
}

void EspBleHidCustom::onFeatureReport(ReportCallback callback)
{
  featureCallback_ = std::move(callback);
}

void EspBleHidCustom::dispatchPendingReports()
{
  EspBleHidDeviceManagerImpl *impl = owner_->hidKeyboardDevice_.impl_;
  if (impl == nullptr) return;
  while (true)
  {
    EspBleHidDeviceManagerImpl::VendorReportEntry entry;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->customEventCount == 0) break;
      entry = impl->customEvents[impl->customEventHead];
      impl->customEventHead =
        (impl->customEventHead + 1) % EspBleHidDeviceManagerImpl::OutputQueueCapacity;
      --impl->customEventCount;
    }
    EspBleHidVendorReport report;
    report.connectionId = entry.connectionId;
    report.reportId = entry.reportId;
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

  // Queue the discovery on the shared GATT engine rather than racing it against
  // in-flight generic operations (e.g. the persistent-subscription auto-resubscribe
  // fired on Connected). The worker is launched from pumpGattQueue() via
  // runQueuedDiscovery() once the ATT channel is free, and the connection is
  // validated there. The generous timeout covers the multi-step blocking reads;
  // HidDiscover is exempt from the generic timeout event since its worker bounds
  // and completes it (see expireGattOperation()).
  return owner_->startGattOperation(
    EspBleGattOperation::HidDiscover, connectionId,
    nullptr, nullptr, nullptr, 0, true, nullptr, 20000, 0);
}

bool EspBleHidHost::runQueuedDiscovery(EspBleConnectionId connectionId)
{
  if (impl_ == nullptr) return false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
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
    // discover() already returned true (the op was accepted), so surface this
    // late worker-creation failure through the normal discovery event.
    EspBleHidKeyboardHostImpl::Event event;
    event.type = EspBleHidKeyboardHostImpl::EventType::Discovery;
    event.discovery.connectionId = connectionId;
    event.discovery.error = EspBleError::ResourceExhausted;
    event.discovery.detail = "failed to create HID discovery task";
    impl_->pushEvent(event);
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->discoveryTask = task;
  }
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

  // Kept synchronous (Write Without Response, fire-and-forget) rather than
  // routed through the queued GATT engine: on the bundled backend, a worker-task
  // write to the output report concurrent with the HID input-report subscriptions
  // on the same client breaks notification delivery (verified on hardware — HID
  // input stops arriving right after a queued output write). The gattOperating
  // gate serializes this inline write against the worker safely.
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

  uint16_t outputReport = 0;
  bool writeWithoutResponse = false;
  uint16_t connectionHandle = 0xffff;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    EspBleHidKeyboardHostImpl::Connection *connection = impl_->findConnection(connectionId);
    if (connection != nullptr)
    {
      outputReport = connection->outputReport;
      writeWithoutResponse = connection->outputReportNoResponse;
      connectionHandle = connection->connectionHandle;
    }
  }
  if (outputReport == 0)
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
  const bool success = espble_raw::writeHandle(
    connectionHandle, outputReport, &leds, 1, !writeWithoutResponse);
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
  // Kept synchronous for the same reason as setKeyboardLeds(): a queued
  // worker-task write concurrent with the HID input subscriptions breaks the
  // bundled backend's notification delivery.
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

  uint16_t report = 0;
  bool writeWithoutResponse = false;
  uint16_t connectionHandle = 0xffff;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    EspBleHidKeyboardHostImpl::Connection *connection = impl_->findConnection(connectionId);
    if (connection != nullptr)
    {
      report = feature ? connection->vendorFeatureReport : connection->vendorOutputReport;
      writeWithoutResponse = !feature && connection->vendorOutputReportNoResponse;
      connectionHandle = connection->connectionHandle;
    }
  }
  if (report == 0)
  {
    std::lock_guard<std::mutex> ownerLock(owner_->impl_->mutex);
    owner_->impl_->gattOperating = false;
    owner_->setError(EspBleError::NotFound,
      feature ? "HID Vendor Feature Report was not found"
              : "HID Vendor Output Report was not found");
    return false;
  }

  // A Feature Report is always written with a response; it is configuration,
  // not a stream of input.
  const bool success = espble_raw::writeHandle(
    connectionHandle, report, data, length, !writeWithoutResponse);
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

void EspBleHidHost::setAutoRediscover(bool enable)
{
  autoRediscover_ = enable;
}

bool EspBleHidHost::autoRediscover() const
{
  return autoRediscover_;
}

void EspBleHidHost::rememberRediscoverPeer(const String &address)
{
  if (address.length() == 0) return;
  for (const String &entry : rediscoverPeers_)
  {
    if (entry.equalsIgnoreCase(address)) return; // already known
  }
  for (String &entry : rediscoverPeers_)
  {
    if (entry.length() == 0)
    {
      entry = address;
      return;
    }
  }
  // Full: drop silently. The set is bounded by the max simultaneous HID peers,
  // so this only happens if more distinct peers were discovered than can be
  // connected at once; the oldest simply will not auto-rediscover.
}

void EspBleHidHost::handleSecurityEstablished(const EspBleSecurityChanged &event)
{
  if (!autoRediscover_ || !event.success) return;
  // HID Host connections are Central; ignore Peripheral security events.
  if (event.connection.localRole != EspBleRole::Central) return;
  bool known = false;
  for (const String &entry : rediscoverPeers_)
  {
    if (entry.length() != 0 && entry.equalsIgnoreCase(event.connection.peerAddress))
    {
      known = true;
      break;
    }
  }
  if (!known) return;
  // Skip when a discovery is already queued or running for this connection so a
  // manual discover() from the app's onSecurityChanged is not duplicated.
  if (owner_->hasPendingHidDiscover(event.connection.id)) return;
  discover(event.connection.id);
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
  memcpy(event.state.changedBitmap, connection->bitmap, sizeof(event.state.changedBitmap));
  bool hasHeldKey = connection->modifiers != 0;
  for (uint8_t value : connection->bitmap)
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
  for (String &entry : rediscoverPeers_) entry = String();
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
      // Remember a successfully discovered HID peer so auto-rediscover can act on
      // its next reconnection. Both this and handleSecurityEstablished() run on
      // the loop task, so rediscoverPeers_ needs no lock.
      if (autoRediscover_ && event.discovery.success)
      {
        String address;
        {
          std::lock_guard<std::mutex> lock(owner_->impl_->mutex);
          address = owner_->impl_->connectionAddressLocked(event.discovery.connectionId);
        }
        rememberRediscoverPeer(address);
      }
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
          if ((event.state.changedBitmap[usage >> 3] &
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
            if ((event.state.changedBitmap[usage >> 3] & mask) == 0 ||
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
      hidVendor_(this), hidCustom_(this), hidKeyboardHost_(this)
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
  if (config.preferredMtu < 23 || config.preferredMtu > 517)
  {
    setError(EspBleError::InvalidArgument, "preferred MTU must be between 23 and 517");
    return false;
  }
  // An NKRO keyboard notifies a 29-byte report, which needs MTU >= 32 (29 + the
  // 3-byte ATT header). Reject up front instead of letting every report notify
  // fail silently against the MTU payload guard.
  if (hidKeyboardDevice_.configured() && hidKeyboardDevice_.nkroEnabled() &&
      config.preferredMtu < 32)
  {
    setError(
      EspBleError::InvalidArgument,
      "NKRO keyboard requires preferredMtu >= 32 (29-byte report + 3-byte ATT header)");
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
      config.security.ioCapability != EspBleSecurityIoCapability::KeyboardOnly &&
      config.security.ioCapability != EspBleSecurityIoCapability::DisplayYesNo)
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
      config.security.ioCapability == EspBleSecurityIoCapability::None)
  {
    setError(
      EspBleError::InvalidArgument,
      "MITM requires DisplayOnly, KeyboardOnly or DisplayYesNo capability");
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
  if (!startNimbleHost())
  {
    setError(EspBleError::BackendFailure, "the BLE controller did not start");
    return false;
  }
  ble_svc_gap_device_name_set(deviceName);
  // Recorded here because starting the GATT server restores the GAP device
  // name, and that can happen while begin() is still running (HID realize).
  activeDeviceName_ = deviceName;
  if (ble_att_set_preferred_mtu(config.preferredMtu) != 0)
  {
    stopNimbleHost();
    setError(EspBleError::BackendFailure, "failed to set preferred MTU");
    return false;
  }

  // Address privacy: present a random static address, or a rotating Resolvable
  // Private Address, instead of the factory public address. Both need a random
  // static identity set first; for RPA the controller then derives the rotating
  // addresses from it. Applied before any advertising/scanning starts.
  if (config.ownAddressType != EspBleOwnAddressType::Public)
  {
    ble_addr_t randomAddress{};
    if (ble_hs_id_gen_rnd(0, &randomAddress) != 0 ||
        ble_hs_id_set_rnd(randomAddress.val) != 0)
    {
      stopNimbleHost();
      setError(EspBleError::BackendFailure, "failed to set a random device address");
      return false;
    }
    // Every supported SoC generates the Resolvable Private Address in the
    // controller (the original ESP32, whose controller cannot, has no NimBLE
    // build and is rejected at compile time).
    const uint8_t ownType =
      config.ownAddressType == EspBleOwnAddressType::ResolvablePrivate
        ? BLE_OWN_ADDR_RPA_RANDOM_DEFAULT
        : BLE_OWN_ADDR_RANDOM;
    if (ble_hs_id_copy_addr(ownType & 1, nullptr, nullptr) != 0)
    {
      stopNimbleHost();
      setError(EspBleError::BackendFailure, "failed to set the own address type");
      return false;
    }
  }

  if (impl_ == nullptr)
  {
    impl_ = new EspBleImpl(this);
    if (impl_ == nullptr)
    {
      stopNimbleHost();
      setError(EspBleError::ResourceExhausted, "failed to allocate connection state");
      return false;
    }
  }

  impl_->ownAddressType = config.ownAddressType == EspBleOwnAddressType::Public
    ? BLE_OWN_ADDR_PUBLIC
    : (config.ownAddressType == EspBleOwnAddressType::RandomStatic
         ? BLE_OWN_ADDR_RANDOM
         : BLE_OWN_ADDR_RPA_RANDOM_DEFAULT);
  impl_->securityEnabled = config.security.enabled;
  impl_->pairOnConnect = config.security.pairOnConnect;
  impl_->persistentSubscriptionsEnabled = config.persistentSubscriptions;
  impl_->autoReconnectEnabled = autoReconnect_;
  {
    std::lock_guard<std::mutex> lock(impl_->passkeyMutex);
    impl_->staticPasskeyEnabled = config.security.staticPasskeyEnabled;
    impl_->staticPasskey = config.security.staticPasskey;
    impl_->passkeyProvided = false;
  }
  applySecurityConfiguration(config.security);

  if (!gattServer_.realize())
  {
    clearSecurityConfiguration();
    stopNimbleHost();
    gattServer_.resetBackend();
    delete impl_;
    impl_ = nullptr;
    return false;
  }
  if (!hidKeyboardDevice_.realize())
  {
    clearSecurityConfiguration();
    hidKeyboardDevice_.resetBackend();
    stopNimbleHost();
    gattServer_.resetBackend();
    delete impl_;
    impl_ = nullptr;
    return false;
  }

  if (!impl_->gapEventListenerRegistered &&
      ble_gap_event_listener_register(
        &impl_->gapEventListener,
        EspBleImpl::gapEventListenerEntry,
        impl_) == 0)
  {
    impl_->gapEventListenerRegistered = true;
  }

  activeDeviceName_ = deviceName;
  activePreferredMtu_ = config.preferredMtu;
  activeOwnAddressType_ = config.ownAddressType;
  activeSecurity_ = config.security;
  initialized_ = true;
  clearError();
  return true;
}

// The Security Manager is configured through ble_hs_cfg: what this device can
// display or type, whether it demands MITM protection, and which keys are
// exchanged. Bonding keys are distributed both ways so a bond can be restored
// from either side.
void EspBle::applySecurityConfiguration(const EspBleSecurityConfig &security)
{
  if (!security.enabled)
  {
    clearSecurityConfiguration();
    return;
  }
  uint8_t ioCapability = BLE_HS_IO_NO_INPUT_OUTPUT;
  if (security.ioCapability == EspBleSecurityIoCapability::DisplayOnly)
  {
    ioCapability = BLE_HS_IO_DISPLAY_ONLY;
  }
  else if (security.ioCapability == EspBleSecurityIoCapability::KeyboardOnly)
  {
    ioCapability = BLE_HS_IO_KEYBOARD_ONLY;
  }
  else if (security.ioCapability == EspBleSecurityIoCapability::DisplayYesNo)
  {
    ioCapability = BLE_HS_IO_DISPLAY_YESNO;
  }
  ble_hs_cfg.sm_io_cap = ioCapability;
  ble_hs_cfg.sm_bonding = security.bonding ? 1 : 0;
  ble_hs_cfg.sm_mitm = security.mitm ? 1 : 0;
  // LE Secure Connections: the modern pairing algorithm. Legacy pairing is only
  // needed for peers older than BLE 4.2 and is materially weaker.
  ble_hs_cfg.sm_sc = 1;
  ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
}

void EspBle::clearSecurityConfiguration()
{
  ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
  ble_hs_cfg.sm_bonding = 0;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_sc = 1;
  ble_hs_cfg.sm_our_key_dist = 0;
  ble_hs_cfg.sm_their_key_dist = 0;
}

void EspBle::end()
{
  if (!initialized_)
  {
    return;
  }

  if (scanner_.isScanning())
  {
    ble_gap_disc_cancel();
  }
  if (advertising_.isAdvertising())
  {
    ble_gap_adv_stop();
  }

  if (impl_ != nullptr)
  {
    impl_->gattServerStarted = false;
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
        // Abort an in-flight connect attempt instead of blocking here until it
        // times out. Repeated calls are harmless.
        ble_gap_conn_cancel();
      }
      delay(1);
    }
  }
  if (gattServer_.impl_ != nullptr)
  {
    {
      // Drop queued-but-unstarted sends; update() no longer runs to pump them.
      std::lock_guard<std::mutex> lock(gattServer_.impl_->mutex);
      gattServer_.impl_->sendQueueHead = 0;
      gattServer_.impl_->sendQueueCount = 0;
    }
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
  clearSecurityConfiguration();
  hidKeyboardDevice_.resetBackend();
  hidKeyboardHost_.resetBackend();
  scanner_.flushPendingResults();
  if (impl_ != nullptr)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (EspBleImpl::ConnectionSlot &slot : impl_->connections)
    {
      slot = EspBleImpl::ConnectionSlot();
    }
  }
  if (impl_ != nullptr && impl_->gapEventListenerRegistered)
  {
    ble_gap_event_listener_unregister(&impl_->gapEventListener);
    impl_->gapEventListenerRegistered = false;
  }
  stopNimbleHost();
  initialized_ = false;
  gattServer_.resetBackend();
  // deinit() drops the backend accept list, so drop the mirror with it.
  for (size_t index = 0; index < acceptListCount_; ++index)
  {
    acceptList_[index] = EspBleBond();
  }
  acceptListCount_ = 0;

  delete impl_;
  impl_ = nullptr;
}

// The caller asked for a timeout; hold the stack to it. ble_gap_conn_cancel()
// is a real cancel now that the attempt is a GAP procedure of ours, so the
// failure is reported from the event it produces rather than guessed at here.
void EspBle::cancelExpiredConnectAttempt()
{
  if (impl_ == nullptr) return;
  bool cancel = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->connecting && !impl_->connectCancelRequested &&
        static_cast<int32_t>(millis() - impl_->connectDeadlineMilliseconds) >= 0)
    {
      impl_->connectCancelRequested = true;
      cancel = true;
    }
  }
  if (cancel) ble_gap_conn_cancel();
}

void EspBle::drainPendingDisconnects()
{
  if (impl_ == nullptr) return;
  struct Target
  {
    uint16_t handle = 0xffff;
    uint8_t reason = 0x13;
  };
  Target targets[ConnectionCapacity];
  size_t count = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (EspBleImpl::ConnectionSlot &slot : impl_->connections)
    {
      if (!slot.used || !slot.pendingDisconnect) continue;
      // Still wait while a GATT op is in flight on this connection.
      if (impl_->gattOperating && impl_->gattConnectionId == slot.connection.id) continue;
      slot.pendingDisconnect = false;
      targets[count].handle = slot.connection.handle;
      targets[count].reason = slot.pendingDisconnectReason;
      ++count;
    }
  }
  for (size_t i = 0; i < count; ++i)
  {
    ble_gap_terminate(targets[i].handle, targets[i].reason);
  }
}

void EspBle::update()
{
  cancelExpiredConnectAttempt();
  expireGattOperation();
  pumpGattQueue();
  pumpSendQueue();
  releaseDeferredNotifications();
  drainPendingDisconnects();
  scanner_.dispatchPendingResults();
  dispatchConnectionEvents();
  driveAutoReconnect();
  hidKeyboardDevice_.dispatchPendingOutputReports();
  hidKeyboardDevice_.dispatchPendingProtocolMode();
  hidVendor_.dispatchPendingReports();
  hidCustom_.dispatchPendingReports();
  hidKeyboardHost_.dispatchPendingEvents();
}

// Backstop for the notification-ordering gate: with no GATT operation in
// flight there is no completion left to order against, so anything still held
// back is released. Needed because a completion callback that lands after its
// operation timed out raises the flag with no worker left to clear it.
void EspBle::releaseDeferredNotifications()
{
  if (impl_ == nullptr) return;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->gattOperating || impl_->deferredNotificationCount == 0) return;
  impl_->gattCompletionPending = false;
  impl_->flushDeferredNotificationsLocked();
}

void EspBle::driveAutoReconnect()
{
  if (impl_ == nullptr) return;

  String address;
  EspBleAddressType addressType = EspBleAddressType::Public;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->autoReconnectEnabled || impl_->connecting) return;
    const uint32_t now = millis();
    // Pick one due target that is not already connected. connect() serializes
    // attempts (the `connecting` flag), so the rest wait for a later tick.
    for (EspBleImpl::DesiredConnection &entry : impl_->desiredConnections)
    {
      if (!entry.used || now < entry.nextAttemptMs) continue;
      if (impl_->isCentralConnectedToLocked(entry.address)) continue;
      address = entry.address;
      addressType = entry.addressType;
      entry.nextAttemptMs = now + EspBleImpl::ReconnectIntervalMilliseconds;
      break;
    }
  }
  if (address.length() != 0)
  {
    // connect() re-checks `connecting` atomically; a false return just means the
    // attempt is retried on a later tick.
    connect(address.c_str(), addressType);
  }
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

  // HID discovery is a queued operation whose own worker bounds and completes it
  // (emitting a discovery event); it must not raise a generic GATT timeout
  // GattResult here (which would carry stale UUIDs and the wrong event type).
  if (impl_->gattOperation == EspBleGattOperation::HidDiscover)
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
  event.gattResult.handle = impl_->gattCharacteristicHandle;
  event.gattResult.descriptorHandle = impl_->gattDescriptorHandle;
  event.gattResult.response = impl_->gattWriteResponse;
  event.gattResult.error = EspBleError::Timeout;
  event.gattResult.detail = "GATT operation timed out";
  impl_->pushEvent(event);
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
    impl_->connectDeadlineMilliseconds = millis() + timeoutMilliseconds;
    impl_->connectCancelRequested = false;
    impl_->connecting = true;
  }

  ble_addr_t peer{};
  peer.type = static_cast<uint8_t>(scanResult.addressType);
  parseAddress(scanResult.address.c_str(), peer.val);

  // A scan and a connection cannot run at once: the controller has one
  // initiator. Stop scanning rather than let ble_gap_connect() fail with
  // BLE_HS_EBUSY, which is what the application would have had to do anyway.
  if (ble_gap_disc_active()) ble_gap_disc_cancel();

  // Asynchronous: the host reports the outcome through centralGapEvent(), and
  // enforces the timeout itself -- no worker task, and a cancel actually
  // cancels.
  const int status = ble_gap_connect(
    impl_->ownAddressType, &peer, static_cast<int32_t>(timeoutMilliseconds), nullptr,
    &EspBleImpl::centralGapEvent, impl_);
  if (status != 0)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->connecting = false;
    setError(EspBleError::BackendFailure, "failed to start the connection attempt");
    return false;
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

bool EspBle::disconnect(EspBleConnectionId connectionId, uint8_t reason)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }

  uint16_t handle = 0xffff;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    EspBleImpl::ConnectionSlot *found = nullptr;
    for (EspBleImpl::ConnectionSlot &slot : impl_->connections)
    {
      if (slot.used && slot.connection.id == connectionId)
      {
        found = &slot;
        break;
      }
    }
    if (found == nullptr)
    {
      setError(EspBleError::InvalidArgument, "connection ID was not found");
      return false;
    }
    // A disconnect() is intentional: stop auto-reconnecting to this peer.
    if (found->connection.localRole == EspBleRole::Central)
    {
      impl_->forgetDesiredLocked(found->connection.peerAddress);
    }
    // Queued work aimed at a peer we are dropping is pointless, and leaving it in
    // the queue would hold the disconnect back: update() pumps the queue before it
    // drains pending disconnects, so each pump would start the next op and the
    // disconnect could be starved indefinitely by an application that keeps
    // enqueueing. Drop it here, with a failure completion each.
    impl_->purgeQueuedGattOpsLocked(connectionId);
    if (impl_->gattOperating && impl_->gattConnectionId == connectionId)
    {
      // A GATT op is in flight on this connection; defer the disconnect until it
      // completes (drainPendingDisconnects() from update()) instead of rejecting
      // and tearing the client down under the running worker.
      found->pendingDisconnect = true;
      found->pendingDisconnectReason = reason;
      clearError();
      return true;
    }
    handle = found->connection.handle;
  }

  if (handle == 0xffff)
  {
    setError(EspBleError::InvalidArgument, "connection ID was not found");
    return false;
  }

  // Same call for both roles: a connection is a connection once it exists.
  if (ble_gap_terminate(handle, reason) != 0)
  {
    setError(EspBleError::BackendFailure, "failed to request disconnection");
    return false;
  }

  clearError();
  return true;
}

void EspBle::setAutoReconnect(bool enabled)
{
  autoReconnect_ = enabled;
  if (impl_ == nullptr) return;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->autoReconnectEnabled = enabled;
  if (!enabled)
  {
    impl_->clearDesiredLocked();
    return;
  }
  // Adopt the peers this central is already connected to, so a drop after
  // enabling is reconnected too.
  for (const EspBleImpl::ConnectionSlot &slot : impl_->connections)
  {
    if (slot.used && slot.connection.localRole == EspBleRole::Central)
    {
      impl_->rememberDesiredLocked(slot.connection.peerAddress, slot.connection.peerAddressType);
    }
  }
}

bool EspBle::autoReconnect() const
{
  return autoReconnect_;
}

bool EspBle::updateConnectionParameters(
  EspBleConnectionId connectionId,
  uint16_t minInterval,
  uint16_t maxInterval,
  uint16_t latency,
  uint16_t supervisionTimeout)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (minInterval > maxInterval)
  {
    setError(EspBleError::InvalidArgument, "minInterval must not exceed maxInterval");
    return false;
  }

  uint16_t handle = 0xffff;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (const EspBleImpl::ConnectionSlot &slot : impl_->connections)
    {
      if (slot.used && slot.connection.id == connectionId)
      {
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

  ble_gap_upd_params parameters{};
  parameters.itvl_min = minInterval;
  parameters.itvl_max = maxInterval;
  parameters.latency = latency;
  parameters.supervision_timeout = supervisionTimeout;
  // The event-length window the controller may use for this connection. Zero
  // and 0x0300 are the host's own defaults; a peripheral's request carries them
  // just as a central's does.
  parameters.min_ce_len = 0;
  parameters.max_ce_len = 0x0300;
  if (ble_gap_update_params(handle, &parameters) != 0)
  {
    setError(EspBleError::BackendFailure, "failed to request connection parameter update");
    return false;
  }

  clearError();
  return true;
}

bool EspBle::updatePhy(EspBleConnectionId connectionId, uint8_t txPhyMask, uint8_t rxPhyMask)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (txPhyMask == 0 || rxPhyMask == 0)
  {
    setError(EspBleError::InvalidArgument, "PHY masks must select at least one PHY");
    return false;
  }

  uint16_t handle = 0xffff;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (const EspBleImpl::ConnectionSlot &slot : impl_->connections)
    {
      if (slot.used && slot.connection.id == connectionId)
      {
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

  // The PHY preference is a link-layer setting, independent of GATT role.
  if (ble_gap_set_prefered_le_phy(handle, txPhyMask, rxPhyMask, 0) != 0)
  {
    setError(EspBleError::BackendFailure, "failed to request PHY update");
    return false;
  }

  clearError();
  return true;
}

bool EspBle::notifyServicesChanged(uint16_t startHandle, uint16_t endHandle)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (startHandle > endHandle)
  {
    setError(EspBleError::InvalidArgument, "startHandle must not exceed endHandle");
    return false;
  }

  // The backend registers the Generic Attribute service and its Service Changed
  // characteristic; this indicates the changed range to subscribed clients.
  ble_svc_gatt_changed(startHandle, endHandle);
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

size_t EspBle::droppedPersistentSubscriptionCount() const
{
  if (impl_ == nullptr)
  {
    return 0;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedPersistentSubscriptions;
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

  // BLE_HS_EALREADY means the link is already encrypted, which is the state the
  // caller asked for.
  const int status = ble_gap_security_initiate(connectionHandle);
  if (status != 0 && status != BLE_HS_EALREADY)
  {
    setError(
      EspBleError::BackendFailure,
      (String("failed to request BLE security, backend code ") + status).c_str());
    return false;
  }
  clearError();
  return true;
}

bool EspBle::syncAcceptList()
{
  // The mirror kept here is authoritative; ble_gap_wl_set() overwrites the
  // controller's list with it in one call.
  ble_addr_t entries[MaxAcceptListEntries];
  for (size_t index = 0; index < acceptListCount_; ++index)
  {
    entries[index].type = static_cast<uint8_t>(acceptList_[index].peerAddressType);
    if (!parseAddress(acceptList_[index].peerAddress.c_str(), entries[index].val))
    {
      return false;
    }
  }
  return ble_gap_wl_set(entries, static_cast<uint8_t>(acceptListCount_)) == 0;
}

String EspBle::localAddress() const
{
  if (!initialized_)
  {
    return String();
  }
  // Only the address bytes are asked for; localAddressType() reports the type
  // that was requested at begin().
  uint8_t address[6] = {};
  if (impl_ == nullptr ||
      ble_hs_id_copy_addr(impl_->ownAddressType & 1, address, nullptr) != 0)
  {
    return String();
  }
  return formatAddress(address);
}

EspBleAddressType EspBle::localAddressType() const
{
  return activeOwnAddressType_ == EspBleOwnAddressType::Public
    ? EspBleAddressType::Public
    : EspBleAddressType::Random;
}

namespace
{
// Transmit power levels the radio supports, paired with their dBm value.
struct TxPowerLevel
{
  int8_t dBm;
  esp_power_level_t level;
};
constexpr TxPowerLevel TxPowerLevels[] = {
  {-12, ESP_PWR_LVL_N12}, {-9, ESP_PWR_LVL_N9}, {-6, ESP_PWR_LVL_N6},
  {-3, ESP_PWR_LVL_N3},   {0, ESP_PWR_LVL_N0},  {3, ESP_PWR_LVL_P3},
  {6, ESP_PWR_LVL_P6},    {9, ESP_PWR_LVL_P9},
};
} // namespace

bool EspBle::setTxPower(int8_t dBm)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }

  const TxPowerLevel *nearest = &TxPowerLevels[0];
  for (const TxPowerLevel &candidate : TxPowerLevels)
  {
    if (abs(candidate.dBm - dBm) < abs(nearest->dBm - dBm))
    {
      nearest = &candidate;
    }
  }
  esp_ble_tx_power_set(
    ESP_BLE_PWR_TYPE_DEFAULT, static_cast<esp_power_level_t>(nearest->level));
  clearError();
  return true;
}

int8_t EspBle::txPower() const
{
  if (!initialized_)
  {
    return INT8_MIN;
  }
  const esp_power_level_t level = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
  for (const TxPowerLevel &candidate : TxPowerLevels)
  {
    if (candidate.level == level) return candidate.dBm;
  }
  return INT8_MIN;
}

bool EspBle::addToAcceptList(const char *address, EspBleAddressType addressType)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (address == nullptr || address[0] == '\0')
  {
    setError(EspBleError::InvalidArgument, "accept list address is empty");
    return false;
  }
  if (!isValidAddressType(addressType))
  {
    setError(EspBleError::InvalidArgument, "accept list address type is invalid");
    return false;
  }
  for (size_t index = 0; index < acceptListCount_; ++index)
  {
    if (acceptList_[index].peerAddressType == addressType &&
        acceptList_[index].peerAddress.equalsIgnoreCase(address))
    {
      clearError();
      return true;
    }
  }
  if (acceptListCount_ == MaxAcceptListEntries)
  {
    setError(EspBleError::ResourceExhausted, "accept list is full");
    return false;
  }

  acceptList_[acceptListCount_].peerAddress = address;
  acceptList_[acceptListCount_].peerAddressType = addressType;
  ++acceptListCount_;
  if (!syncAcceptList())
  {
    acceptList_[--acceptListCount_] = EspBleBond();
    syncAcceptList();
    setError(EspBleError::BackendFailure, "failed to write the accept list");
    return false;
  }
  clearError();
  return true;
}

bool EspBle::removeFromAcceptList(const char *address, EspBleAddressType addressType)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (address == nullptr || address[0] == '\0')
  {
    setError(EspBleError::InvalidArgument, "accept list address is empty");
    return false;
  }
  for (size_t index = 0; index < acceptListCount_; ++index)
  {
    if (acceptList_[index].peerAddressType != addressType ||
        !acceptList_[index].peerAddress.equalsIgnoreCase(address))
    {
      continue;
    }
    const EspBleBond removed = acceptList_[index];
    for (size_t next = index + 1; next < acceptListCount_; ++next)
    {
      acceptList_[next - 1] = acceptList_[next];
    }
    acceptList_[--acceptListCount_] = EspBleBond();
    if (!syncAcceptList())
    {
      for (size_t back = acceptListCount_; back > index; --back)
      {
        acceptList_[back] = acceptList_[back - 1];
      }
      acceptList_[index] = removed;
      ++acceptListCount_;
      syncAcceptList();
      setError(EspBleError::BackendFailure, "failed to write the accept list");
      return false;
    }
    clearError();
    return true;
  }
  setError(EspBleError::NotFound, "accept list entry was not found");
  return false;
}

void EspBle::clearAcceptList()
{
  const size_t previousCount = acceptListCount_;
  for (size_t index = 0; index < previousCount; ++index)
  {
    acceptList_[index] = EspBleBond();
  }
  acceptListCount_ = 0;
  if (initialized_ && previousCount != 0)
  {
    // An empty list is written back so the controller stops matching the old
    // entries. A restrictive filter policy then rejects every peer.
    syncAcceptList();
  }
}

size_t EspBle::acceptListCount() const
{
  return acceptListCount_;
}

bool EspBle::acceptListEntry(size_t index, EspBleBond &entry) const
{
  if (index >= acceptListCount_)
  {
    return false;
  }
  entry = acceptList_[index];
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
  bond.peerAddress = formatAddress(peers[index].val);
  bond.peerAddressType = static_cast<EspBleAddressType>(peers[index].type);
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
    if (peers[index].type == static_cast<uint8_t>(bond.peerAddressType) &&
        formatAddress(peers[index].val).equalsIgnoreCase(bond.peerAddress))
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
  std::lock_guard<std::mutex> lock(listenerMutex_);
  connectedListeners_.setPrimary(std::move(callback));
}

void EspBle::onDisconnected(ConnectionCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  disconnectedListeners_.setPrimary(std::move(callback));
}

void EspBle::onConnectionFailed(ConnectionFailureCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  connectionFailedListeners_.setPrimary(std::move(callback));
}

void EspBle::onMtuChanged(MtuChangedCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  mtuChangedListeners_.setPrimary(std::move(callback));
}

void EspBle::onConnectionParametersUpdated(ConnectionCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  connectionParametersUpdatedListeners_.setPrimary(std::move(callback));
}

void EspBle::onPhyUpdated(ConnectionCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  phyUpdatedListeners_.setPrimary(std::move(callback));
}

void EspBle::onSecurityChanged(SecurityChangedCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  securityChangedListeners_.setPrimary(std::move(callback));
}

void EspBle::onPasskeyDisplayed(PasskeyDisplayedCallback callback)
{
  passkeyDisplayedCallback_ = std::move(callback);
}

void EspBle::onNumericComparison(PasskeyDisplayedCallback callback)
{
  numericComparisonCallback_ = std::move(callback);
}

bool EspBle::confirmNumericComparison(bool accept)
{
  if (!initialized_ || impl_ == nullptr)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->passkeyMutex);
    impl_->numericComparisonAccept = accept;
    impl_->numericComparisonConfirmed = true;
  }
  clearError();
  return true;
}

bool EspBle::providePasskey(uint32_t passkey)
{
  if (!initialized_ || impl_ == nullptr)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  if (passkey > 999999)
  {
    setError(EspBleError::InvalidArgument, "BLE passkey must be between 000000 and 999999");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->passkeyMutex);
    impl_->providedPasskey = passkey;
    impl_->passkeyProvided = true;
  }
  clearError();
  return true;
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
  const auto *database = impl_->findDatabaseLocked(connectionId);
  return database != nullptr && database->valid ? database->serviceCount : 0;
}

bool EspBle::discoveredService(
  EspBleConnectionId connectionId,
  size_t index,
  EspBleGattServiceInfo &service) const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const auto *database = impl_->findDatabaseLocked(connectionId);
  if (database == nullptr || !database->valid ||
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
  const auto *database = impl_->findDatabaseLocked(connectionId);
  if (database == nullptr || !database->valid) return 0;
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
  const auto *database = impl_->findDatabaseLocked(connectionId);
  if (database == nullptr || !database->valid) return false;
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
  const auto *database = impl_->findDatabaseLocked(connectionId);
  if (database == nullptr || !database->valid) return 0;
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
  const auto *database = impl_->findDatabaseLocked(connectionId);
  if (database == nullptr || !database->valid) return false;
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

bool EspBle::readCharacteristic(
  EspBleConnectionId connectionId,
  uint16_t characteristicHandle,
  uint32_t timeoutMilliseconds)
{
  if (characteristicHandle == 0)
  {
    setError(EspBleError::InvalidArgument, "characteristic handle must be non-zero");
    return false;
  }
  return startGattOperation(
    EspBleGattOperation::Read, connectionId, nullptr, nullptr,
    nullptr, 0, true, nullptr, timeoutMilliseconds, characteristicHandle);
}

bool EspBle::writeCharacteristic(
  EspBleConnectionId connectionId,
  uint16_t characteristicHandle,
  const uint8_t *data,
  size_t length,
  bool response,
  uint32_t timeoutMilliseconds)
{
  if (characteristicHandle == 0)
  {
    setError(EspBleError::InvalidArgument, "characteristic handle must be non-zero");
    return false;
  }
  return startGattOperation(
    EspBleGattOperation::Write, connectionId, nullptr, nullptr,
    data, length, response, nullptr, timeoutMilliseconds, characteristicHandle);
}

bool EspBle::writeCharacteristic(
  EspBleConnectionId connectionId,
  uint16_t characteristicHandle,
  const String &value,
  bool response,
  uint32_t timeoutMilliseconds)
{
  return writeCharacteristic(
    connectionId,
    characteristicHandle,
    reinterpret_cast<const uint8_t *>(value.c_str()),
    value.length(),
    response,
    timeoutMilliseconds);
}

bool EspBle::subscribe(
  EspBleConnectionId connectionId,
  uint16_t characteristicHandle,
  bool notifications,
  uint32_t timeoutMilliseconds)
{
  if (characteristicHandle == 0)
  {
    setError(EspBleError::InvalidArgument, "characteristic handle must be non-zero");
    return false;
  }
  return startGattOperation(
    EspBleGattOperation::Subscribe, connectionId, nullptr, nullptr,
    nullptr, 0, notifications, nullptr, timeoutMilliseconds, characteristicHandle);
}

bool EspBle::unsubscribe(
  EspBleConnectionId connectionId,
  uint16_t characteristicHandle,
  uint32_t timeoutMilliseconds)
{
  if (characteristicHandle == 0)
  {
    setError(EspBleError::InvalidArgument, "characteristic handle must be non-zero");
    return false;
  }
  return startGattOperation(
    EspBleGattOperation::Unsubscribe, connectionId, nullptr, nullptr,
    nullptr, 0, true, nullptr, timeoutMilliseconds, characteristicHandle);
}

bool EspBle::readDescriptor(
  EspBleConnectionId connectionId,
  uint16_t descriptorHandle,
  uint32_t timeoutMilliseconds)
{
  if (descriptorHandle == 0)
  {
    setError(EspBleError::InvalidArgument, "descriptor handle must be non-zero");
    return false;
  }
  return startGattOperation(
    EspBleGattOperation::ReadDescriptor, connectionId, nullptr, nullptr,
    nullptr, 0, true, nullptr, timeoutMilliseconds, 0, descriptorHandle);
}

bool EspBle::writeDescriptor(
  EspBleConnectionId connectionId,
  uint16_t descriptorHandle,
  const uint8_t *data,
  size_t length,
  bool response,
  uint32_t timeoutMilliseconds)
{
  if (descriptorHandle == 0)
  {
    setError(EspBleError::InvalidArgument, "descriptor handle must be non-zero");
    return false;
  }
  return startGattOperation(
    EspBleGattOperation::WriteDescriptor, connectionId, nullptr, nullptr,
    data, length, response, nullptr, timeoutMilliseconds, 0, descriptorHandle);
}

bool EspBle::writeDescriptor(
  EspBleConnectionId connectionId,
  uint16_t descriptorHandle,
  const String &value,
  bool response,
  uint32_t timeoutMilliseconds)
{
  return writeDescriptor(
    connectionId,
    descriptorHandle,
    reinterpret_cast<const uint8_t *>(value.c_str()),
    value.length(),
    response,
    timeoutMilliseconds);
}

void EspBle::onCharacteristicDiscovered(GattResultCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  characteristicDiscoveredListeners_.setPrimary(std::move(callback));
}

void EspBle::onCharacteristicRead(GattResultCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  characteristicReadListeners_.setPrimary(std::move(callback));
}

void EspBle::onCharacteristicWritten(GattResultCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  characteristicWrittenListeners_.setPrimary(std::move(callback));
}

void EspBle::onServicesDiscovered(GattResultCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  servicesDiscoveredListeners_.setPrimary(std::move(callback));
}

void EspBle::onDescriptorRead(GattResultCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  descriptorReadListeners_.setPrimary(std::move(callback));
}

void EspBle::onDescriptorWritten(GattResultCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  descriptorWrittenListeners_.setPrimary(std::move(callback));
}

void EspBle::onSubscribed(GattResultCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  subscribedListeners_.setPrimary(std::move(callback));
}

void EspBle::onUnsubscribed(GattResultCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  unsubscribedListeners_.setPrimary(std::move(callback));
}

void EspBle::onNotification(NotificationCallback callback)
{
  std::lock_guard<std::mutex> lock(listenerMutex_);
  notificationListeners_.setPrimary(std::move(callback));
}

EspBleListenerId EspBle::allocateListenerIdLocked()
{
  // Monotonic and owner-unique across every listener list on this object (GATT
  // client and connection events alike); wraps past ~4 billion adds, skipping
  // the invalid sentinel. Never reuses a live id in practice, so removal by id
  // is unambiguous.
  const EspBleListenerId id = nextListenerId_;
  nextListenerId_ = (id == 0xffffffffu) ? 1 : id + 1;
  return id;
}

// Allocate an id and store callback in `list`, reporting a full list. Serialized
// by listenerMutex_.
#define ESPBLE_ADD_GATT_LISTENER(list, cbType)                                   \
  do                                                                             \
  {                                                                              \
    std::lock_guard<std::mutex> lock(listenerMutex_);                        \
    const EspBleListenerId id = (list).add(std::move(callback),                  \
                                           allocateListenerIdLocked());      \
    if (id == EspBleInvalidListenerId)                                           \
    {                                                                            \
      setError(EspBleError::ResourceExhausted, "too many GATT client listeners");\
      return EspBleInvalidListenerId;                                            \
    }                                                                            \
    clearError();                                                                \
    return id;                                                                   \
  } while (0)

EspBleListenerId EspBle::addCharacteristicDiscoveredListener(GattResultCallback callback)
{ ESPBLE_ADD_GATT_LISTENER(characteristicDiscoveredListeners_, GattResultCallback); }
EspBleListenerId EspBle::addCharacteristicReadListener(GattResultCallback callback)
{ ESPBLE_ADD_GATT_LISTENER(characteristicReadListeners_, GattResultCallback); }
EspBleListenerId EspBle::addCharacteristicWrittenListener(GattResultCallback callback)
{ ESPBLE_ADD_GATT_LISTENER(characteristicWrittenListeners_, GattResultCallback); }
EspBleListenerId EspBle::addServicesDiscoveredListener(GattResultCallback callback)
{ ESPBLE_ADD_GATT_LISTENER(servicesDiscoveredListeners_, GattResultCallback); }
EspBleListenerId EspBle::addDescriptorReadListener(GattResultCallback callback)
{ ESPBLE_ADD_GATT_LISTENER(descriptorReadListeners_, GattResultCallback); }
EspBleListenerId EspBle::addDescriptorWrittenListener(GattResultCallback callback)
{ ESPBLE_ADD_GATT_LISTENER(descriptorWrittenListeners_, GattResultCallback); }
EspBleListenerId EspBle::addSubscribedListener(GattResultCallback callback)
{ ESPBLE_ADD_GATT_LISTENER(subscribedListeners_, GattResultCallback); }
EspBleListenerId EspBle::addUnsubscribedListener(GattResultCallback callback)
{ ESPBLE_ADD_GATT_LISTENER(unsubscribedListeners_, GattResultCallback); }
EspBleListenerId EspBle::addNotificationListener(NotificationCallback callback)
{ ESPBLE_ADD_GATT_LISTENER(notificationListeners_, NotificationCallback); }

#undef ESPBLE_ADD_GATT_LISTENER

// Same as ESPBLE_ADD_GATT_LISTENER for the connection-event lists; only the
// error detail differs, so a caller that overflows one can tell which.
#define ESPBLE_ADD_CONNECTION_LISTENER(list)                                    \
  do                                                                            \
  {                                                                             \
    std::lock_guard<std::mutex> lock(listenerMutex_);                           \
    const EspBleListenerId id = (list).add(std::move(callback),                 \
                                           allocateListenerIdLocked());         \
    if (id == EspBleInvalidListenerId)                                          \
    {                                                                           \
      setError(EspBleError::ResourceExhausted, "too many connection listeners");\
      return EspBleInvalidListenerId;                                           \
    }                                                                           \
    clearError();                                                               \
    return id;                                                                  \
  } while (0)

EspBleListenerId EspBle::addConnectedListener(ConnectionCallback callback)
{ ESPBLE_ADD_CONNECTION_LISTENER(connectedListeners_); }
EspBleListenerId EspBle::addDisconnectedListener(ConnectionCallback callback)
{ ESPBLE_ADD_CONNECTION_LISTENER(disconnectedListeners_); }
EspBleListenerId EspBle::addConnectionFailedListener(ConnectionFailureCallback callback)
{ ESPBLE_ADD_CONNECTION_LISTENER(connectionFailedListeners_); }
EspBleListenerId EspBle::addMtuChangedListener(MtuChangedCallback callback)
{ ESPBLE_ADD_CONNECTION_LISTENER(mtuChangedListeners_); }
EspBleListenerId EspBle::addConnectionParametersUpdatedListener(ConnectionCallback callback)
{ ESPBLE_ADD_CONNECTION_LISTENER(connectionParametersUpdatedListeners_); }
EspBleListenerId EspBle::addPhyUpdatedListener(ConnectionCallback callback)
{ ESPBLE_ADD_CONNECTION_LISTENER(phyUpdatedListeners_); }
EspBleListenerId EspBle::addSecurityChangedListener(SecurityChangedCallback callback)
{ ESPBLE_ADD_CONNECTION_LISTENER(securityChangedListeners_); }

#undef ESPBLE_ADD_CONNECTION_LISTENER

bool EspBle::removeConnectionListener(EspBleListenerId listenerId)
{
  if (listenerId == EspBleInvalidListenerId)
  {
    setError(EspBleError::InvalidArgument, "listener ID is invalid");
    return false;
  }
  std::lock_guard<std::mutex> lock(listenerMutex_);
  const bool removed =
    connectedListeners_.remove(listenerId) ||
    disconnectedListeners_.remove(listenerId) ||
    connectionFailedListeners_.remove(listenerId) ||
    mtuChangedListeners_.remove(listenerId) ||
    connectionParametersUpdatedListeners_.remove(listenerId) ||
    phyUpdatedListeners_.remove(listenerId) ||
    securityChangedListeners_.remove(listenerId);
  if (!removed)
  {
    setError(EspBleError::NotFound, "listener ID was not found");
    return false;
  }
  clearError();
  return true;
}

bool EspBle::removeGattListener(EspBleListenerId listenerId)
{
  if (listenerId == EspBleInvalidListenerId)
  {
    setError(EspBleError::InvalidArgument, "listener ID is invalid");
    return false;
  }
  std::lock_guard<std::mutex> lock(listenerMutex_);
  const bool removed =
    characteristicDiscoveredListeners_.remove(listenerId) ||
    characteristicReadListeners_.remove(listenerId) ||
    characteristicWrittenListeners_.remove(listenerId) ||
    servicesDiscoveredListeners_.remove(listenerId) ||
    descriptorReadListeners_.remove(listenerId) ||
    descriptorWrittenListeners_.remove(listenerId) ||
    subscribedListeners_.remove(listenerId) ||
    unsubscribedListeners_.remove(listenerId) ||
    notificationListeners_.remove(listenerId);
  if (!removed)
  {
    setError(EspBleError::NotFound, "listener ID was not found");
    return false;
  }
  clearError();
  return true;
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
  uint32_t timeoutMilliseconds,
  uint16_t characteristicHandle,
  uint16_t descriptorHandle)
{
  if (!initialized_)
  {
    setError(EspBleError::InvalidState, "BLE stack is not initialized");
    return false;
  }
  // DiscoverServices enumerates the whole database and HidDiscover discovers the
  // HID service on its own worker, so neither needs a service/characteristic UUID.
  const bool noTargetOperation = operation == EspBleGattOperation::DiscoverServices ||
    operation == EspBleGattOperation::HidDiscover;
  const bool descriptorOperation = operation == EspBleGattOperation::ReadDescriptor ||
    operation == EspBleGattOperation::WriteDescriptor;
  // A handle-based operation identifies the target by handle, so the
  // service/characteristic UUID arguments are not required. A descriptor handle
  // also supplies the descriptor UUID and the owning characteristic, so it
  // stands in for the whole UUID triple.
  const bool handleBased = characteristicHandle != 0 || descriptorHandle != 0;
  if ((!noTargetOperation && !handleBased &&
       (serviceUuid == nullptr || serviceUuid[0] == '\0' ||
        characteristicUuid == nullptr || characteristicUuid[0] == '\0')) ||
      (descriptorOperation && descriptorHandle == 0 &&
       (descriptorUuid == nullptr || descriptorUuid[0] == '\0')) ||
      (descriptorHandle != 0 && !descriptorOperation) ||
      (data == nullptr && length != 0) || timeoutMilliseconds == 0)
  {
    setError(EspBleError::InvalidArgument, "invalid GATT operation arguments");
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    bool centralConnectionFound = false;
    for (const EspBleImpl::ConnectionSlot &slot : impl_->connections)
    {
      if (slot.used && slot.connection.id == connectionId &&
          slot.connection.localRole == EspBleRole::Central)
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
    if (impl_->gattQueueCount == EspBleImpl::GattQueueCapacity)
    {
      setError(EspBleError::ResourceExhausted, "too many queued GATT operations");
      return false;
    }

    // Enqueue; the loop task pumps the queue once the ATT channel is free.
    const size_t tail =
      (impl_->gattQueueHead + impl_->gattQueueCount) % EspBleImpl::GattQueueCapacity;
    EspBleImpl::PendingGattOp &op = impl_->gattQueue[tail];
    op.operation = operation;
    op.connectionId = connectionId;
    op.serviceUuid = serviceUuid == nullptr ? "" : serviceUuid;
    op.characteristicUuid = characteristicUuid == nullptr ? "" : characteristicUuid;
    op.descriptorUuid = descriptorUuid == nullptr ? "" : descriptorUuid;
    op.characteristicHandle = characteristicHandle;
    op.descriptorHandle = descriptorHandle;
    op.writeValue = length == 0
      ? String()
      : String(reinterpret_cast<const char *>(data), length);
    op.response = response;
    op.timeoutMilliseconds = timeoutMilliseconds;
    ++impl_->gattQueueCount;
  }

  pumpGattQueue();
  clearError();
  return true;
}

bool EspBle::hasPendingHidDiscover(EspBleConnectionId connectionId) const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->gattOperating &&
      impl_->gattOperation == EspBleGattOperation::HidDiscover &&
      impl_->gattConnectionId == connectionId)
  {
    return true;
  }
  for (size_t i = 0; i < impl_->gattQueueCount; ++i)
  {
    const size_t idx = (impl_->gattQueueHead + i) % EspBleImpl::GattQueueCapacity;
    const EspBleImpl::PendingGattOp &op = impl_->gattQueue[idx];
    if (op.operation == EspBleGattOperation::HidDiscover && op.connectionId == connectionId)
    {
      return true;
    }
  }
  return false;
}

void EspBle::pumpGattQueue()
{
  if (impl_ == nullptr) return;

  EspBleGattOperation operation = EspBleGattOperation::Discover;
  EspBleConnectionId connectionId = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->gattOperating || impl_->gattQueueCount == 0) return;

    const EspBleImpl::PendingGattOp &op = impl_->gattQueue[impl_->gattQueueHead];
    operation = op.operation;
    connectionId = op.connectionId;
    impl_->gattOperation = op.operation;
    impl_->gattConnectionId = op.connectionId;
    impl_->gattServiceUuid = op.serviceUuid;
    impl_->gattCharacteristicUuid = op.characteristicUuid;
    impl_->gattDescriptorUuid = op.descriptorUuid;
    impl_->gattCharacteristicHandle = op.characteristicHandle;
    impl_->gattDescriptorHandle = op.descriptorHandle;
    impl_->gattWriteValue = op.writeValue;
    impl_->gattWriteResponse = op.response;
    impl_->gattStartMilliseconds = millis();
    impl_->gattTimeoutMilliseconds = op.timeoutMilliseconds;
    impl_->gattTimedOut = false;
    if (operation == EspBleGattOperation::DiscoverServices)
    {
      EspBleImpl::GattDatabaseSnapshot *database =
        impl_->findDatabaseLocked(connectionId);
      if (database != nullptr) database->reset(connectionId);
    }
    impl_->gattQueueHead = (impl_->gattQueueHead + 1) % EspBleImpl::GattQueueCapacity;
    --impl_->gattQueueCount;
    impl_->gattOperating = true;
  }

  // HID Host discovery runs on its own worker (a compound sequence of blocking
  // reads + subscribes), launched here so it shares the single ATT slot and
  // serializes with the generic operations. The worker clears gattOperating on
  // completion; the next update() pumps the rest of the queue.
  if (operation == EspBleGattOperation::HidDiscover)
  {
    if (!hidKeyboardHost_.runQueuedDiscovery(connectionId))
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      impl_->gattOperating = false;
    }
    return;
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
    // Could not start the worker: surface a failure result for this operation so
    // the caller's callback still fires, then let the next pump try the rest.
    EspBleImpl::Event event;
    event.type = EspBleImpl::EventType::GattResult;
    event.gattResult.operation = operation;
    event.gattResult.connectionId = connectionId;
    event.gattResult.error = EspBleError::ResourceExhausted;
    event.gattResult.detail = "failed to create GATT operation task";
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->gattOperating = false;
    impl_->pushEvent(event);
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->gattOperating)
  {
    impl_->gattTask = task;
  }
}

// Start the GATT server exactly once. ble_svc_gap_init() (run when the backend
// server object is created) resets the GAP device name to the sdkconfig default,
// and ble_gatts_start() is what commits the attribute table, so the name is
// restored afterwards.
bool EspBle::startGattServer()
{
  if (impl_ == nullptr)
  {
    setError(EspBleError::InvalidState, "connection state is unavailable");
    return false;
  }
  if (impl_->gattServerStarted) return true;
  const int backendCode = ble_gatts_start();
  if (backendCode != 0)
  {
    setError(
      EspBleError::BackendFailure,
      (String("failed to start the GATT server, backend code ") + backendCode).c_str());
    return false;
  }
  impl_->gattServerStarted = true;
  if (activeDeviceName_.length() != 0)
  {
    ble_svc_gap_device_name_set(activeDeviceName_.c_str());
  }
  return true;
}

// The first claim on the peripheral role starts the attribute table. Every
// GATT server exposes two services before its own: Generic Access (0x1800),
// which carries the device name and appearance, and Generic Attribute (0x1801),
// whose Service Changed characteristic tells a bonded peer that its cached copy
// of the table is stale. Both are mandatory, and a peer that discovers neither
// cannot subscribe to Service Changed at all.
bool EspBle::preparePeripheral()
{
  if (impl_ == nullptr)
  {
    setError(EspBleError::InvalidState, "connection state is unavailable");
    return false;
  }
  if (impl_->peripheralPrepared) return true;
  // Discards anything a previous session registered, so this table starts clean.
  ble_gatts_reset();
  ble_svc_gap_init();
  ble_svc_gatt_init();
  impl_->peripheralPrepared = true;
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
      {
        std::shared_ptr<ConnectionCallback> callbacks[decltype(connectedListeners_)::Capacity];
        size_t count = 0;
        {
          std::lock_guard<std::mutex> lock(listenerMutex_);
          count = connectedListeners_.snapshot(callbacks);
        }
        for (size_t i = 0; i < count; ++i) (*callbacks[i])(event.connection);
      }
      // Auto-restore any subscriptions recorded for this peer on a previous
      // connection. On the first connection to a peer the registry is empty, so
      // this is a no-op; on a reconnect the notifications resume without the
      // application re-subscribing. Enqueued after the user's callback so the
      // application's own operations keep their relative order.
      if (event.connection.localRole == EspBleRole::Central)
      {
        EspBleImpl::PersistentSubscription
          restore[EspBleImpl::PersistentSubscriptionCapacity];
        size_t restoreCount = 0;
        {
          std::lock_guard<std::mutex> lock(impl_->mutex);
          restoreCount = impl_->collectPersistentSubscriptionsLocked(
            event.connection.id, restore,
            EspBleImpl::PersistentSubscriptionCapacity);
        }
        for (size_t index = 0; index < restoreCount; ++index)
        {
          subscribe(
            event.connection.id,
            restore[index].serviceUuid.c_str(),
            restore[index].characteristicUuid.c_str(),
            restore[index].notifications);
        }
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
            // The LED state belonged to the host that just went away; keeping
            // it would report a stale Caps Lock to whoever connects next.
            hidKeyboardDevice_.impl_->ledState = EspBleHidKeyboardOutputReport();
          }
          hidKeyboardDevice_.nkroState_.clear();
        }
      }
      {
        std::shared_ptr<ConnectionCallback> callbacks[decltype(disconnectedListeners_)::Capacity];
        size_t count = 0;
        {
          std::lock_guard<std::mutex> lock(listenerMutex_);
          count = disconnectedListeners_.snapshot(callbacks);
        }
        for (size_t i = 0; i < count; ++i) (*callbacks[i])(event.connection);
      }
      break;
    case EspBleImpl::EventType::Failed:
      {
        std::shared_ptr<ConnectionFailureCallback> callbacks[decltype(connectionFailedListeners_)::Capacity];
        size_t count = 0;
        {
          std::lock_guard<std::mutex> lock(listenerMutex_);
          count = connectionFailedListeners_.snapshot(callbacks);
        }
        for (size_t i = 0; i < count; ++i) (*callbacks[i])(event.failure);
      }
      break;
    case EspBleImpl::EventType::GattResult:
    {
      EspBleCallbackList<GattResultCallback> *list = nullptr;
      switch (event.gattResult.operation)
      {
      case EspBleGattOperation::Discover:
        list = &characteristicDiscoveredListeners_;
        break;
      case EspBleGattOperation::Read:
        list = &characteristicReadListeners_;
        break;
      case EspBleGattOperation::Write:
        list = &characteristicWrittenListeners_;
        break;
      case EspBleGattOperation::Subscribe:
        list = &subscribedListeners_;
        break;
      case EspBleGattOperation::Unsubscribe:
        list = &unsubscribedListeners_;
        break;
      case EspBleGattOperation::DiscoverServices:
        list = &servicesDiscoveredListeners_;
        break;
      case EspBleGattOperation::ReadDescriptor:
        list = &descriptorReadListeners_;
        break;
      case EspBleGattOperation::WriteDescriptor:
        list = &descriptorWrittenListeners_;
        break;
      case EspBleGattOperation::HidDiscover:
        // HID discovery reports through its own discovery event, never a
        // generic GattResult; nothing to dispatch here.
        break;
      }
      if (list != nullptr)
      {
        std::shared_ptr<GattResultCallback> callbacks[decltype(characteristicDiscoveredListeners_)::Capacity];
        size_t count = 0;
        {
          std::lock_guard<std::mutex> lock(listenerMutex_);
          count = list->snapshot(callbacks);
        }
        for (size_t i = 0; i < count; ++i) (*callbacks[i])(event.gattResult);
      }
      break;
    }
    case EspBleImpl::EventType::ServerWrite:
      gattServer_.dispatchWrite(event.serverWrite);
      break;
    case EspBleImpl::EventType::ServerDescriptorWrite:
    {
      EspBleGattDescriptorWrite write;
      write.connectionId = event.serverWrite.connectionId;
      write.serviceUuid = event.serverWrite.serviceUuid;
      write.characteristicUuid = event.serverWrite.characteristicUuid;
      write.descriptor = event.serverDescriptor;
      write.descriptorUuid = event.serverDescriptorUuid;
      write.value = event.serverWrite.value;
      gattServer_.dispatchDescriptorWrite(write);
      break;
    }
    case EspBleImpl::EventType::Notification:
    {
      std::shared_ptr<NotificationCallback> callbacks[decltype(notificationListeners_)::Capacity];
      size_t count = 0;
      {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        count = notificationListeners_.snapshot(callbacks);
      }
      for (size_t i = 0; i < count; ++i) (*callbacks[i])(event.notification);
      break;
    }
    case EspBleImpl::EventType::ServerSubscription:
      gattServer_.dispatchSubscription(event.serverSubscription);
      break;
    case EspBleImpl::EventType::ServerSendResult:
      gattServer_.dispatchSendResult(event.serverSendResult);
      break;
    case EspBleImpl::EventType::MtuChanged:
      {
        std::shared_ptr<MtuChangedCallback> callbacks[decltype(mtuChangedListeners_)::Capacity];
        size_t count = 0;
        {
          std::lock_guard<std::mutex> lock(listenerMutex_);
          count = mtuChangedListeners_.snapshot(callbacks);
        }
        for (size_t i = 0; i < count; ++i) (*callbacks[i])(event.mtuChanged);
      }
      break;
    case EspBleImpl::EventType::ConnParamsUpdated:
      {
        std::shared_ptr<ConnectionCallback> callbacks[decltype(connectionParametersUpdatedListeners_)::Capacity];
        size_t count = 0;
        {
          std::lock_guard<std::mutex> lock(listenerMutex_);
          count = connectionParametersUpdatedListeners_.snapshot(callbacks);
        }
        for (size_t i = 0; i < count; ++i) (*callbacks[i])(event.connection);
      }
      break;
    case EspBleImpl::EventType::PhyUpdated:
      {
        std::shared_ptr<ConnectionCallback> callbacks[decltype(phyUpdatedListeners_)::Capacity];
        size_t count = 0;
        {
          std::lock_guard<std::mutex> lock(listenerMutex_);
          count = phyUpdatedListeners_.snapshot(callbacks);
        }
        for (size_t i = 0; i < count; ++i) (*callbacks[i])(event.connection);
      }
      break;
    case EspBleImpl::EventType::SecurityChanged:
      {
        std::shared_ptr<SecurityChangedCallback> callbacks[decltype(securityChangedListeners_)::Capacity];
        size_t count = 0;
        {
          std::lock_guard<std::mutex> lock(listenerMutex_);
          count = securityChangedListeners_.snapshot(callbacks);
        }
        for (size_t i = 0; i < count; ++i) (*callbacks[i])(event.securityChanged);
      }
      // After the app's handler (which may itself call discover()), give the HID
      // Host a chance to auto-rediscover a reconnected peer, de-duped against a
      // discovery the app just queued.
      hidKeyboardHost_.handleSecurityEstablished(event.securityChanged);
      break;
    case EspBleImpl::EventType::PasskeyDisplayed:
      if (passkeyDisplayedCallback_)
      {
        passkeyDisplayedCallback_(event.passkeyDisplayed);
      }
      break;
    case EspBleImpl::EventType::NumericComparison:
      if (numericComparisonCallback_)
      {
        numericComparisonCallback_(event.passkeyDisplayed);
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
EspBleHidCustom &EspBle::hidCustom() { return hidCustom_; }
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
