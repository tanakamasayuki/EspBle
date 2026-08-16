#include "EspBleClassic.h"
#include "EspBleClassicBuild.h"
#include "EspBleClassicVisibility.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <utility>

#if defined(CONFIG_IDF_TARGET_ESP32) && \
  defined(ESPBLE_CLASSIC_CUSTOM_HOST) && \
  (defined(ESPBLE_CLASSIC_ONLY) || defined(ESPBLE_ENABLE_CLASSIC))
#define ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE 1
#define esp_bt_gap_set_scan_mode espble_bd_esp_bt_gap_set_scan_mode
#define esp_bt_hid_device_deinit espble_bd_esp_bt_hid_device_deinit
#define esp_bt_hid_device_disconnect espble_bd_esp_bt_hid_device_disconnect
#define esp_bt_hid_device_init espble_bd_esp_bt_hid_device_init
#define esp_bt_hid_device_register_app \
  espble_bd_esp_bt_hid_device_register_app
#define esp_bt_hid_device_register_callback \
  espble_bd_esp_bt_hid_device_register_callback
#define esp_bt_hid_device_send_report \
  espble_bd_esp_bt_hid_device_send_report
#define esp_bt_hid_device_unregister_app \
  espble_bd_esp_bt_hid_device_unregister_app
#define esp_bt_hid_host_connect espble_bd_esp_bt_hid_host_connect
#define esp_bt_hid_host_deinit espble_bd_esp_bt_hid_host_deinit
#define esp_bt_hid_host_disconnect espble_bd_esp_bt_hid_host_disconnect
#define esp_bt_hid_host_init espble_bd_esp_bt_hid_host_init
#define esp_bt_hid_host_register_callback \
  espble_bd_esp_bt_hid_host_register_callback
#define esp_bt_hid_host_set_report espble_bd_esp_bt_hid_host_set_report
#define esp_bt_hid_host_get_report espble_bd_esp_bt_hid_host_get_report
#define esp_bt_hid_host_get_protocol espble_bd_esp_bt_hid_host_get_protocol
#define esp_bt_hid_host_set_protocol espble_bd_esp_bt_hid_host_set_protocol
#define esp_bt_hid_host_get_idle espble_bd_esp_bt_hid_host_get_idle
#define esp_bt_hid_host_set_idle espble_bd_esp_bt_hid_host_set_idle
#define esp_bt_hid_host_virtual_cable_unplug \
  espble_bd_esp_bt_hid_host_virtual_cable_unplug
#define esp_bt_hid_device_report_error \
  espble_bd_esp_bt_hid_device_report_error
#define esp_bt_hid_device_virtual_cable_unplug \
  espble_bd_esp_bt_hid_device_virtual_cable_unplug
#include "esp32/include/esp_gap_bt_api.h"
#include "esp32/include/esp_hidd_api.h"
#include "esp32/include/esp_hidh_api.h"
#else
#define ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE 0
#endif

namespace
{
constexpr size_t HidEventQueueCapacity = 12;
constexpr size_t MaximumHidReportLength =
  EspBleClassicHidDevice::MaximumReportLength;

// Report Descriptor plus the device strings, as they share the SDP record's pad.
// See the check in EspBleClassicHidDevice::begin() for where this comes from.
constexpr size_t MaximumSdpRecordPayload = 214;

#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
String hidAddress(const esp_bd_addr_t address)
{
  char value[18];
  snprintf(
    value, sizeof(value), "%02x:%02x:%02x:%02x:%02x:%02x",
    address[0], address[1], address[2], address[3], address[4], address[5]);
  return String(value);
}

EspBleClassicHidReportType hidReportType(esp_hidd_report_type_t type)
{
  switch (type)
  {
    case ESP_HIDD_REPORT_TYPE_OUTPUT: return EspBleClassicHidReportType::Output;
    case ESP_HIDD_REPORT_TYPE_FEATURE:
      return EspBleClassicHidReportType::Feature;
    default: break;
  }
  return EspBleClassicHidReportType::Input;
}

bool hidParseAddress(const char *value, esp_bd_addr_t address)
{
  if (value == nullptr) return false;
  unsigned bytes[ESP_BD_ADDR_LEN] = {};
  char trailing = '\0';
  if (
    sscanf(
      value, "%02x:%02x:%02x:%02x:%02x:%02x%c",
      &bytes[0], &bytes[1], &bytes[2],
      &bytes[3], &bytes[4], &bytes[5], &trailing) != ESP_BD_ADDR_LEN)
  {
    return false;
  }
  for (size_t index = 0; index < ESP_BD_ADDR_LEN; ++index)
  {
    if (bytes[index] > UINT8_MAX) return false;
    address[index] = static_cast<uint8_t>(bytes[index]);
  }
  return true;
}
#endif
} // namespace

struct EspBleClassicHidDeviceImpl
{
  enum class EventType : uint8_t
  {
    Connected,
    Disconnected,
    OutputReport,
    SetReport,
    ReportRequested,
    ProtocolMode,
  };
  struct Event
  {
    EventType type = EventType::Connected;
    EspBleClassicHidConnection connection;
    EspBleClassicHidReport report;
    EspBleClassicHidReportRequest request;
    EspBleClassicHidProtocolMode protocolMode =
      EspBleClassicHidProtocolMode::Report;
  };

  bool enqueue(Event event)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (eventCount == HidEventQueueCapacity)
    {
      ++droppedEvents;
      return false;
    }
    const size_t tail = (eventHead + eventCount) % HidEventQueueCapacity;
    events[tail] = std::move(event);
    ++eventCount;
    return true;
  }

  mutable std::mutex mutex;
  std::atomic<size_t> callbackUsers{0};
  Event events[HidEventQueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  bool initializationCompleted = false;
  bool initialized = false;
  bool registrationCompleted = false;
  bool registered = false;
  bool ending = false;
  bool connected = false;
  String peerAddress;
  String name;
  String description;
  String provider;
  uint8_t *descriptor = nullptr;
  size_t descriptorLength = 0;
  // The Host owns the protocol mode; the device only observes it.
  EspBleClassicHidProtocolMode protocolMode =
    EspBleClassicHidProtocolMode::Report;
  // The request being answered, so a sketch can reply with the type and report
  // ID the Host asked for without tracking them itself.
  bool reportRequestPending = false;
  EspBleClassicHidReportRequest reportRequest;
  // A Set_Report on the control channel has to be acknowledged with a HID
  // handshake, or the Host waits and never learns the value arrived. Reports on
  // the interrupt channel are not acknowledged, so this only covers Set_Report.
  bool setReportPending = false;
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  esp_hidd_app_param_t application = {};
  esp_hidd_qos_param_t inputQos = {};
  esp_hidd_qos_param_t outputQos = {};
#endif
};

struct EspBleClassicHidHostImpl
{
  enum class EventType : uint8_t
  {
    Connected,
    Disconnected,
    ConnectionFailed,
    InputReport,
    ReportResult,
    ReportSent,
    ProtocolMode,
    IdleRate,
  };
  struct Event
  {
    EventType type = EventType::Connected;
    EspBleClassicHidConnection connection;
    EspBleClassicHidConnectionFailure failure;
    EspBleClassicHidReport report;
    EspBleClassicHidHost::ReportResult reportResult;
    EspBleClassicHidHost::ProtocolModeResult protocolModeResult;
    EspBleClassicHidHost::IdleRateResult idleRateResult;
  };

  bool enqueue(Event event)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (eventCount == HidEventQueueCapacity)
    {
      ++droppedEvents;
      return false;
    }
    const size_t tail = (eventHead + eventCount) % HidEventQueueCapacity;
    events[tail] = std::move(event);
    ++eventCount;
    return true;
  }

  mutable std::mutex mutex;
  std::atomic<size_t> callbackUsers{0};
  Event events[HidEventQueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  bool initializationCompleted = false;
  bool initialized = false;
  bool ending = false;
  bool connecting = false;
  bool connected = false;
  uint8_t handle = 0;
  // The Host receives the peer's Report Descriptor over SDP, which is the same
  // information the BLE side reads from the Report Map characteristic. Parsing
  // it is what turns raw reports into keyboard and mouse events.
  EspBleHidReportMapInfo reportMap;
  bool reportMapValid = false;
  size_t invalidInputReports = 0;
  // Previous keyboard usages, so a report can be turned into press and release
  // events rather than a snapshot the sketch has to diff itself.
  uint8_t previousKeys[EspBleClassicHidKeyboardState::BitmapSize] = {};
  uint8_t previousModifiers = 0;
  uint8_t previousMouseButtons = 0;
  // Set_Protocol and Set_Idle acknowledge without echoing the value, so the
  // request is remembered to report what is now in effect.
  EspBleClassicHidProtocolMode requestedProtocolMode =
    EspBleClassicHidProtocolMode::Report;
  uint8_t requestedIdleRate = 0;
  String peerAddress;
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  esp_bd_addr_t backendAddress = {};
#else
  uint8_t backendAddress[6] = {};
#endif
};

#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
namespace
{
std::atomic<EspBleClassicHidDeviceImpl *> activeHidDevice{nullptr};
std::atomic<EspBleClassicHidHostImpl *> activeHidHost{nullptr};
std::mutex hidDeviceCallbackTargetMutex;
std::mutex hidHostCallbackTargetMutex;

template <typename T>
class CallbackLease
{
public:
  CallbackLease(std::atomic<T *> &active, std::mutex &targetMutex)
  {
    std::lock_guard<std::mutex> lock(targetMutex);
    impl_ = active.load(std::memory_order_relaxed);
    if (impl_ != nullptr)
      impl_->callbackUsers.fetch_add(1, std::memory_order_acq_rel);
  }

  ~CallbackLease()
  {
    if (impl_ != nullptr)
      impl_->callbackUsers.fetch_sub(1, std::memory_order_acq_rel);
  }

  T *get() const { return impl_; }

private:
  T *impl_ = nullptr;
};

template <typename T>
bool activateCallbackTarget(
  std::atomic<T *> &active, std::mutex &targetMutex, T *impl)
{
  std::lock_guard<std::mutex> lock(targetMutex);
  T *current = active.load(std::memory_order_relaxed);
  if (current != nullptr && current != impl) return false;
  active.store(impl, std::memory_order_release);
  return true;
}

template <typename T>
void deactivateCallbackTarget(
  std::atomic<T *> &active, std::mutex &targetMutex, T *impl)
{
  {
    std::lock_guard<std::mutex> lock(targetMutex);
    if (active.load(std::memory_order_relaxed) == impl)
      active.store(nullptr, std::memory_order_release);
  }
  while (impl->callbackUsers.load(std::memory_order_acquire) != 0) delay(1);
}

bool failHidHostConnection(
  EspBleClassicHidHostImpl *impl, EspBleError error, const char *detail)
{
  EspBleClassicHidConnectionFailure failure;
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->connecting) return false;
    failure.peerAddress = impl->peerAddress;
    failure.error = error;
    failure.detail = detail == nullptr ? "" : detail;
    impl->connecting = false;
    impl->peerAddress = "";
    memset(impl->backendAddress, 0, sizeof(impl->backendAddress));
  }
  EspBleClassicHidHostImpl::Event queued;
  queued.type = EspBleClassicHidHostImpl::EventType::ConnectionFailed;
  queued.failure = std::move(failure);
  impl->enqueue(std::move(queued));
  return true;
}

void hidDeviceCallback(
  esp_hidd_cb_event_t event, esp_hidd_cb_param_t *parameter)
{
  CallbackLease<EspBleClassicHidDeviceImpl> lease(
    activeHidDevice, hidDeviceCallbackTargetMutex);
  EspBleClassicHidDeviceImpl *impl = lease.get();
  if (impl == nullptr || parameter == nullptr) return;

  if (event == ESP_HIDD_INIT_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->initialized = parameter->init.status == ESP_HIDD_SUCCESS;
    impl->initializationCompleted = true;
  }
  else if (event == ESP_HIDD_DEINIT_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->initialized = false;
  }
  else if (event == ESP_HIDD_REGISTER_APP_EVT)
  {
    // The registration writes a Class of Device derived from the HID service,
    // so whatever the sketch asked for is re-asserted afterwards.
    if (parameter->register_app.status == ESP_HIDD_SUCCESS)
      (void)EspBleClassicVisibilityOwner::apply();
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->registered = parameter->register_app.status == ESP_HIDD_SUCCESS;
    impl->registrationCompleted = true;
  }
  else if (event == ESP_HIDD_UNREGISTER_APP_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->registered = false;
  }
  else if (event == ESP_HIDD_OPEN_EVT)
  {
    EspBleClassicHidDeviceImpl::Event queued;
    queued.type = EspBleClassicHidDeviceImpl::EventType::Connected;
    queued.connection.peerAddress = hidAddress(parameter->open.bd_addr);
    queued.connection.incoming = true;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->connected =
        !impl->ending &&
        parameter->open.status == ESP_HIDD_SUCCESS &&
        parameter->open.conn_status == ESP_HIDD_CONN_STATE_CONNECTED;
      if (!impl->connected) return;
      impl->peerAddress = queued.connection.peerAddress;
    }
    impl->enqueue(std::move(queued));
  }
  else if (event == ESP_HIDD_CLOSE_EVT)
  {
    EspBleClassicHidDeviceImpl::Event queued;
    queued.type = EspBleClassicHidDeviceImpl::EventType::Disconnected;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (!impl->connected) return;
      queued.connection.peerAddress = impl->peerAddress;
      queued.connection.incoming = true;
      impl->connected = false;
      impl->peerAddress = "";
    }
    impl->enqueue(std::move(queued));
  }
  else if (
    event == ESP_HIDD_INTR_DATA_EVT || event == ESP_HIDD_SET_REPORT_EVT)
  {
    const bool interrupt = event == ESP_HIDD_INTR_DATA_EVT;
    const uint8_t *data = interrupt ?
      parameter->intr_data.data : parameter->set_report.data;
    size_t length = interrupt ?
      parameter->intr_data.len : parameter->set_report.len;
    if (data == nullptr || length > MaximumHidReportLength) return;
    EspBleClassicHidDeviceImpl::Event queued;
    // A Set_Report keeps the type the Host used, so a Feature report is not
    // reported as an Output report. Data on the interrupt channel has no type
    // of its own and is an Output report by definition.
    queued.type = interrupt
      ? EspBleClassicHidDeviceImpl::EventType::OutputReport
      : EspBleClassicHidDeviceImpl::EventType::SetReport;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      queued.report.peerAddress = impl->peerAddress;
    }
    queued.report.type = interrupt
      ? EspBleClassicHidReportType::Output
      : hidReportType(parameter->set_report.report_type);
    queued.report.reportId = interrupt ?
      parameter->intr_data.report_id : parameter->set_report.report_id;
    // The two channels do not agree on where the report ID lives: Set_Report
    // hands it over separately, while a report on the interrupt channel carries
    // it in front of the payload. reportId is the authority either way, so a
    // leading copy of it is removed and value is the payload alone — otherwise
    // a sketch would have to know which channel delivered the report.
    if (
      length > 1 && queued.report.reportId != 0 &&
      data[0] == queued.report.reportId)
    {
      ++data;
      --length;
    }
    queued.report.value = String(
      reinterpret_cast<const char *>(data), length);
    if (!interrupt)
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->setReportPending = true;
    }
    impl->enqueue(std::move(queued));
  }
  else if (event == ESP_HIDD_GET_REPORT_EVT)
  {
    EspBleClassicHidDeviceImpl::Event queued;
    queued.type = EspBleClassicHidDeviceImpl::EventType::ReportRequested;
    queued.request.type = hidReportType(parameter->get_report.report_type);
    queued.request.reportId = parameter->get_report.report_id;
    queued.request.maximumLength = parameter->get_report.buffer_size;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      queued.request.peerAddress = impl->peerAddress;
      // Held so a sketch can answer without repeating the type and report ID,
      // and so an answer sent for no request can be refused.
      impl->reportRequest = queued.request;
      impl->reportRequestPending = true;
    }
    impl->enqueue(std::move(queued));
  }
  else if (event == ESP_HIDD_SET_PROTOCOL_EVT)
  {
    EspBleClassicHidDeviceImpl::Event queued;
    queued.type = EspBleClassicHidDeviceImpl::EventType::ProtocolMode;
    queued.protocolMode =
      parameter->set_protocol.protocol_mode == ESP_HIDD_BOOT_MODE
        ? EspBleClassicHidProtocolMode::Boot
        : EspBleClassicHidProtocolMode::Report;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->protocolMode = queued.protocolMode;
    }
    impl->enqueue(std::move(queued));
  }
}

void hidHostCallback(
  esp_hidh_cb_event_t event, esp_hidh_cb_param_t *parameter)
{
  CallbackLease<EspBleClassicHidHostImpl> lease(
    activeHidHost, hidHostCallbackTargetMutex);
  EspBleClassicHidHostImpl *impl = lease.get();
  if (impl == nullptr || parameter == nullptr) return;

  if (event == ESP_HIDH_INIT_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->initialized = parameter->init.status == ESP_HIDH_OK;
    impl->initializationCompleted = true;
  }
  else if (event == ESP_HIDH_DEINIT_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->initialized = false;
  }
  else if (event == ESP_HIDH_GET_DSCP_EVT)
  {
    // The peer's Report Descriptor. Parsing it here means the decode path is
    // ready before the first input report arrives.
    if (parameter->dscp.dsc_list != nullptr && parameter->dscp.dl_len > 0)
    {
      const EspBleHidReportMapInfo parsed = espBleParseHidReportMap(
        parameter->dscp.dsc_list, parameter->dscp.dl_len);
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->reportMap = parsed;
      impl->reportMapValid = parsed.count > 0;
      memset(impl->previousKeys, 0, sizeof(impl->previousKeys));
      impl->previousModifiers = 0;
    }
  }
  else if (event == ESP_HIDH_OPEN_EVT)
  {
    // Bluedroid reports the accepted asynchronous request first, followed by a
    // second OPEN event when paging either connects or fails.
    if (parameter->open.status == ESP_HIDH_OK &&
        parameter->open.conn_status == ESP_HIDH_CONN_STATE_CONNECTING)
      return;
    const bool succeeded =
      parameter->open.status == ESP_HIDH_OK &&
      parameter->open.conn_status == ESP_HIDH_CONN_STATE_CONNECTED;
    if (!succeeded)
    {
      char detail[96];
      snprintf(
        detail, sizeof(detail),
        "Classic HID connection failed (status %u, state %u)",
        static_cast<unsigned>(parameter->open.status),
        static_cast<unsigned>(parameter->open.conn_status));
      (void)failHidHostConnection(
        impl, EspBleError::BackendFailure, detail);
      return;
    }
    EspBleClassicHidHostImpl::Event queued;
    queued.type = EspBleClassicHidHostImpl::EventType::Connected;
    queued.connection.peerAddress = hidAddress(parameter->open.bd_addr);
    queued.connection.incoming = !parameter->open.is_orig;
    bool acceptConnection = false;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      acceptConnection = !impl->ending &&
        (!parameter->open.is_orig || impl->connecting);
      if (acceptConnection)
      {
        impl->connecting = false;
        impl->connected = true;
        impl->handle = parameter->open.handle;
        impl->peerAddress = queued.connection.peerAddress;
        memcpy(
          impl->backendAddress, parameter->open.bd_addr,
          sizeof(impl->backendAddress));
      }
    }
    // An outgoing OPEN must match an active request. Incoming connections do
    // not use the connecting flag.
    if (!acceptConnection)
    {
      (void)esp_bt_hid_host_disconnect(parameter->open.bd_addr);
      return;
    }
    impl->enqueue(std::move(queued));
  }
  else if (event == ESP_HIDH_CLOSE_EVT)
  {
    bool failedWhileConnecting = false;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      failedWhileConnecting = impl->connecting && !impl->connected;
    }
    if (failedWhileConnecting)
    {
      (void)failHidHostConnection(
        impl, EspBleError::BackendFailure,
        "Classic HID connection closed before opening");
      return;
    }
    EspBleClassicHidHostImpl::Event queued;
    queued.type = EspBleClassicHidHostImpl::EventType::Disconnected;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (!impl->connected && !impl->connecting) return;
      queued.connection.peerAddress = impl->peerAddress;
      queued.connection.incoming = false;
      impl->connecting = false;
      impl->connected = false;
      impl->handle = 0;
      impl->peerAddress = "";
      memset(impl->backendAddress, 0, sizeof(impl->backendAddress));
    }
    impl->enqueue(std::move(queued));
  }
  else if (
    event == ESP_HIDH_DATA_IND_EVT &&
    parameter->data_ind.status == ESP_HIDH_OK)
  {
    if (
      parameter->data_ind.data == nullptr ||
      parameter->data_ind.len > MaximumHidReportLength)
    {
      return;
    }
    EspBleClassicHidHostImpl::Event queued;
    queued.type = EspBleClassicHidHostImpl::EventType::InputReport;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      queued.report.peerAddress = impl->peerAddress;
    }
    queued.report.type = EspBleClassicHidReportType::Input;
    queued.report.reportId = parameter->data_ind.len == 0 ? 0 :
      parameter->data_ind.data[0];
    queued.report.value = String(
      reinterpret_cast<const char *>(parameter->data_ind.data),
      parameter->data_ind.len);
    impl->enqueue(std::move(queued));
  }
  else if (event == ESP_HIDH_GET_RPT_EVT)
  {
    EspBleClassicHidHostImpl::Event queued;
    queued.type = EspBleClassicHidHostImpl::EventType::ReportResult;
    queued.reportResult.success = parameter->get_rpt.status == ESP_HIDH_OK;
    if (
      queued.reportResult.success && parameter->get_rpt.data != nullptr &&
      parameter->get_rpt.len <= MaximumHidReportLength)
    {
      queued.reportResult.value = String(
        reinterpret_cast<const char *>(parameter->get_rpt.data),
        parameter->get_rpt.len);
    }
    impl->enqueue(std::move(queued));
  }
  else if (event == ESP_HIDH_SET_RPT_EVT)
  {
    EspBleClassicHidHostImpl::Event queued;
    queued.type = EspBleClassicHidHostImpl::EventType::ReportSent;
    queued.reportResult.success = parameter->set_rpt.status == ESP_HIDH_OK;
    impl->enqueue(std::move(queued));
  }
  else if (event == ESP_HIDH_GET_PROTO_EVT || event == ESP_HIDH_SET_PROTO_EVT)
  {
    EspBleClassicHidHostImpl::Event queued;
    queued.type = EspBleClassicHidHostImpl::EventType::ProtocolMode;
    // Set_Protocol reports no mode of its own, so the mode the sketch asked for
    // is the one in effect when it succeeded.
    if (event == ESP_HIDH_GET_PROTO_EVT)
    {
      queued.protocolModeResult.success =
        parameter->get_proto.status == ESP_HIDH_OK;
      queued.protocolModeResult.mode =
        parameter->get_proto.proto_mode == ESP_HIDH_BOOT_MODE
          ? EspBleClassicHidProtocolMode::Boot
          : EspBleClassicHidProtocolMode::Report;
    }
    else
    {
      queued.protocolModeResult.success =
        parameter->set_proto.status == ESP_HIDH_OK;
      std::lock_guard<std::mutex> lock(impl->mutex);
      queued.protocolModeResult.mode = impl->requestedProtocolMode;
    }
    impl->enqueue(std::move(queued));
  }
  else if (event == ESP_HIDH_GET_IDLE_EVT || event == ESP_HIDH_SET_IDLE_EVT)
  {
    EspBleClassicHidHostImpl::Event queued;
    queued.type = EspBleClassicHidHostImpl::EventType::IdleRate;
    if (event == ESP_HIDH_GET_IDLE_EVT)
    {
      queued.idleRateResult.success =
        parameter->get_idle.status == ESP_HIDH_OK;
      queued.idleRateResult.idleRate = parameter->get_idle.idle_rate;
    }
    else
    {
      queued.idleRateResult.success =
        parameter->set_idle.status == ESP_HIDH_OK;
      std::lock_guard<std::mutex> lock(impl->mutex);
      queued.idleRateResult.idleRate = impl->requestedIdleRate;
    }
    impl->enqueue(std::move(queued));
  }
}

bool waitForFlag(
  const std::function<bool()> &condition, uint32_t timeoutMilliseconds = 3000)
{
  const uint32_t deadline = millis() + timeoutMilliseconds;
  while (!condition())
  {
    if (static_cast<int32_t>(millis() - deadline) >= 0) return false;
    delay(1);
  }
  return true;
}
} // namespace
#endif

EspBleClassicHidDevice::EspBleClassicHidDevice(EspBleClassic *owner) :
  owner_(owner) {}

EspBleClassicHidDevice::~EspBleClassicHidDevice()
{
  end();
  if (impl_ != nullptr) delete[] impl_->descriptor;
  delete impl_;
}

bool EspBleClassicHidDevice::begin(
  const EspBleClassicHidDeviceConfig &config)
{
#if !ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  (void)config;
  owner_->setError(
    EspBleError::BackendFailure,
    "Classic HID requires the custom ESP32 Classic host build");
  return false;
#else
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "Classic stack is not initialized");
    return false;
  }
  if (
    config.name == nullptr || config.description == nullptr ||
    config.provider == nullptr || config.reportDescriptor == nullptr ||
    config.reportDescriptorLength == 0 ||
    config.reportDescriptorLength > ESP_HIDD_APP_DESC_LIST_LEN_MAX)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid Classic HID descriptor");
    return false;
  }
  if (initialized())
  {
    owner_->clearError();
    return true;
  }
  if (impl_ == nullptr)
  {
    impl_ = new (std::nothrow) EspBleClassicHidDeviceImpl();
    if (impl_ == nullptr)
    {
      owner_->setError(EspBleError::ResourceExhausted, "failed to allocate HID device state");
      return false;
    }
  }
  delete[] impl_->descriptor;
  impl_->descriptor = new (std::nothrow) uint8_t[config.reportDescriptorLength];
  if (impl_->descriptor == nullptr)
  {
    owner_->setError(EspBleError::ResourceExhausted, "failed to copy HID descriptor");
    return false;
  }
  memcpy(
    impl_->descriptor, config.reportDescriptor,
    config.reportDescriptorLength);
  impl_->descriptorLength = config.reportDescriptorLength;
  impl_->name = config.name;
  impl_->description = config.description;
  impl_->provider = config.provider;
  impl_->application.name = impl_->name.c_str();
  impl_->application.description = impl_->description.c_str();
  impl_->application.provider = impl_->provider.c_str();
  impl_->application.subclass = config.subclass;
  impl_->application.desc_list = impl_->descriptor;
  impl_->application.desc_list_len = impl_->descriptorLength;
  memset(&impl_->inputQos, 0, sizeof(impl_->inputQos));
  memset(&impl_->outputQos, 0, sizeof(impl_->outputQos));
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = false;
  }

  if (!activateCallbackTarget(
        activeHidDevice, hidDeviceCallbackTargetMutex, impl_))
  {
    owner_->setError(
      EspBleError::InvalidState, "another HID device profile is active");
    return false;
  }
  if (
    esp_bt_hid_device_register_callback(hidDeviceCallback) != ESP_OK ||
    esp_bt_hid_device_init() != ESP_OK ||
    !waitForFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return impl_->initializationCompleted;
    }) || !impl_->initialized)
  {
    deactivateCallbackTarget(
      activeHidDevice, hidDeviceCallbackTargetMutex, impl_);
    owner_->setError(EspBleError::BackendFailure, "failed to initialize HID device profile");
    return false;
  }
  // The backend does not report this failure: when the SDP record does not fit,
  // HID_DevAddRecord logs it, register_app still reports success, and the device
  // ends up registered with no record — so no Host can find or connect to it.
  // Checked here instead, because a silent "started" is worse than a refusal.
  //
  // The record holds the Report Descriptor and the three strings inside a
  // 300-byte pad (CONFIG_BT_SDP_PAD_LEN) shared with every other attribute,
  // each of which consumes its encoded length. Adding up what the bundled host
  // writes for a HID device record — record handle 4, service class list 3,
  // protocol list 13, language base 9, additional protocol list 15, profile
  // descriptor list 8, the HID integers and booleans 20, language id base 8,
  // browse group 3, the descriptor list's own header 6 and a NUL for each of
  // the three strings — leaves 300 - 86 = 214 bytes for the descriptor and the
  // strings together. Hardware agrees: 144 + 57 = 201 registers, 158 + 57 =
  // 215 does not.
  const size_t recordBudget =
    impl_->descriptorLength + impl_->name.length() +
    impl_->description.length() + impl_->provider.length();
  if (recordBudget > MaximumSdpRecordPayload)
  {
    deactivateCallbackTarget(
      activeHidDevice, hidDeviceCallbackTargetMutex, impl_);
    esp_bt_hid_device_deinit();
    owner_->setError(
      EspBleError::ResourceExhausted,
      "the HID Report Descriptor and names do not fit the SDP record; "
      "use fewer profiles or shorter names");
    return false;
  }
  if (
    esp_bt_hid_device_register_app(
      &impl_->application, &impl_->inputQos, &impl_->outputQos) != ESP_OK ||
    !waitForFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return impl_->registrationCompleted;
    }) || !impl_->registered)
  {
    esp_bt_hid_device_deinit();
    deactivateCallbackTarget(
      activeHidDevice, hidDeviceCallbackTargetMutex, impl_);
    owner_->setError(EspBleError::BackendFailure, "failed to register HID device application");
    return false;
  }
  if (!EspBleClassicVisibilityOwner::apply())
  {
    end();
    owner_->setError(EspBleError::BackendFailure, "failed to make HID device discoverable");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

void EspBleClassicHidDevice::end()
{
  if (impl_ == nullptr) return;
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  bool wasInitialized = false;
  bool wasRegistered = false;
  bool wasConnected = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = true;
    wasInitialized = impl_->initialized;
    wasRegistered = impl_->registered;
    wasConnected = impl_->connected;
  }
  if (wasConnected) esp_bt_hid_device_disconnect();
  if (wasConnected)
  {
    (void)waitForFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return !impl_->connected;
    });
  }
  if (wasRegistered)
  {
    if (esp_bt_hid_device_unregister_app() == ESP_OK)
    {
      (void)waitForFlag([this]() {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return !impl_->registered;
      });
    }
  }
  if (wasInitialized) esp_bt_hid_device_deinit();
  if (wasInitialized)
  {
    (void)waitForFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return !impl_->initialized;
    });
  }
  deactivateCallbackTarget(
    activeHidDevice, hidDeviceCallbackTargetMutex, impl_);
#endif
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->eventHead = 0;
  impl_->eventCount = 0;
  impl_->initializationCompleted = false;
  impl_->initialized = false;
  impl_->registrationCompleted = false;
  impl_->registered = false;
  impl_->ending = false;
  impl_->connected = false;
  impl_->peerAddress = "";
}

bool EspBleClassicHidDevice::initialized() const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->initialized;
}

bool EspBleClassicHidDevice::registered() const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->registered;
}

bool EspBleClassicHidDevice::connected() const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->connected;
}

String EspBleClassicHidDevice::peerAddress() const
{
  if (impl_ == nullptr) return "";
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->peerAddress;
}

bool EspBleClassicHidDevice::sendReport(
  EspBleClassicHidReportType type, uint8_t reportId,
  const uint8_t *data, size_t length)
{
  if (
    !connected() || data == nullptr || length == 0 ||
    length > MaximumReportLength)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid HID device report");
    return false;
  }
#if !ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  (void)type;
  (void)reportId;
  return false;
#else
  esp_hidd_report_type_t backendType = ESP_HIDD_REPORT_TYPE_INTRDATA;
  if (type == EspBleClassicHidReportType::Output)
    backendType = ESP_HIDD_REPORT_TYPE_OUTPUT;
  else if (type == EspBleClassicHidReportType::Feature)
    backendType = ESP_HIDD_REPORT_TYPE_FEATURE;
  if (
    esp_bt_hid_device_send_report(
      backendType, reportId, length, const_cast<uint8_t *>(data)) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure, "failed to send HID device report");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicHidDevice::sendInputReport(
  uint8_t reportId, const uint8_t *data, size_t length)
{
  return sendReport(EspBleClassicHidReportType::Input, reportId, data, length);
}

bool EspBleClassicHidDevice::disconnect()
{
  if (!connected()) return false;
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  return esp_bt_hid_device_disconnect() == ESP_OK;
#else
  return false;
#endif
}

bool EspBleClassicHidDevice::virtualCableUnplug()
{
  if (!connected())
  {
    owner_->setError(EspBleError::InvalidState, "HID Device is not connected");
    return false;
  }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  if (esp_bt_hid_device_virtual_cable_unplug() != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to unplug the virtual cable");
    return false;
  }
  owner_->clearError();
  return true;
#else
  return false;
#endif
}

bool EspBleClassicHidDevice::respondToReportRequest(
  const EspBleClassicHidReportRequest &request,
  const uint8_t *data, size_t length)
{
  if (data == nullptr || length == 0 || length > MaximumReportLength)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid HID report");
    return false;
  }
  if (!connected())
  {
    owner_->setError(EspBleError::InvalidState, "HID Device is not connected");
    return false;
  }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    // Answering when the Host asked nothing would send an unsolicited report on
    // the control channel, which is not what the caller means by "respond".
    if (!impl_->reportRequestPending)
    {
      owner_->setError(
        EspBleError::InvalidState, "no HID report request is pending");
      return false;
    }
    impl_->reportRequestPending = false;
  }
  // The Host matches the answer against what it asked for, so an answer that
  // exceeds the size it offered is refused here rather than being truncated.
  if (length > request.maximumLength && request.maximumLength != 0)
  {
    owner_->setError(
      EspBleError::InvalidArgument,
      "the report is larger than the Host allowed");
    return false;
  }
  const esp_hidd_report_type_t type =
    request.type == EspBleClassicHidReportType::Feature
      ? ESP_HIDD_REPORT_TYPE_FEATURE
      : (request.type == EspBleClassicHidReportType::Output
          ? ESP_HIDD_REPORT_TYPE_OUTPUT : ESP_HIDD_REPORT_TYPE_INPUT);
  if (
    esp_bt_hid_device_send_report(
      type, request.reportId, static_cast<uint16_t>(length),
      const_cast<uint8_t *>(data)) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to answer the HID report request");
    return false;
  }
  owner_->clearError();
  return true;
#else
  (void)request;
  return false;
#endif
}

bool EspBleClassicHidDevice::refuseReportRequest(
  EspBleClassicHidRequestError error)
{
  if (!connected())
  {
    owner_->setError(EspBleError::InvalidState, "HID Device is not connected");
    return false;
  }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    // Either kind of control-channel request can be refused: a Get_Report the
    // device cannot answer, or a Set_Report it will not accept.
    if (!impl_->reportRequestPending && !impl_->setReportPending)
    {
      owner_->setError(
        EspBleError::InvalidState, "no HID report request is pending");
      return false;
    }
    impl_->reportRequestPending = false;
    impl_->setReportPending = false;
  }
  if (
    esp_bt_hid_device_report_error(
      static_cast<esp_hidd_handshake_error_t>(error)) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to refuse the HID report request");
    return false;
  }
  owner_->clearError();
  return true;
#else
  (void)error;
  return false;
#endif
}

EspBleClassicHidProtocolMode EspBleClassicHidDevice::protocolMode() const
{
  if (impl_ == nullptr) return EspBleClassicHidProtocolMode::Report;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->protocolMode;
}

void EspBleClassicHidDevice::onConnected(ConnectionCallback callback)
{
  connectedCallback_ = std::move(callback);
}

void EspBleClassicHidDevice::onDisconnected(ConnectionCallback callback)
{
  disconnectedCallback_ = std::move(callback);
}

void EspBleClassicHidDevice::onOutputReport(ReportCallback callback)
{
  outputReportCallback_ = std::move(callback);
}

void EspBleClassicHidDevice::onSetReport(ReportCallback callback)
{
  setReportCallback_ = std::move(callback);
}

void EspBleClassicHidDevice::onReportRequested(ReportRequestCallback callback)
{
  reportRequestedCallback_ = std::move(callback);
}

void EspBleClassicHidDevice::onProtocolMode(ProtocolModeCallback callback)
{
  protocolModeCallback_ = std::move(callback);
}

size_t EspBleClassicHidDevice::droppedEventCount() const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedEvents;
}

void EspBleClassicHidDevice::update()
{
  if (impl_ == nullptr) return;
  while (true)
  {
    EspBleClassicHidDeviceImpl::Event event;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->eventCount == 0) break;
      event = std::move(impl_->events[impl_->eventHead]);
      impl_->eventHead = (impl_->eventHead + 1) % HidEventQueueCapacity;
      --impl_->eventCount;
    }
    if (
      event.type == EspBleClassicHidDeviceImpl::EventType::Connected &&
      connectedCallback_)
      connectedCallback_(event.connection);
    else if (
      event.type == EspBleClassicHidDeviceImpl::EventType::Disconnected &&
      disconnectedCallback_)
      disconnectedCallback_(event.connection);
    else if (
      event.type == EspBleClassicHidDeviceImpl::EventType::OutputReport)
    {
      // A composed keyboard turns the LED report into keyboard state; the raw
      // callback still sees every output report, custom descriptors included.
      owner_->deliverHidKeyboardLeds(event.report);
      if (outputReportCallback_) outputReportCallback_(event.report);
    }
    else if (
      event.type == EspBleClassicHidDeviceImpl::EventType::SetReport)
    {
      // An Output report reaches onOutputReport() whichever channel carried it,
      // so a sketch that only cares about LEDs does not have to know about
      // Set_Report at all.
      if (event.report.type == EspBleClassicHidReportType::Output)
      {
        owner_->deliverHidKeyboardLeds(event.report);
        if (outputReportCallback_) outputReportCallback_(event.report);
      }
      if (setReportCallback_) setReportCallback_(event.report);
      // Acknowledged unless the sketch refused it during the callback, so a
      // sketch that only reads the value does not have to know the control
      // channel expects a handshake.
      bool acknowledge = false;
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        acknowledge = impl_->setReportPending;
        impl_->setReportPending = false;
      }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
      if (acknowledge)
      {
        (void)esp_bt_hid_device_report_error(
          ESP_HID_PAR_HANDSHAKE_RSP_SUCCESS);
      }
#else
      (void)acknowledge;
#endif
    }
    else if (
      event.type == EspBleClassicHidDeviceImpl::EventType::ReportRequested)
    {
      if (reportRequestedCallback_) reportRequestedCallback_(event.request);
    }
    else if (
      event.type == EspBleClassicHidDeviceImpl::EventType::ProtocolMode)
    {
      if (protocolModeCallback_) protocolModeCallback_(event.protocolMode);
    }
  }
}

EspBleClassicHidHost::EspBleClassicHidHost(EspBleClassic *owner) :
  owner_(owner) {}

EspBleClassicHidHost::~EspBleClassicHidHost()
{
  end();
  delete impl_;
}

bool EspBleClassicHidHost::begin()
{
#if !ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  owner_->setError(
    EspBleError::BackendFailure,
    "Classic HID requires the custom ESP32 Classic host build");
  return false;
#else
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "Classic stack is not initialized");
    return false;
  }
  if (initialized()) return true;
  if (impl_ == nullptr)
  {
    impl_ = new (std::nothrow) EspBleClassicHidHostImpl();
    if (impl_ == nullptr)
    {
      owner_->setError(EspBleError::ResourceExhausted, "failed to allocate HID host state");
      return false;
    }
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = false;
  }
  if (!activateCallbackTarget(
        activeHidHost, hidHostCallbackTargetMutex, impl_))
  {
    owner_->setError(
      EspBleError::InvalidState, "another HID host profile is active");
    return false;
  }
  if (
    esp_bt_hid_host_register_callback(hidHostCallback) != ESP_OK ||
    esp_bt_hid_host_init() != ESP_OK ||
    !waitForFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return impl_->initializationCompleted;
    }) || !impl_->initialized)
  {
    deactivateCallbackTarget(
      activeHidHost, hidHostCallbackTargetMutex, impl_);
    owner_->setError(EspBleError::BackendFailure, "failed to initialize HID host profile");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

void EspBleClassicHidHost::end()
{
  if (impl_ == nullptr) return;
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  bool wasInitialized = false;
  bool wasConnected = false;
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = true;
    wasInitialized = impl_->initialized;
    wasConnected = impl_->connected;
    memcpy(address, impl_->backendAddress, sizeof(address));
  }
  if (wasConnected) esp_bt_hid_host_disconnect(address);
  if (wasConnected)
  {
    (void)waitForFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return !impl_->connected;
    });
  }
  if (wasInitialized) esp_bt_hid_host_deinit();
  if (wasInitialized)
  {
    (void)waitForFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return !impl_->initialized;
    });
  }
  deactivateCallbackTarget(
    activeHidHost, hidHostCallbackTargetMutex, impl_);
#endif
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->eventHead = 0;
  impl_->eventCount = 0;
  impl_->initializationCompleted = false;
  impl_->initialized = false;
  impl_->ending = false;
  impl_->connecting = false;
  impl_->connected = false;
  impl_->handle = 0;
  impl_->peerAddress = "";
  memset(impl_->backendAddress, 0, sizeof(impl_->backendAddress));
}

bool EspBleClassicHidHost::initialized() const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->initialized;
}

bool EspBleClassicHidHost::connect(const char *address)
{
  if (!initialized())
  {
    owner_->setError(EspBleError::InvalidState, "HID host is not initialized");
    return false;
  }
#if !ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  (void)address;
  return false;
#else
  esp_bd_addr_t parsed = {};
  if (!hidParseAddress(address, parsed))
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid HID peer address");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->connecting || impl_->connected)
    {
      owner_->setError(EspBleError::InvalidState, "HID host connection is active");
      return false;
    }
    impl_->connecting = true;
    impl_->peerAddress = address;
    memcpy(impl_->backendAddress, parsed, sizeof(parsed));
  }
  if (esp_bt_hid_host_connect(parsed) != ESP_OK)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->connecting = false;
    impl_->peerAddress = "";
    memset(impl_->backendAddress, 0, sizeof(impl_->backendAddress));
    owner_->setError(EspBleError::BackendFailure, "failed to start HID host connection");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicHidHost::disconnect()
{
  if (!connected()) return false;
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    memcpy(address, impl_->backendAddress, sizeof(address));
  }
  return esp_bt_hid_host_disconnect(address) == ESP_OK;
#else
  return false;
#endif
}

bool EspBleClassicHidHost::connected() const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->connected;
}

String EspBleClassicHidHost::peerAddress() const
{
  if (impl_ == nullptr) return "";
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->peerAddress;
}

namespace
{
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
esp_hidh_report_type_t hostReportType(EspBleClassicHidReportType type)
{
  switch (type)
  {
    case EspBleClassicHidReportType::Output:
      return ESP_HIDH_REPORT_TYPE_OUTPUT;
    case EspBleClassicHidReportType::Feature:
      return ESP_HIDH_REPORT_TYPE_FEATURE;
    default: break;
  }
  return ESP_HIDH_REPORT_TYPE_INPUT;
}
#endif
} // namespace

bool EspBleClassicHidHost::requestReport(
  EspBleClassicHidReportType type, uint8_t reportId, uint16_t maximumLength)
{
  if (!connected())
  {
    owner_->setError(EspBleError::InvalidState, "HID Host is not connected");
    return false;
  }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    memcpy(address, impl_->backendAddress, sizeof(address));
  }
  if (
    esp_bt_hid_host_get_report(
      address, hostReportType(type), reportId,
      static_cast<int>(maximumLength)) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to request a HID report");
    return false;
  }
  owner_->clearError();
  return true;
#else
  (void)type;
  (void)reportId;
  (void)maximumLength;
  return false;
#endif
}

bool EspBleClassicHidHost::sendReport(
  EspBleClassicHidReportType type, const uint8_t *data, size_t length)
{
  if (!connected() || data == nullptr || length == 0 ||
      length > MaximumReportLength)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid HID host report");
    return false;
  }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    memcpy(address, impl_->backendAddress, sizeof(address));
  }
  if (
    esp_bt_hid_host_set_report(
      address, hostReportType(type), const_cast<uint8_t *>(data),
      length) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to send HID host report");
    return false;
  }
  owner_->clearError();
  return true;
#else
  (void)type;
  return false;
#endif
}

bool EspBleClassicHidHost::requestProtocolMode()
{
  if (!connected())
  {
    owner_->setError(EspBleError::InvalidState, "HID Host is not connected");
    return false;
  }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    memcpy(address, impl_->backendAddress, sizeof(address));
  }
  if (esp_bt_hid_host_get_protocol(address) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to request the protocol mode");
    return false;
  }
  owner_->clearError();
  return true;
#else
  return false;
#endif
}

bool EspBleClassicHidHost::setProtocolMode(EspBleClassicHidProtocolMode mode)
{
  if (!connected())
  {
    owner_->setError(EspBleError::InvalidState, "HID Host is not connected");
    return false;
  }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    memcpy(address, impl_->backendAddress, sizeof(address));
    impl_->requestedProtocolMode = mode;
  }
  if (
    esp_bt_hid_host_set_protocol(
      address, mode == EspBleClassicHidProtocolMode::Boot
        ? ESP_HIDH_BOOT_MODE : ESP_HIDH_REPORT_MODE) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to set the protocol mode");
    return false;
  }
  owner_->clearError();
  return true;
#else
  (void)mode;
  return false;
#endif
}

bool EspBleClassicHidHost::requestIdleRate()
{
  if (!connected())
  {
    owner_->setError(EspBleError::InvalidState, "HID Host is not connected");
    return false;
  }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    memcpy(address, impl_->backendAddress, sizeof(address));
  }
  if (esp_bt_hid_host_get_idle(address) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to request the idle rate");
    return false;
  }
  owner_->clearError();
  return true;
#else
  return false;
#endif
}

bool EspBleClassicHidHost::setIdleRate(uint8_t idleRate)
{
  if (!connected())
  {
    owner_->setError(EspBleError::InvalidState, "HID Host is not connected");
    return false;
  }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    memcpy(address, impl_->backendAddress, sizeof(address));
    impl_->requestedIdleRate = idleRate;
  }
  if (esp_bt_hid_host_set_idle(address, idleRate) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to set the idle rate");
    return false;
  }
  owner_->clearError();
  return true;
#else
  (void)idleRate;
  return false;
#endif
}

bool EspBleClassicHidHost::virtualCableUnplug()
{
  if (!connected())
  {
    owner_->setError(EspBleError::InvalidState, "HID Host is not connected");
    return false;
  }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    memcpy(address, impl_->backendAddress, sizeof(address));
  }
  if (esp_bt_hid_host_virtual_cable_unplug(address) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to unplug the virtual cable");
    return false;
  }
  owner_->clearError();
  return true;
#else
  return false;
#endif
}

bool EspBleClassicHidHost::sendOutputReport(
  const uint8_t *data, size_t length)
{
  if (!connected() || data == nullptr || length == 0 ||
      length > MaximumReportLength)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid HID host report");
    return false;
  }
#if ESPBLE_CLASSIC_HID_BACKEND_AVAILABLE
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    memcpy(address, impl_->backendAddress, sizeof(address));
  }
  if (
    esp_bt_hid_host_set_report(
      address, ESP_HIDH_REPORT_TYPE_OUTPUT,
      const_cast<uint8_t *>(data), length) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure, "failed to send HID host report");
    return false;
  }
  owner_->clearError();
  return true;
#else
  return false;
#endif
}

bool EspBleClassicHidHost::setKeyboardLeds(
  bool numLock, bool capsLock, bool scrollLock, bool compose, bool kana)
{
  if (impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidState, "HID Host is not initialized");
    return false;
  }
  bool keyboardFound = false;
  bool hasReportId = false;
  uint8_t reportId = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->reportMapValid)
    {
      for (size_t index = 0; index < impl_->reportMap.count; ++index)
      {
        const EspBleHidReportMapEntry &entry = impl_->reportMap.entries[index];
        if (entry.kind != EspBleHidReportKind::Keyboard) continue;
        keyboardFound = true;
        hasReportId = entry.hasReportId;
        reportId = entry.reportId;
        break;
      }
    }
  }
  if (!keyboardFound)
  {
    // Guessing an ID would send the LED byte to whichever report happens to
    // carry that number on this device.
    owner_->setError(
      EspBleError::NotFound, "the peer has no keyboard report to light");
    return false;
  }
  const uint8_t leds = static_cast<uint8_t>(
    (numLock ? 0x01 : 0) |
    (capsLock ? 0x02 : 0) |
    (scrollLock ? 0x04 : 0) |
    (compose ? 0x08 : 0) |
    (kana ? 0x10 : 0));
  // The transport carries the report ID in front of the payload, so a device
  // that declares one gets it here too.
  uint8_t report[2];
  size_t length = 0;
  if (hasReportId) report[length++] = reportId;
  report[length++] = leds;
  return sendOutputReport(report, length);
}

void EspBleClassicHidHost::onConnected(ConnectionCallback callback)
{
  connectedCallback_ = std::move(callback);
}

void EspBleClassicHidHost::onDisconnected(ConnectionCallback callback)
{
  disconnectedCallback_ = std::move(callback);
}

void EspBleClassicHidHost::onConnectionFailed(
  ConnectionFailureCallback callback)
{
  connectionFailureCallback_ = std::move(callback);
}

void EspBleClassicHidHost::onInputReport(ReportCallback callback)
{
  inputReportCallback_ = std::move(callback);
}

size_t EspBleClassicHidHost::droppedEventCount() const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedEvents;
}

void EspBleClassicHidHost::update()
{
  if (impl_ == nullptr) return;
  while (true)
  {
    EspBleClassicHidHostImpl::Event event;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->eventCount == 0) break;
      event = std::move(impl_->events[impl_->eventHead]);
      impl_->eventHead = (impl_->eventHead + 1) % HidEventQueueCapacity;
      --impl_->eventCount;
    }
    if (
      event.type == EspBleClassicHidHostImpl::EventType::Connected &&
      connectedCallback_)
      connectedCallback_(event.connection);
    else if (
      event.type == EspBleClassicHidHostImpl::EventType::Disconnected &&
      disconnectedCallback_)
      disconnectedCallback_(event.connection);
    else if (
      event.type == EspBleClassicHidHostImpl::EventType::ConnectionFailed &&
      connectionFailureCallback_)
      connectionFailureCallback_(event.failure);
    else if (
      event.type == EspBleClassicHidHostImpl::EventType::InputReport)
    {
      deliverDecoded(event.report);
      if (inputReportCallback_) inputReportCallback_(event.report);
    }
    else if (
      event.type == EspBleClassicHidHostImpl::EventType::ReportResult &&
      reportResultCallback_)
      reportResultCallback_(event.reportResult);
    else if (
      event.type == EspBleClassicHidHostImpl::EventType::ReportSent &&
      reportSentCallback_)
      reportSentCallback_(event.reportResult);
    else if (
      event.type == EspBleClassicHidHostImpl::EventType::ProtocolMode &&
      protocolModeCallback_)
      protocolModeCallback_(event.protocolModeResult);
    else if (
      event.type == EspBleClassicHidHostImpl::EventType::IdleRate &&
      idleRateCallback_)
      idleRateCallback_(event.idleRateResult);
  }
}

// --- Classic HID Device profiles ------------------------------------------
//
// These mirror the BLE profile classes call for call. They build their reports
// with the shared packers and hand them to the HID Device transport, so the
// bytes a Host receives are the same over either radio.

EspBleClassicHidKeyboard::EspBleClassicHidKeyboard(EspBleClassic *owner) :
  owner_(owner) {}
EspBleClassicHidMouse::EspBleClassicHidMouse(EspBleClassic *owner) :
  owner_(owner) {}
EspBleClassicHidConsumerControl::EspBleClassicHidConsumerControl(
  EspBleClassic *owner) : owner_(owner) {}
EspBleClassicHidSystemControl::EspBleClassicHidSystemControl(
  EspBleClassic *owner) : owner_(owner) {}
EspBleClassicHidGamepad::EspBleClassicHidGamepad(EspBleClassic *owner) :
  owner_(owner) {}

bool EspBleClassicHidKeyboard::configure(
  const EspBleClassicHidProfileConfig &config)
{
  configured_ = owner_->configureHidProfile(ESPBLE_HID_PROFILE_KEYBOARD, config);
  return configured_;
}

bool EspBleClassicHidKeyboard::configured() const { return configured_; }

void EspBleClassicHidKeyboard::enableNkro(bool enable)
{
  nkroEnabled_ = enable;
  owner_->setHidKeyboardNkro(enable);
}

bool EspBleClassicHidKeyboard::nkroEnabled() const { return nkroEnabled_; }

bool EspBleClassicHidKeyboard::ready() const
{
  return configured_ && owner_->hidDevice().connected();
}

bool EspBleClassicHidKeyboard::sendReport(
  const EspBleHidKeyboardInputReport &report)
{
  if (nkroEnabled_)
  {
    // A 6KRO report carries modifiers in its own byte, so key slots holding a
    // modifier usage are dropped rather than routed, as on the BLE side.
    nkroState_.clear();
    nkroState_.modifiers = report.modifiers;
    for (uint8_t usage : report.keys)
      if (usage != 0 && usage <= EspBleHidKeyboardNkroReport::MaxBitmapUsage)
        nkroState_.press(usage);
    return sendHeldNkroState();
  }
  uint8_t value[8];
  const size_t length = espBleHidPackKeyboardReport(
    report.modifiers, report.keys, value, sizeof(value));
  return owner_->hidDevice().sendInputReport(
    ESPBLE_HID_REPORT_ID_KEYBOARD, value, length);
}

bool EspBleClassicHidKeyboard::sendReport(
  const EspBleHidKeyboardNkroReport &report)
{
  if (!nkroEnabled_)
  {
    owner_->setError(
      EspBleError::InvalidState,
      "NKRO reports need enableNkro() before configure()");
    return false;
  }
  nkroState_ = report;
  return sendHeldNkroState();
}

bool EspBleClassicHidKeyboard::sendHeldNkroState()
{
  uint8_t value[1 + EspBleHidKeyboardNkroReport::BitmapSize];
  const size_t length = espBleHidPackNkroKeyboardReport(
    nkroState_.modifiers, nkroState_.bitmap,
    EspBleHidKeyboardNkroReport::BitmapSize, value, sizeof(value));
  return owner_->hidDevice().sendInputReport(
    ESPBLE_HID_REPORT_ID_KEYBOARD, value, length);
}

const EspBleHidKeyboardNkroReport &EspBleClassicHidKeyboard::heldState() const
{
  return nkroState_;
}

bool EspBleClassicHidKeyboard::pressUsage(
  uint8_t usage, uint8_t modifiers, uint32_t)
{
  if (nkroEnabled_)
  {
    nkroState_.modifiers = static_cast<uint8_t>(nkroState_.modifiers | modifiers);
    nkroState_.press(usage);
    return sendHeldNkroState();
  }
  EspBleHidKeyboardInputReport report;
  report.modifiers = modifiers;
  report.keys[0] = usage;
  return sendReport(report);
}

bool EspBleClassicHidKeyboard::releaseUsage(uint8_t usage)
{
  if (nkroEnabled_)
  {
    nkroState_.release(usage);
    return sendHeldNkroState();
  }
  // Without a bitmap there is no per-usage state to keep, so releasing one key
  // releases the report, exactly as the BLE side does.
  return releaseAll();
}

bool EspBleClassicHidKeyboard::tapUsage(
  uint8_t usage, uint8_t modifiers, uint32_t holdMs)
{
  if (!pressUsage(usage, modifiers)) return false;
  delay(holdMs);
  return nkroEnabled_ ? releaseUsage(usage) : releaseAll();
}

bool EspBleClassicHidKeyboard::pressKey(char key, uint32_t)
{
  const uint8_t modifiers[] = {0, EspBleHidKeyboardInputReport::LeftShift,
    EspBleHidKeyboardInputReport::RightAlt,
    static_cast<uint8_t>(EspBleHidKeyboardInputReport::LeftShift |
                         EspBleHidKeyboardInputReport::RightAlt)};
  for (uint8_t modifier : modifiers)
  {
    for (uint16_t usage = 1; usage < 256; ++usage)
    {
      if (espBleUsageToUnicode(
            static_cast<uint8_t>(usage), modifier, layout_, false, false) ==
          static_cast<uint8_t>(key))
      {
        return pressUsage(static_cast<uint8_t>(usage), modifier);
      }
    }
  }
  owner_->setError(
    EspBleError::InvalidArgument, "character is not available in keyboard layout");
  return false;
}

bool EspBleClassicHidKeyboard::tapKey(char key, uint32_t holdMs)
{
  if (!pressKey(key)) return false;
  delay(holdMs);
  return releaseAll();
}

bool EspBleClassicHidKeyboard::write(const char *text, uint32_t interKeyDelayMs)
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

bool EspBleClassicHidKeyboard::releaseAll()
{
  if (nkroEnabled_)
  {
    nkroState_.clear();
    return sendHeldNkroState();
  }
  EspBleHidKeyboardInputReport report;
  return sendReport(report);
}

void EspBleClassicHidKeyboard::setLayout(EspBleKeyboardLayout layout)
{
  layout_ = layout;
}

EspBleKeyboardLayout EspBleClassicHidKeyboard::layout() const { return layout_; }

void EspBleClassicHidKeyboard::onOutputReport(OutputReportCallback callback)
{
  outputReportCallback_ = std::move(callback);
}

EspBleClassicHidKeyboardLeds EspBleClassicHidKeyboard::ledState() const
{
  return ledState_;
}

bool EspBleClassicHidMouse::configure(
  const EspBleClassicHidProfileConfig &config)
{
  configured_ = owner_->configureHidProfile(ESPBLE_HID_PROFILE_MOUSE, config);
  return configured_;
}

bool EspBleClassicHidMouse::configured() const { return configured_; }

bool EspBleClassicHidMouse::ready() const
{
  return configured_ && owner_->hidDevice().connected();
}

bool EspBleClassicHidMouse::sendReport(const EspBleHidMouseReport &report)
{
  buttons_ = report.buttons;
  uint8_t value[4];
  const size_t length = espBleHidPackMouseReport(
    report.buttons, report.x, report.y, report.wheel, value, sizeof(value));
  return owner_->hidDevice().sendInputReport(
    ESPBLE_HID_REPORT_ID_MOUSE, value, length);
}

bool EspBleClassicHidMouse::move(
  int8_t x, int8_t y, int8_t wheelAmount, uint8_t buttonMask)
{
  EspBleHidMouseReport report;
  report.buttons = buttonMask != 0 ? buttonMask : buttons_;
  report.x = x;
  report.y = y;
  report.wheel = wheelAmount;
  return sendReport(report);
}

bool EspBleClassicHidMouse::wheel(int8_t amount)
{
  // Wheel only: the pointer must not drift and the held buttons must stay.
  return move(0, 0, amount, buttons_);
}

bool EspBleClassicHidMouse::press(uint8_t buttonMask)
{
  EspBleHidMouseReport report;
  report.buttons = static_cast<uint8_t>(buttons_ | buttonMask);
  return sendReport(report);
}

bool EspBleClassicHidMouse::release(uint8_t buttonMask)
{
  EspBleHidMouseReport report;
  report.buttons = static_cast<uint8_t>(buttons_ & ~buttonMask);
  return sendReport(report);
}

bool EspBleClassicHidMouse::click(uint8_t button, uint32_t holdMs)
{
  if (!press(button)) return false;
  delay(holdMs);
  return release(button);
}

bool EspBleClassicHidMouse::releaseAll()
{
  EspBleHidMouseReport report;
  return sendReport(report);
}

uint8_t EspBleClassicHidMouse::buttons() const { return buttons_; }

bool EspBleClassicHidConsumerControl::configure(
  const EspBleClassicHidProfileConfig &config)
{
  configured_ = owner_->configureHidProfile(ESPBLE_HID_PROFILE_CONSUMER, config);
  return configured_;
}

bool EspBleClassicHidConsumerControl::configured() const { return configured_; }

bool EspBleClassicHidConsumerControl::ready() const
{
  return configured_ && owner_->hidDevice().connected();
}

bool EspBleClassicHidConsumerControl::sendUsage(uint16_t usage)
{
  usage_ = usage;
  uint8_t value[2];
  const size_t length =
    espBleHidPackConsumerReport(usage, value, sizeof(value));
  return owner_->hidDevice().sendInputReport(
    ESPBLE_HID_REPORT_ID_CONSUMER, value, length);
}

bool EspBleClassicHidConsumerControl::release() { return sendUsage(0); }

bool EspBleClassicHidConsumerControl::click(uint16_t usage, uint32_t holdMs)
{
  if (!sendUsage(usage)) return false;
  delay(holdMs);
  return release();
}

uint16_t EspBleClassicHidConsumerControl::usage() const { return usage_; }

bool EspBleClassicHidSystemControl::configure(
  const EspBleClassicHidProfileConfig &config)
{
  configured_ = owner_->configureHidProfile(ESPBLE_HID_PROFILE_SYSTEM, config);
  return configured_;
}

bool EspBleClassicHidSystemControl::configured() const { return configured_; }

bool EspBleClassicHidSystemControl::ready() const
{
  return configured_ && owner_->hidDevice().connected();
}

bool EspBleClassicHidSystemControl::sendUsage(uint8_t usage)
{
  usage_ = usage;
  uint8_t value[1];
  const size_t length = espBleHidPackSystemReport(usage, value, sizeof(value));
  return owner_->hidDevice().sendInputReport(
    ESPBLE_HID_REPORT_ID_SYSTEM, value, length);
}

bool EspBleClassicHidSystemControl::release() { return sendUsage(0); }

bool EspBleClassicHidSystemControl::click(uint8_t usage, uint32_t holdMs)
{
  if (!sendUsage(usage)) return false;
  delay(holdMs);
  return release();
}

uint8_t EspBleClassicHidSystemControl::usage() const { return usage_; }

bool EspBleClassicHidGamepad::configure(
  const EspBleClassicHidProfileConfig &config)
{
  configured_ = owner_->configureHidProfile(ESPBLE_HID_PROFILE_GAMEPAD, config);
  return configured_;
}

bool EspBleClassicHidGamepad::configured() const { return configured_; }

bool EspBleClassicHidGamepad::ready() const
{
  return configured_ && owner_->hidDevice().connected();
}

bool EspBleClassicHidGamepad::sendReport(const EspBleHidGamepadReport &report)
{
  const uint8_t value[11] = {
    static_cast<uint8_t>(report.x), static_cast<uint8_t>(report.y),
    static_cast<uint8_t>(report.z), static_cast<uint8_t>(report.rz),
    static_cast<uint8_t>(report.rx), static_cast<uint8_t>(report.ry),
    report.hat,
    static_cast<uint8_t>(report.buttons & 0xff),
    static_cast<uint8_t>((report.buttons >> 8) & 0xff),
    static_cast<uint8_t>((report.buttons >> 16) & 0xff),
    static_cast<uint8_t>((report.buttons >> 24) & 0xff)};
  return owner_->hidDevice().sendInputReport(
    ESPBLE_HID_REPORT_ID_GAMEPAD, value, sizeof(value));
}

bool EspBleClassicHidGamepad::send(
  int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry,
  uint8_t hat, uint32_t buttons)
{
  EspBleHidGamepadReport report;
  report.x = x;
  report.y = y;
  report.z = z;
  report.rz = rz;
  report.rx = rx;
  report.ry = ry;
  report.hat = hat;
  report.buttons = buttons;
  return sendReport(report);
}

bool EspBleClassicHidGamepad::releaseAll()
{
  EspBleHidGamepadReport report;
  return sendReport(report);
}

// --- Classic HID Host decoding --------------------------------------------

void EspBleClassicHidHost::onKeyboardState(KeyboardStateCallback callback)
{
  keyboardStateCallback_ = std::move(callback);
}

void EspBleClassicHidHost::onKeyboard(KeyboardCallback callback)
{
  keyboardCallback_ = std::move(callback);
}

void EspBleClassicHidHost::onMouse(MouseCallback callback)
{
  mouseCallback_ = std::move(callback);
}

void EspBleClassicHidHost::onReportResult(ReportResultCallback callback)
{
  reportResultCallback_ = std::move(callback);
}

void EspBleClassicHidHost::onReportSent(ReportResultCallback callback)
{
  reportSentCallback_ = std::move(callback);
}

void EspBleClassicHidHost::onProtocolMode(ProtocolModeResultCallback callback)
{
  protocolModeCallback_ = std::move(callback);
}

void EspBleClassicHidHost::onIdleRate(IdleRateResultCallback callback)
{
  idleRateCallback_ = std::move(callback);
}

void EspBleClassicHidHost::setKeyboardLayout(EspBleKeyboardLayout layout)
{
  keyboardLayout_ = layout;
}

EspBleKeyboardLayout EspBleClassicHidHost::keyboardLayout() const
{
  return keyboardLayout_;
}

bool EspBleClassicHidHost::reportMapKnown() const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->reportMapValid;
}

size_t EspBleClassicHidHost::invalidInputReportCount() const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->invalidInputReports;
}

void EspBleClassicHidHost::deliverDecoded(const EspBleClassicHidReport &report)
{
  if (impl_ == nullptr) return;
  if (!keyboardCallback_ && !keyboardStateCallback_ && !mouseCallback_) return;

  EspBleHidReportMapEntry entry;
  bool found = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->reportMapValid) return;
    for (size_t index = 0; index < impl_->reportMap.count; ++index)
    {
      const EspBleHidReportMapEntry &candidate = impl_->reportMap.entries[index];
      // A device with a single report collection may omit the report ID, in
      // which case the transport reports 0 and the descriptor agrees.
      if (candidate.hasReportId && candidate.reportId != report.reportId)
        continue;
      entry = candidate;
      found = true;
      break;
    }
  }
  if (!found) return;

  const uint8_t *data = reinterpret_cast<const uint8_t *>(report.value.c_str());
  size_t length = report.value.length();
  // The transport hands the report over exactly as it arrived, so a device
  // that uses report IDs puts one in front of the payload. Descriptor field
  // offsets are relative to the payload, so that byte is removed first — but
  // only when it really is this report's ID and the rest then fits, so a
  // payload that merely starts with the same value is not truncated.
  if (
    entry.hasReportId && length == entry.inputByteLength() + 1 &&
    data[0] == entry.reportId)
  {
    ++data;
    --length;
  }
  if (entry.inputBitLength == 0 || length != entry.inputByteLength())
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    ++impl_->invalidInputReports;
    return;
  }

  if (entry.kind == EspBleHidReportKind::Keyboard)
  {
    EspBleClassicHidKeyboardState state;
    state.peerAddress = report.peerAddress;
    if (entry.keyboardBitmap)
    {
      if (entry.keyboardHasModifiers)
      {
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
          const size_t source = entry.keyboardModifierBitOffset + bit;
          if ((data[source >> 3] &
               static_cast<uint8_t>(1u << (source & 7))) != 0)
            state.modifiers = static_cast<uint8_t>(state.modifiers | (1u << bit));
        }
      }
      for (uint16_t index = 0; index < entry.keyboardBitmapBitCount; ++index)
      {
        const size_t source = entry.keyboardBitmapBitOffset + index;
        if ((data[source >> 3] & static_cast<uint8_t>(1u << (source & 7))) == 0)
          continue;
        const uint16_t usage = entry.keyboardBitmapUsageMinimum + index;
        if (usage > 0xff) continue;
        state.bitmap[usage >> 3] =
          static_cast<uint8_t>(state.bitmap[usage >> 3] | (1u << (usage & 7)));
      }
    }
    else
    {
      // Boot-compatible layout: modifiers, one constant byte, six usages.
      if (length < 8)
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        ++impl_->invalidInputReports;
        return;
      }
      state.modifiers = data[0];
      for (size_t index = 0; index < 6; ++index)
      {
        const uint8_t usage = data[index + 2];
        // 0x01 to 0x03 are rollover and error codes, not keys. Reporting them
        // as presses would invent keys the user never touched.
        if (usage >= 0x01 && usage <= 0x03)
        {
          std::lock_guard<std::mutex> lock(impl_->mutex);
          ++impl_->invalidInputReports;
          return;
        }
        if (usage == 0) continue;
        state.bitmap[usage >> 3] =
          static_cast<uint8_t>(state.bitmap[usage >> 3] | (1u << (usage & 7)));
      }
    }

    uint8_t previousKeys[EspBleClassicHidKeyboardState::BitmapSize];
    uint8_t previousModifiers = 0;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      memcpy(previousKeys, impl_->previousKeys, sizeof(previousKeys));
      previousModifiers = impl_->previousModifiers;
      memcpy(impl_->previousKeys, state.bitmap, sizeof(impl_->previousKeys));
      impl_->previousModifiers = state.modifiers;
    }

    // State first, then one event per changed usage: the same order the BLE
    // host delivers them in.
    if (keyboardStateCallback_) keyboardStateCallback_(state);
    if (!keyboardCallback_) return;
    for (uint16_t usage = 0; usage < 256; ++usage)
    {
      const bool now = state.isDown(static_cast<uint8_t>(usage));
      const bool before =
        (previousKeys[usage >> 3] &
         static_cast<uint8_t>(1u << (usage & 7))) != 0;
      if (now == before) continue;
      EspBleClassicHidKeyboardEvent value;
      value.peerAddress = report.peerAddress;
      value.usage = static_cast<uint8_t>(usage);
      value.modifiers = state.modifiers;
      value.pressed = now;
      value.released = !now;
      value.rawData = data;
      value.rawLength = length;
      value.unicode = espBleUsageToUnicode(
        static_cast<uint8_t>(usage),
        now ? state.modifiers : previousModifiers, keyboardLayout_, false, false);
      value.ascii = value.unicode <= 0xff
        ? static_cast<uint8_t>(value.unicode) : 0;
      keyboardCallback_(value);
    }
    return;
  }

  if (entry.kind == EspBleHidReportKind::Mouse && mouseCallback_)
  {
    EspBleClassicHidMouseEvent value;
    value.peerAddress = report.peerAddress;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    // Field positions come from the descriptor rather than a fixed layout, so
    // a device that orders or sizes them differently still decodes.
    for (size_t index = 0; index < impl_->reportMap.fieldCount; ++index)
    {
      const EspBleHidReportField &field = impl_->reportMap.fields[index];
      if (field.kind != EspBleHidReportKind::Mouse) continue;
      if (entry.hasReportId && field.reportId != report.reportId) continue;
      const int32_t raw = espBleHidReadFieldValue(field, data, length);
      if (field.usagePage == 0x09)
      {
        if (raw != 0)
          value.buttons = static_cast<uint8_t>(
            value.buttons | (1u << (field.usage - 1)));
      }
      else if (field.usagePage == 0x01 && field.usage == 0x30)
        value.x = static_cast<int16_t>(raw);
      else if (field.usagePage == 0x01 && field.usage == 0x31)
        value.y = static_cast<int16_t>(raw);
      else if (field.usagePage == 0x01 && field.usage == 0x38)
        value.wheel = static_cast<int16_t>(raw);
    }
    value.moved = value.x != 0 || value.y != 0 || value.wheel != 0;
    value.buttonsChanged = value.buttons != impl_->previousMouseButtons;
    impl_->previousMouseButtons = value.buttons;
    mouseCallback_(value);
  }
}
