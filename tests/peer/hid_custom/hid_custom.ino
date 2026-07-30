// hid_custom DUT: EspBle generic GATT client that validates a Custom HID device
// built with an arbitrary Report Descriptor, AND the handle-based client
// operations. The device exposes three Report characteristics that share UUID
// 0x2A4D (a notifiable input, a writable output and a writable feature).
//
// Each report's role is read from its Report Reference descriptor (0x2908,
// report ID + type 1=Input / 2=Output / 3=Feature) — the way HID actually
// declares it. Every Report Reference is 0x2908 under a 0x2A4D characteristic,
// so naming one takes readDescriptor() BY HANDLE; the UUID triple cannot pick
// between three characteristics that share a UUID.
//
// The client then subscribes to the input by handle, decodes a custom 2-byte
// report, and writes the output and the feature by handle.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *HID_SERVICE_UUID = "1812";
static constexpr const char *REPORT_MAP_UUID = "2a4b";
static constexpr const char *REPORT_UUID = "2a4d";
static constexpr const char *REPORT_REFERENCE_UUID = "2908";

static constexpr uint8_t ReportTypeInput = 1;
static constexpr uint8_t ReportTypeOutput = 2;
static constexpr uint8_t ReportTypeFeature = 3;
static constexpr size_t MaxReports = 4;

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;
uint16_t inputHandle = 0;
uint16_t outputHandle = 0;
uint16_t featureHandle = 0;

// One entry per 0x2A4D characteristic, paired with its Report Reference.
struct ReportEntry
{
  uint16_t characteristicHandle = 0;
  uint16_t referenceHandle = 0;
  bool notifiable = false;
  bool writable = false;
  bool writableWithoutResponse = false;
};
ReportEntry reports[MaxReports];
size_t reportCount = 0;
size_t referencesRead = 0;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

// Discovered UUIDs come back in full 128-bit form (0000XXXX-...); match the
// 16-bit short UUID either way.
static bool uuidIs(const String &uuid, const char *shortUuid)
{
  String lower = uuid;
  lower.toLowerCase();
  String needle = shortUuid;
  needle.toLowerCase();
  return lower == needle || lower.indexOf(needle) == 4;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();
  if (!ble.begin())
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    ble.discoverServices(connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    inputHandle = 0;
    outputHandle = 0;
    featureHandle = 0;
    reportCount = 0;
    referencesRead = 0;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.onServicesDiscovered([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.println("DISCOVER_FAILED");
      return;
    }
    // Pair each 0x2A4D Report characteristic with its own Report Reference
    // descriptor. A descriptor is tied to one characteristic by the owning value
    // handle, never by the UUID pair: all three characteristics are 0x2A4D and
    // all three descriptors are 0x2908.
    reportCount = 0;
    const size_t count = ble.discoveredCharacteristicCount(result.connectionId, HID_SERVICE_UUID);
    for (size_t index = 0; index < count && reportCount < MaxReports; ++index)
    {
      EspBleGattCharacteristicInfo info;
      if (!ble.discoveredCharacteristic(result.connectionId, index, info, HID_SERVICE_UUID))
        continue;
      if (!uuidIs(info.characteristicUuid, REPORT_UUID)) continue;
      ReportEntry &entry = reports[reportCount];
      entry.characteristicHandle = info.handle;
      entry.notifiable = info.notifiable;
      entry.writable = info.writable;
      entry.writableWithoutResponse = info.writableWithoutResponse;
      entry.referenceHandle = 0;
      const size_t descriptorCount =
        ble.discoveredDescriptorCount(result.connectionId, HID_SERVICE_UUID);
      for (size_t d = 0; d < descriptorCount; ++d)
      {
        EspBleGattDescriptorInfo descriptor;
        if (!ble.discoveredDescriptor(result.connectionId, d, descriptor, HID_SERVICE_UUID))
          continue;
        if (descriptor.characteristicHandle != info.handle) continue;
        if (!uuidIs(descriptor.descriptorUuid, REPORT_REFERENCE_UUID)) continue;
        entry.referenceHandle = descriptor.handle;
        break;
      }
      ++reportCount;
    }
    Serial.printf("REPORTS_PAIRED count=%u\n", static_cast<unsigned>(reportCount));
    for (size_t index = 0; index < reportCount; ++index)
    {
      Serial.printf("REPORT_PAIR char=%u ref=%u notify=%u write=%u wwr=%u\n",
        reports[index].characteristicHandle,
        reports[index].referenceHandle,
        reports[index].notifiable ? 1 : 0,
        reports[index].writable ? 1 : 0,
        reports[index].writableWithoutResponse ? 1 : 0);
    }
  });
  ble.onDescriptorRead([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("REPORT_REF_FAILED desc=%u error=%u detail=%s\n",
        result.descriptorHandle, static_cast<unsigned>(result.error), result.detail.c_str());
      return;
    }
    if (result.value.length() < 2)
    {
      Serial.printf("REPORT_REF_SHORT desc=%u len=%u\n",
        result.descriptorHandle, result.value.length());
      return;
    }
    const uint8_t reportId = static_cast<uint8_t>(result.value[0]);
    const uint8_t reportType = static_cast<uint8_t>(result.value[1]);
    // `handle` reports the characteristic that owns the descriptor, so the role
    // read out of the descriptor lands on the right Report characteristic even
    // though all three share UUID 0x2A4D.
    if (reportType == ReportTypeInput) inputHandle = result.handle;
    else if (reportType == ReportTypeOutput) outputHandle = result.handle;
    else if (reportType == ReportTypeFeature) featureHandle = result.handle;
    Serial.printf("REPORT_REF desc=%u char=%u id=%u type=%u context=%s\n",
      result.descriptorHandle, result.handle, reportId, reportType, contextName());
    if (++referencesRead < reportCount) return;
    Serial.printf("REPORTS_RESOLVED input=%u output=%u feature=%u distinct=%u\n",
      inputHandle, outputHandle, featureHandle,
      (inputHandle != 0 && outputHandle != 0 && featureHandle != 0 &&
       inputHandle != outputHandle && outputHandle != featureHandle &&
       inputHandle != featureHandle) ? 1 : 0);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(REPORT_MAP_UUID)) return;
    Serial.printf("REPORT_MAP success=%u length=%u\n",
      result.success ? 1 : 0, result.value.length());
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    // The result handle says which of the two writable reports this was.
    Serial.printf("%s success=%u handle=%u context=%s\n",
      result.handle == featureHandle ? "FEATURE_WRITTEN" : "OUTPUT_WRITTEN",
      result.success ? 1 : 0, result.handle, contextName());
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("INPUT_SUBSCRIBED success=%u handle=%u context=%s\n",
      result.success ? 1 : 0, result.handle, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.handle != inputHandle || notification.value.length() != 2) return;
    Serial.printf("INPUT_REPORT handle=%u delta=%d buttons=%u context=%s\n",
      notification.handle,
      static_cast<int8_t>(notification.value[0]),
      static_cast<uint8_t>(notification.value[1]), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(HID_SERVICE_UUID)) return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
    Serial.println(connectionRequested ? "CONNECT_REQUESTED" : "CONNECT_REQUEST_FAILED");
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's' && !connectionRequested)
    {
      EspBleScanConfig scan;
      scan.active = true;
      Serial.println(ble.scanner().start(scan) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'm' && connectionId != 0)
    {
      Serial.println(ble.readCharacteristic(connectionId, HID_SERVICE_UUID, REPORT_MAP_UUID)
        ? "READ_REQUESTED" : "READ_REQUEST_FAILED");
    }
    else if (command == 'p' && reportCount != 0)
    {
      // Read every Report Reference BY HANDLE. Three fit in the GATT queue
      // (8 beside the one in flight), and it is FIFO, so the results arrive in
      // the order issued.
      referencesRead = 0;
      size_t requested = 0;
      for (size_t index = 0; index < reportCount; ++index)
      {
        if (reports[index].referenceHandle == 0) continue;
        if (ble.readDescriptor(connectionId, reports[index].referenceHandle)) ++requested;
      }
      Serial.printf("REPORT_REF_REQUESTED count=%u\n", static_cast<unsigned>(requested));
    }
    else if (command == 'z')
    {
      // Rejected locally: a zero handle names nothing.
      Serial.printf("REF_ZERO success=%u error=%s\n",
        ble.readDescriptor(connectionId, 0) ? 1 : 0, ble.lastErrorName());
    }
    else if (command == 'Z' && connectionId != 0)
    {
      // Accepted locally, then reported NotFound by the operation: the handle is
      // well-formed but absent from the discovery snapshot.
      Serial.printf("REF_BOGUS success=%u\n",
        ble.readDescriptor(connectionId, 0xfff0) ? 1 : 0);
    }
    else if (command == 'S' && inputHandle != 0)
    {
      // Subscribe to the input report by handle (not UUID).
      Serial.println(ble.subscribe(connectionId, inputHandle, true)
        ? "SUBSCRIBE_REQUESTED" : "SUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'o' && outputHandle != 0)
    {
      // Write the output report by handle: the other 0x2A4D characteristic.
      const uint8_t leds = 0x02;
      Serial.println(ble.writeCharacteristic(connectionId, outputHandle, &leds, sizeof(leds), true)
        ? "OUTPUT_WRITE_REQUESTED" : "OUTPUT_WRITE_REQUEST_FAILED");
    }
    else if (command == 'f' && featureHandle != 0)
    {
      // Write the feature report by handle: two bytes of configuration.
      const uint8_t configuration[2] = {0x5a, 0xa5};
      Serial.println(ble.writeCharacteristic(
        connectionId, featureHandle, configuration, sizeof(configuration), true)
        ? "FEATURE_WRITE_REQUESTED" : "FEATURE_WRITE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
