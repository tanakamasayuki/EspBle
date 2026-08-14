#include "EspBleClassic.h"
#include "EspBleClassicBuild.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <utility>

#include <soc/soc_caps.h>

#if defined(CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_BT_CLASSIC_ENABLED) && \
  defined(CONFIG_BT_SPP_ENABLED) && \
  (defined(ESPBLE_CLASSIC_ONLY) || defined(ESPBLE_ENABLE_CLASSIC) || \
   defined(ESPBLE_CLASSIC_CUSTOM_HOST))
#define ESPBLE_CLASSIC_BACKEND_AVAILABLE 1
#include <esp32-hal-alloc-bt-classic-mem.h>
#include <esp32-hal-bt.h>
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
// The independently built Classic-only Bluedroid host is namespaced so it can
// coexist at link time with Arduino-ESP32's built-in Bluedroid archive.
#define esp_bluedroid_attach_hci_driver \
  espble_bd_esp_bluedroid_attach_hci_driver
#define esp_bluedroid_detach_hci_driver \
  espble_bd_esp_bluedroid_detach_hci_driver
#define esp_bluedroid_deinit espble_bd_esp_bluedroid_deinit
#define esp_bluedroid_disable espble_bd_esp_bluedroid_disable
#define esp_bluedroid_enable espble_bd_esp_bluedroid_enable
#define esp_bluedroid_get_status espble_bd_esp_bluedroid_get_status
#define esp_bluedroid_init espble_bd_esp_bluedroid_init
#define esp_bt_gap_pin_reply espble_bd_esp_bt_gap_pin_reply
#define esp_bt_gap_register_callback espble_bd_esp_bt_gap_register_callback
#define esp_bt_gap_set_device_name espble_bd_esp_bt_gap_set_device_name
#define esp_bt_gap_set_pin espble_bd_esp_bt_gap_set_pin
#define esp_bt_gap_set_scan_mode espble_bd_esp_bt_gap_set_scan_mode
#define esp_bt_gap_start_discovery espble_bd_esp_bt_gap_start_discovery
#define esp_bt_gap_cancel_discovery espble_bd_esp_bt_gap_cancel_discovery
#define esp_bt_gap_resolve_eir_data espble_bd_esp_bt_gap_resolve_eir_data
#define esp_bt_gap_set_security_param \
  espble_bd_esp_bt_gap_set_security_param
#define esp_bt_gap_ssp_confirm_reply \
  espble_bd_esp_bt_gap_ssp_confirm_reply
#define esp_bt_gap_ssp_passkey_reply \
  espble_bd_esp_bt_gap_ssp_passkey_reply
#define esp_bt_gap_get_bond_device_num espble_bd_esp_bt_gap_get_bond_device_num
#define esp_bt_gap_get_bond_device_list \
  espble_bd_esp_bt_gap_get_bond_device_list
#define esp_bt_gap_remove_bond_device espble_bd_esp_bt_gap_remove_bond_device
#define esp_spp_connect espble_bd_esp_spp_connect
#define esp_spp_deinit espble_bd_esp_spp_deinit
#define esp_spp_disconnect espble_bd_esp_spp_disconnect
#define esp_spp_enhanced_init espble_bd_esp_spp_enhanced_init
#define esp_spp_register_callback espble_bd_esp_spp_register_callback
#define esp_spp_start_discovery espble_bd_esp_spp_start_discovery
#define esp_spp_start_srv espble_bd_esp_spp_start_srv
#define esp_spp_stop_srv espble_bd_esp_spp_stop_srv
#define esp_spp_write espble_bd_esp_spp_write
#include <esp_bluedroid_hci.h>
#include "EspBleHciBroker.h"
#endif
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <esp_spp_api.h>
#else
#define ESPBLE_CLASSIC_BACKEND_AVAILABLE 0
#endif

namespace
{
constexpr size_t EventQueueCapacity = 12;

#if ESPBLE_CLASSIC_BACKEND_AVAILABLE && defined(ESPBLE_CLASSIC_CUSTOM_HOST)
void classicHostSend(uint8_t *data, uint16_t length)
{
  (void)espble_hci_broker_send(ESPBLE_HCI_HOST_CLASSIC, data, length);
}

bool classicHostCanSend()
{
  return espble_hci_broker_can_send(ESPBLE_HCI_HOST_CLASSIC);
}

esp_err_t classicHostRegisterCallbacks(
  const esp_bluedroid_hci_driver_callbacks_t *callbacks)
{
  static_assert(
    sizeof(esp_bluedroid_hci_driver_callbacks_t) ==
      sizeof(espble_hci_host_callbacks_t),
    "Bluedroid and broker callback ABIs must match");
  return espble_hci_broker_register(
    ESPBLE_HCI_HOST_CLASSIC,
    reinterpret_cast<const espble_hci_host_callbacks_t *>(callbacks));
}

const esp_bluedroid_hci_driver_operations_t ClassicHostHciOperations = {
  .send = classicHostSend,
  .check_send_available = classicHostCanSend,
  .register_host_callback = classicHostRegisterCallbacks,
};

bool attachClassicHost()
{
  return esp_bluedroid_attach_hci_driver(&ClassicHostHciOperations) == ESP_OK;
}

void detachClassicHost()
{
  espble_hci_broker_unregister(ESPBLE_HCI_HOST_CLASSIC);
  (void)esp_bluedroid_detach_hci_driver();
}

bool stopAdoptedController()
{
  return btStop();
}

void shutdownAdoptedController()
{
  (void)espble_hci_broker_shutdown_controller();
}

// Answers the broker's platform question before setup() runs, so the NimBLE
// port can size the controller for both hosts even when BLE starts first. This
// is the one place that knows how this platform links its Bluetooth libraries;
// the HCI component below only asks.
struct ClassicHostPresence
{
  ClassicHostPresence() { espble_hci_broker_set_classic_host_expected(true); }
};
const ClassicHostPresence classicHostPresence;

// Classic starts the controller, so it picks the mode both hosts have to live
// with.  BT_MODE_CLASSIC_BT releases the BLE controller memory for the rest of
// the boot, which would make a later ble.begin() impossible.  Choose it only
// when no BLE host is linked into the sketch; otherwise run the dual-mode
// controller so the sketch can start BLE before, after, or alongside Classic.
bt_mode controllerStartMode()
{
  return bleInUse() ? BT_MODE_BTDM : BT_MODE_CLASSIC_BT;
}
#endif

const char *errorName(EspBleError error)
{
  switch (error)
  {
    case EspBleError::None: return "None";
    case EspBleError::InvalidState: return "InvalidState";
    case EspBleError::InvalidArgument: return "InvalidArgument";
    case EspBleError::BackendFailure: return "BackendFailure";
    case EspBleError::ResourceExhausted: return "ResourceExhausted";
    case EspBleError::NotFound: return "NotFound";
    case EspBleError::Timeout: return "Timeout";
  }
  return "Unknown";
}

#if ESPBLE_CLASSIC_BACKEND_AVAILABLE
String formatAddress(const esp_bd_addr_t address)
{
  char value[18];
  snprintf(
    value, sizeof(value), "%02x:%02x:%02x:%02x:%02x:%02x",
    address[0], address[1], address[2], address[3], address[4], address[5]);
  return String(value);
}

bool parseAddress(const char *value, esp_bd_addr_t address)
{
  if (value == nullptr) return false;
  unsigned bytes[ESP_BD_ADDR_LEN] = {};
  if (sscanf(
        value, "%02x:%02x:%02x:%02x:%02x:%02x",
        &bytes[0], &bytes[1], &bytes[2],
        &bytes[3], &bytes[4], &bytes[5]) != ESP_BD_ADDR_LEN)
  {
    return false;
  }
  char trailing = '\0';
  if (sscanf(
        value, "%*02x:%*02x:%*02x:%*02x:%*02x:%*02x%c", &trailing) == 1)
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

constexpr size_t InquiryQueueCapacity = 16;

struct EspBleClassicInquiryImpl
{
  bool enqueue(EspBleClassicInquiryResult result)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (count == InquiryQueueCapacity)
    {
      // Report the loss instead of pretending the peer was never in range.
      ++dropped;
      return false;
    }
    queue[(head + count) % InquiryQueueCapacity] = std::move(result);
    ++count;
    return true;
  }

  mutable std::mutex mutex;
  EspBleClassicInquiryResult queue[InquiryQueueCapacity];
  size_t head = 0;
  size_t count = 0;
  size_t dropped = 0;
  bool running = false;
  bool stopRequested = false;
  bool completionPending = false;
  bool completionCancelled = false;
};

constexpr size_t SecurityEventQueueCapacity = 8;

struct EspBleClassicImpl
{
  enum class EventType : uint8_t
  {
    SecurityChanged,
    NumericComparison,
    PasskeyDisplayed,
    PasskeyRequested,
  };

  struct Event
  {
    EventType type = EventType::SecurityChanged;
    EspBleClassicSecurityChanged securityChanged;
    EspBleClassicNumericComparison numericComparison;
    EspBleClassicPasskeyDisplayed passkeyDisplayed;
    EspBleClassicPasskeyRequested passkeyRequested;
  };

  bool enqueue(Event event)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (eventCount == SecurityEventQueueCapacity)
    {
      ++droppedEvents;
      return false;
    }
    events[(eventHead + eventCount) % SecurityEventQueueCapacity] =
      std::move(event);
    ++eventCount;
    return true;
  }

  mutable std::mutex mutex;
  bool initialized = false;
  String deviceName;
  EspBleClassicSecurityConfig security;
  Event events[SecurityEventQueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  // A pairing request is answered exactly once, so the pending peer is kept
  // here and cleared by the answer, the timeout, or the completion event.
  bool numericComparisonPending = false;
  bool passkeyPending = false;
  bool numericComparisonCallbackConfigured = false;
  bool passkeyRequestedCallbackConfigured = false;
  String numericComparisonAddress;
  String passkeyAddress;
  uint8_t numericComparisonBackendAddress[6] = {};
  uint8_t passkeyBackendAddress[6] = {};
  uint32_t numericComparisonDeadlineMs = 0;
  uint32_t passkeyDeadlineMs = 0;
};

struct EspBleClassicSppImpl
{
  enum class EventType : uint8_t
  {
    ServerStarted,
    Connected,
    Disconnected,
    Data,
    WriteCompleted,
    ConnectionFailed,
  };

  struct Event
  {
    EventType type = EventType::ServerStarted;
    EspBleClassicSppSession session;
    EspBleClassicSppData data;
    EspBleClassicSppWriteResult writeResult;
    EspBleClassicSppConnectionFailure failure;
  };

  bool enqueue(Event event)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (eventCount == EventQueueCapacity)
    {
      ++droppedEvents;
      return false;
    }
    const size_t tail = (eventHead + eventCount) % EventQueueCapacity;
    events[tail] = std::move(event);
    ++eventCount;
    return true;
  }

  mutable std::mutex mutex;
  std::atomic<size_t> callbackUsers{0};
  Event events[EventQueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  bool initialized = false;
  bool initializationCompleted = false;
  bool ending = false;
  bool serverStartPending = false;
  bool serverRunning = false;
  String serverName;
  uint8_t serverChannel = 0;
  uint32_t backendHandle = 0;
  EspBleClassicSppSession activeSession;
  EspBleClassicSppSessionId nextSessionId = 1;
  String txQueue[EspBleClassicSpp::WriteQueueCapacity];
  size_t txHead = 0;
  size_t txCount = 0;
  size_t droppedWrites = 0;
  bool txInFlight = false;
  bool txCongested = false;
  uint8_t rxBuffer[EspBleClassicSpp::ReceiveBufferCapacity] = {};
  size_t rxHead = 0;
  size_t rxCount = 0;
  size_t droppedReceiveBytes = 0;
  bool connecting = false;
  String connectAddress;
#if ESPBLE_CLASSIC_BACKEND_AVAILABLE
  esp_bd_addr_t connectBackendAddress = {};
#else
  uint8_t connectBackendAddress[6] = {};
#endif
  uint32_t connectDeadlineMs = 0;
};

#if ESPBLE_CLASSIC_BACKEND_AVAILABLE
namespace
{
std::atomic<EspBleClassic *> activeClassic{nullptr};
std::atomic<EspBleClassicImpl *> activeClassicImpl{nullptr};
// The GAP callback runs on Bluedroid's task, so it reaches the inquiry
// state through this pointer rather than through the owning object.
std::atomic<EspBleClassicInquiryImpl *> activeInquiry{nullptr};
std::atomic<EspBleClassicSppImpl *> activeSpp{nullptr};
std::mutex sppCallbackTargetMutex;

class SppCallbackLease
{
public:
  SppCallbackLease()
  {
    std::lock_guard<std::mutex> lock(sppCallbackTargetMutex);
    impl_ = activeSpp.load(std::memory_order_relaxed);
    if (impl_ != nullptr)
      impl_->callbackUsers.fetch_add(1, std::memory_order_acq_rel);
  }

  ~SppCallbackLease()
  {
    if (impl_ != nullptr)
      impl_->callbackUsers.fetch_sub(1, std::memory_order_acq_rel);
  }

  EspBleClassicSppImpl *get() const { return impl_; }

private:
  EspBleClassicSppImpl *impl_ = nullptr;
};

bool activateSppCallbackTarget(EspBleClassicSppImpl *impl)
{
  std::lock_guard<std::mutex> lock(sppCallbackTargetMutex);
  EspBleClassicSppImpl *current =
    activeSpp.load(std::memory_order_relaxed);
  if (current != nullptr && current != impl) return false;
  activeSpp.store(impl, std::memory_order_release);
  return true;
}

void deactivateSppCallbackTarget(EspBleClassicSppImpl *impl)
{
  {
    std::lock_guard<std::mutex> lock(sppCallbackTargetMutex);
    if (activeSpp.load(std::memory_order_relaxed) == impl)
      activeSpp.store(nullptr, std::memory_order_release);
  }
  while (impl->callbackUsers.load(std::memory_order_acquire) != 0) delay(1);
}

void failConnection(
  EspBleClassicSppImpl *impl, EspBleError error, const char *detail)
{
  EspBleClassicSppConnectionFailure failure;
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->connecting) return;
    failure.peerAddress = impl->connectAddress;
    failure.error = error;
    failure.detail = detail == nullptr ? "" : detail;
    impl->connecting = false;
    impl->connectAddress = "";
    memset(impl->connectBackendAddress, 0, sizeof(impl->connectBackendAddress));
    impl->connectDeadlineMs = 0;
  }
  EspBleClassicSppImpl::Event event;
  event.type = EspBleClassicSppImpl::EventType::ConnectionFailed;
  event.failure = std::move(failure);
  impl->enqueue(std::move(event));
}

void startNextWrite(EspBleClassicSppImpl *impl)
{
  uint32_t handle = 0;
  uint8_t *data = nullptr;
  size_t length = 0;
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (
      impl->ending || impl->backendHandle == 0 || impl->txCount == 0 ||
      impl->txInFlight || impl->txCongested)
    {
      return;
    }
    String &value = impl->txQueue[impl->txHead];
    handle = impl->backendHandle;
    data = reinterpret_cast<uint8_t *>(const_cast<char *>(value.c_str()));
    length = value.length();
    impl->txInFlight = true;
  }
  const esp_err_t status = esp_spp_write(handle, length, data);
  if (status == ESP_OK) return;

  EspBleClassicSppWriteResult result;
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    result.sessionId = impl->activeSession.id;
    result.length = length;
    result.error = EspBleError::BackendFailure;
    result.detail = String("SPP write start failed: ") + String(status);
    impl->txQueue[impl->txHead] = "";
    impl->txHead =
      (impl->txHead + 1) % EspBleClassicSpp::WriteQueueCapacity;
    --impl->txCount;
    ++impl->droppedWrites;
    impl->txInFlight = false;
  }
  EspBleClassicSppImpl::Event event;
  event.type = EspBleClassicSppImpl::EventType::WriteCompleted;
  event.writeResult = std::move(result);
  impl->enqueue(std::move(event));
  startNextWrite(impl);
}

void startPendingServer(EspBleClassicSppImpl *impl)
{
  String name;
  uint8_t channel = 0;
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->initialized || !impl->serverStartPending || impl->ending) return;
    name = impl->serverName;
    channel = impl->serverChannel;
  }
  if (
    esp_bt_gap_set_scan_mode(
      ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE) != ESP_OK ||
    esp_spp_start_srv(
      ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, channel, name.c_str()) != ESP_OK)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->serverStartPending = false;
  }
}

void classicGapCallback(
  esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *parameter)
{
  if (parameter == nullptr) return;
  EspBleClassicInquiryImpl *inquiry =
    activeInquiry.load(std::memory_order_acquire);
  if (inquiry != nullptr && event == ESP_BT_GAP_DISC_RES_EVT)
  {
    EspBleClassicInquiryResult result;
    result.address = formatAddress(parameter->disc_res.bda);
    uint8_t *eir = nullptr;
    for (int index = 0; index < parameter->disc_res.num_prop; ++index)
    {
      const esp_bt_gap_dev_prop_t &property = parameter->disc_res.prop[index];
      if (property.val == nullptr) continue;
      if (property.type == ESP_BT_GAP_DEV_PROP_BDNAME)
      {
        // Bluedroid may or may not include the terminator in the length.
        const char *text = static_cast<const char *>(property.val);
        const size_t length = property.len > 0 && text[property.len - 1] == '\0'
          ? static_cast<size_t>(property.len) - 1
          : static_cast<size_t>(property.len);
        result.name = String(text, length);
      }
      else if (property.type == ESP_BT_GAP_DEV_PROP_COD &&
               property.len >= static_cast<int>(sizeof(uint32_t)))
      {
        memcpy(&result.classOfDevice, property.val, sizeof(result.classOfDevice));
        result.hasClassOfDevice = true;
      }
      else if (property.type == ESP_BT_GAP_DEV_PROP_RSSI &&
               property.len >= static_cast<int>(sizeof(int8_t)))
      {
        int8_t rssi = 0;
        memcpy(&rssi, property.val, sizeof(rssi));
        result.rssi = rssi;
        result.hasRssi = true;
      }
      else if (property.type == ESP_BT_GAP_DEV_PROP_EIR)
      {
        eir = static_cast<uint8_t *>(property.val);
      }
    }
    // A peer that answers the inquiry without a name property often still
    // carries one in its extended inquiry response.
    if (result.name.isEmpty() && eir != nullptr)
    {
      uint8_t length = 0;
      uint8_t *name = esp_bt_gap_resolve_eir_data(
        eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &length);
      if (name == nullptr)
        name = esp_bt_gap_resolve_eir_data(
          eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &length);
      if (name != nullptr && length > 0)
        result.name = String(reinterpret_cast<const char *>(name), length);
    }
    inquiry->enqueue(std::move(result));
  }
  else if (inquiry != nullptr && event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT &&
           parameter->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED)
  {
    std::lock_guard<std::mutex> lock(inquiry->mutex);
    inquiry->running = false;
    inquiry->completionCancelled = inquiry->stopRequested;
    inquiry->stopRequested = false;
    inquiry->completionPending = true;
  }

  EspBleClassicImpl *classic = activeClassicImpl.load(std::memory_order_acquire);
  if (classic == nullptr) return;

  if (event == ESP_BT_GAP_AUTH_CMPL_EVT)
  {
    {
      // Pairing finished, so nothing is waiting for an answer any more.
      std::lock_guard<std::mutex> lock(classic->mutex);
      if (classic->numericComparisonPending &&
          memcmp(classic->numericComparisonBackendAddress,
            parameter->auth_cmpl.bda, ESP_BD_ADDR_LEN) == 0)
      {
        classic->numericComparisonPending = false;
        classic->numericComparisonAddress = "";
        classic->numericComparisonDeadlineMs = 0;
      }
      if (classic->passkeyPending &&
          memcmp(classic->passkeyBackendAddress,
            parameter->auth_cmpl.bda, ESP_BD_ADDR_LEN) == 0)
      {
        classic->passkeyPending = false;
        classic->passkeyAddress = "";
        classic->passkeyDeadlineMs = 0;
      }
    }
    EspBleClassicImpl::Event queued;
    queued.type = EspBleClassicImpl::EventType::SecurityChanged;
    queued.securityChanged.peerAddress =
      formatAddress(parameter->auth_cmpl.bda);
    queued.securityChanged.success =
      parameter->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS;
    queued.securityChanged.status = parameter->auth_cmpl.stat;
    classic->enqueue(std::move(queued));
  }
  else if (event == ESP_BT_GAP_CFM_REQ_EVT)
  {
    bool justWorks = false;
    bool canAsk = false;
    {
      std::lock_guard<std::mutex> lock(classic->mutex);
      justWorks = !classic->security.enabled ||
        classic->security.ioCapability ==
          EspBleClassicSecurityIoCapability::None;
      canAsk = classic->security.enabled &&
        classic->security.ioCapability ==
          EspBleClassicSecurityIoCapability::DisplayYesNo &&
        classic->numericComparisonCallbackConfigured &&
        !classic->numericComparisonPending;
      if (canAsk)
      {
        classic->numericComparisonPending = true;
        classic->numericComparisonAddress =
          formatAddress(parameter->cfm_req.bda);
        memcpy(classic->numericComparisonBackendAddress,
          parameter->cfm_req.bda, ESP_BD_ADDR_LEN);
        classic->numericComparisonDeadlineMs =
          millis() + classic->security.responseTimeoutMilliseconds;
      }
    }
    // Just Works has nobody to ask. Any other configuration without a
    // reachable application answer is rejected rather than accepted blindly.
    if (justWorks)
    {
      esp_bt_gap_ssp_confirm_reply(parameter->cfm_req.bda, true);
      return;
    }
    if (!canAsk)
    {
      esp_bt_gap_ssp_confirm_reply(parameter->cfm_req.bda, false);
      return;
    }
    EspBleClassicImpl::Event queued;
    queued.type = EspBleClassicImpl::EventType::NumericComparison;
    queued.numericComparison.peerAddress =
      formatAddress(parameter->cfm_req.bda);
    queued.numericComparison.value = parameter->cfm_req.num_val;
    if (!classic->enqueue(std::move(queued)))
    {
      {
        std::lock_guard<std::mutex> lock(classic->mutex);
        classic->numericComparisonPending = false;
        classic->numericComparisonAddress = "";
        classic->numericComparisonDeadlineMs = 0;
      }
      esp_bt_gap_ssp_confirm_reply(parameter->cfm_req.bda, false);
    }
  }
  else if (event == ESP_BT_GAP_KEY_NOTIF_EVT)
  {
    EspBleClassicImpl::Event queued;
    queued.type = EspBleClassicImpl::EventType::PasskeyDisplayed;
    queued.passkeyDisplayed.peerAddress =
      formatAddress(parameter->key_notif.bda);
    queued.passkeyDisplayed.passkey = parameter->key_notif.passkey;
    classic->enqueue(std::move(queued));
  }
  else if (event == ESP_BT_GAP_KEY_REQ_EVT)
  {
    bool canAsk = false;
    {
      std::lock_guard<std::mutex> lock(classic->mutex);
      canAsk = classic->security.enabled &&
        classic->security.ioCapability ==
          EspBleClassicSecurityIoCapability::KeyboardOnly &&
        classic->passkeyRequestedCallbackConfigured &&
        !classic->passkeyPending;
      if (canAsk)
      {
        classic->passkeyPending = true;
        classic->passkeyAddress = formatAddress(parameter->key_req.bda);
        memcpy(classic->passkeyBackendAddress,
          parameter->key_req.bda, ESP_BD_ADDR_LEN);
        classic->passkeyDeadlineMs =
          millis() + classic->security.responseTimeoutMilliseconds;
      }
    }
    if (!canAsk)
    {
      esp_bt_gap_ssp_passkey_reply(parameter->key_req.bda, false, 0);
      return;
    }
    EspBleClassicImpl::Event queued;
    queued.type = EspBleClassicImpl::EventType::PasskeyRequested;
    queued.passkeyRequested.peerAddress = formatAddress(parameter->key_req.bda);
    if (!classic->enqueue(std::move(queued)))
    {
      {
        std::lock_guard<std::mutex> lock(classic->mutex);
        classic->passkeyPending = false;
        classic->passkeyAddress = "";
        classic->passkeyDeadlineMs = 0;
      }
      esp_bt_gap_ssp_passkey_reply(parameter->key_req.bda, false, 0);
    }
  }
  else if (event == ESP_BT_GAP_PIN_REQ_EVT)
  {
    // Legacy PIN pairing has no application path here, and a fixed PIN would
    // be a fixed key. Refuse instead of accepting with a guessable one.
    esp_bt_pin_code_t pin = {};
    esp_bt_gap_pin_reply(parameter->pin_req.bda, false, 0, pin);
  }
}

void sppCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *parameter)
{
  SppCallbackLease lease;
  EspBleClassicSppImpl *impl = lease.get();
  if (impl == nullptr || parameter == nullptr) return;

  if (event == ESP_SPP_INIT_EVT)
  {
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->initialized = parameter->init.status == ESP_SPP_SUCCESS;
      impl->initializationCompleted = true;
    }
    if (parameter->init.status == ESP_SPP_SUCCESS) startPendingServer(impl);
  }
  else if (event == ESP_SPP_UNINIT_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->initialized = false;
  }
  else if (event == ESP_SPP_START_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->serverStartPending = false;
    impl->serverRunning = parameter->start.status == ESP_SPP_SUCCESS;
    if (impl->serverRunning) impl->serverChannel = parameter->start.scn;
    if (impl->serverRunning)
    {
      EspBleClassicSppImpl::Event queued;
      queued.type = EspBleClassicSppImpl::EventType::ServerStarted;
      const size_t tail = (impl->eventHead + impl->eventCount) % EventQueueCapacity;
      if (impl->eventCount == EventQueueCapacity) ++impl->droppedEvents;
      else
      {
        impl->events[tail] = std::move(queued);
        ++impl->eventCount;
      }
    }
  }
  else if (event == ESP_SPP_DISCOVERY_COMP_EVT)
  {
    bool connecting = false;
    esp_bd_addr_t address = {};
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      connecting = impl->connecting && !impl->ending;
      memcpy(address, impl->connectBackendAddress, sizeof(address));
    }
    if (!connecting) return;
    if (
      parameter->disc_comp.status != ESP_SPP_SUCCESS ||
      parameter->disc_comp.scn_num == 0)
    {
      failConnection(
        impl, EspBleError::NotFound,
        "peer does not advertise an SPP service");
    }
    else if (
      esp_spp_connect(
        ESP_SPP_SEC_NONE, ESP_SPP_ROLE_MASTER,
        parameter->disc_comp.scn[0], address) != ESP_OK)
    {
      failConnection(
        impl, EspBleError::BackendFailure,
        "failed to start the SPP connection");
    }
  }
  else if (
    event == ESP_SPP_CL_INIT_EVT &&
    parameter->cl_init.status != ESP_SPP_SUCCESS)
  {
    failConnection(
      impl, EspBleError::BackendFailure,
      "failed to initialize the SPP client connection");
  }
  else if (event == ESP_SPP_OPEN_EVT || event == ESP_SPP_SRV_OPEN_EVT)
  {
    const bool incoming = event == ESP_SPP_SRV_OPEN_EVT;
    const esp_spp_status_t status = incoming
      ? parameter->srv_open.status : parameter->open.status;
    if (status != ESP_SPP_SUCCESS)
    {
      if (!incoming)
      {
        failConnection(
          impl, EspBleError::BackendFailure, "SPP connection failed");
      }
      return;
    }
    const uint32_t handle = incoming
      ? parameter->srv_open.handle : parameter->open.handle;
    const uint8_t *address = incoming
      ? parameter->srv_open.rem_bda : parameter->open.rem_bda;
    EspBleClassicSppSession session;
    bool accepted = false;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      accepted = !impl->ending && impl->backendHandle == 0;
      if (accepted)
      {
        session.id = impl->nextSessionId++;
        if (session.id == 0) session.id = impl->nextSessionId++;
        session.peerAddress = formatAddress(address);
        session.incoming = incoming;
        impl->backendHandle = handle;
        impl->activeSession = session;
        impl->connecting = false;
        impl->connectAddress = "";
        memset(
          impl->connectBackendAddress, 0,
          sizeof(impl->connectBackendAddress));
        impl->connectDeadlineMs = 0;
      }
    }
    if (!accepted)
    {
      esp_spp_disconnect(handle);
      return;
    }
    EspBleClassicSppImpl::Event queued;
    queued.type = EspBleClassicSppImpl::EventType::Connected;
    queued.session = std::move(session);
    impl->enqueue(std::move(queued));
  }
  else if (
    event == ESP_SPP_DATA_IND_EVT &&
    parameter->data_ind.status == ESP_SPP_SUCCESS)
  {
    EspBleClassicSppData dataEvent;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (
        impl->backendHandle == 0 ||
        impl->backendHandle != parameter->data_ind.handle)
      {
        return;
      }
      dataEvent.sessionId = impl->activeSession.id;
      dataEvent.value = String(
        reinterpret_cast<const char *>(parameter->data_ind.data),
        parameter->data_ind.len);
      for (size_t index = 0; index < parameter->data_ind.len; ++index)
      {
        if (impl->rxCount == EspBleClassicSpp::ReceiveBufferCapacity)
        {
          ++impl->droppedReceiveBytes;
          continue;
        }
        const size_t tail =
          (impl->rxHead + impl->rxCount) %
          EspBleClassicSpp::ReceiveBufferCapacity;
        impl->rxBuffer[tail] = parameter->data_ind.data[index];
        ++impl->rxCount;
      }
    }
    EspBleClassicSppImpl::Event queued;
    queued.type = EspBleClassicSppImpl::EventType::Data;
    queued.data = std::move(dataEvent);
    impl->enqueue(std::move(queued));
  }
  else if (event == ESP_SPP_WRITE_EVT)
  {
    EspBleClassicSppWriteResult result;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (!impl->txInFlight || impl->txCount == 0) return;
      result.sessionId = impl->activeSession.id;
      result.length = impl->txQueue[impl->txHead].length();
      result.success = parameter->write.status == ESP_SPP_SUCCESS;
      if (!result.success)
      {
        result.error = EspBleError::BackendFailure;
        result.detail = String("SPP write failed: ") +
          String(parameter->write.status);
        ++impl->droppedWrites;
      }
      impl->txQueue[impl->txHead] = "";
      impl->txHead =
        (impl->txHead + 1) % EspBleClassicSpp::WriteQueueCapacity;
      --impl->txCount;
      impl->txInFlight = false;
      impl->txCongested = parameter->write.cong;
    }
    EspBleClassicSppImpl::Event queued;
    queued.type = EspBleClassicSppImpl::EventType::WriteCompleted;
    queued.writeResult = std::move(result);
    impl->enqueue(std::move(queued));
    startNextWrite(impl);
  }
  else if (event == ESP_SPP_CONG_EVT)
  {
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->txCongested = parameter->cong.cong;
    }
    startNextWrite(impl);
  }
  else if (event == ESP_SPP_CLOSE_EVT)
  {
    EspBleClassicSppSession session;
    bool hadSession = false;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      hadSession =
        impl->backendHandle != 0 &&
        impl->backendHandle == parameter->close.handle;
      if (hadSession) session = impl->activeSession;
      impl->backendHandle = 0;
      impl->activeSession = EspBleClassicSppSession();
      for (String &value : impl->txQueue) value = "";
      impl->txHead = 0;
      impl->txCount = 0;
      impl->txInFlight = false;
      impl->txCongested = false;
      impl->rxHead = 0;
      impl->rxCount = 0;
    }
    if (hadSession)
    {
      EspBleClassicSppImpl::Event queued;
      queued.type = EspBleClassicSppImpl::EventType::Disconnected;
      queued.session = std::move(session);
      impl->enqueue(std::move(queued));
    }
  }
  else if (event == ESP_SPP_SRV_STOP_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->serverRunning = false;
    impl->serverStartPending = false;
  }
}
} // namespace
#endif

EspBleClassicSpp::EspBleClassicSpp(EspBleClassic *owner) : owner_(owner) {}

EspBleClassicSpp::~EspBleClassicSpp()
{
  end();
  delete impl_;
}

bool EspBleClassicSpp::begin()
{
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  owner_->setError(
    EspBleError::BackendFailure,
    "Classic SPP is available only on the original ESP32");
  return false;
#else
  if (impl_ == nullptr)
  {
    impl_ = new (std::nothrow) EspBleClassicSppImpl();
    if (impl_ == nullptr)
    {
      owner_->setError(
        EspBleError::ResourceExhausted,
        "failed to allocate Classic SPP state");
      return false;
    }
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->initialized)
    {
      owner_->clearError();
      return true;
    }
    impl_->initializationCompleted = false;
  }
  if (!activateSppCallbackTarget(impl_))
  {
    owner_->setError(
      EspBleError::InvalidState, "another SPP profile is active");
    return false;
  }
  if (esp_spp_register_callback(sppCallback) != ESP_OK)
  {
    deactivateSppCallbackTarget(impl_);
    owner_->setError(
      EspBleError::BackendFailure, "failed to register SPP callback");
    return false;
  }
  esp_spp_cfg_t config = BT_SPP_DEFAULT_CONFIG();
  config.mode = ESP_SPP_MODE_CB;
  if (esp_spp_enhanced_init(&config) != ESP_OK)
  {
    deactivateSppCallbackTarget(impl_);
    owner_->setError(EspBleError::BackendFailure, "failed to initialize SPP");
    return false;
  }
  const uint32_t deadline = millis() + 2000;
  while (true)
  {
    bool completed = false;
    bool initialized = false;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      completed = impl_->initializationCompleted;
      initialized = impl_->initialized;
    }
    if (completed)
    {
      if (initialized) return true;
      deactivateSppCallbackTarget(impl_);
      owner_->setError(
        EspBleError::BackendFailure, "SPP initialization failed");
      return false;
    }
    if (static_cast<int32_t>(millis() - deadline) >= 0)
    {
      deactivateSppCallbackTarget(impl_);
      owner_->setError(EspBleError::Timeout, "SPP initialization timed out");
      return false;
    }
    delay(1);
  }
#endif
}

void EspBleClassicSpp::end()
{
  if (impl_ == nullptr) return;
#if ESPBLE_CLASSIC_BACKEND_AVAILABLE
  bool initialized = false;
  bool serverRunning = false;
  uint32_t handle = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = true;
    initialized = impl_->initialized;
    serverRunning = impl_->serverRunning;
    handle = impl_->backendHandle;
  }
  if (handle != 0) esp_spp_disconnect(handle);
  if (serverRunning) esp_spp_stop_srv();
  if (initialized)
  {
    esp_spp_deinit();
    const uint32_t deadline = millis() + 2000;
    while (true)
    {
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->initialized) break;
      }
      if (static_cast<int32_t>(millis() - deadline) >= 0) break;
      delay(1);
    }
  }
  deactivateSppCallbackTarget(impl_);
#endif
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->eventHead = 0;
  impl_->eventCount = 0;
  impl_->droppedEvents = 0;
  impl_->initialized = false;
  impl_->initializationCompleted = false;
  impl_->ending = false;
  impl_->serverStartPending = false;
  impl_->serverRunning = false;
  impl_->serverName = "";
  impl_->serverChannel = 0;
  impl_->backendHandle = 0;
  impl_->activeSession = EspBleClassicSppSession();
  impl_->nextSessionId = 1;
  for (String &value : impl_->txQueue) value = "";
  impl_->txHead = 0;
  impl_->txCount = 0;
  impl_->droppedWrites = 0;
  impl_->txInFlight = false;
  impl_->txCongested = false;
  impl_->rxHead = 0;
  impl_->rxCount = 0;
  impl_->droppedReceiveBytes = 0;
  impl_->connecting = false;
  impl_->connectAddress = "";
  memset(impl_->connectBackendAddress, 0, sizeof(impl_->connectBackendAddress));
  impl_->connectDeadlineMs = 0;
}

void EspBleClassicSpp::onServerStarted(ServerStartedCallback callback)
{
  serverStartedCallback_ = std::move(callback);
}

void EspBleClassicSpp::onConnected(SessionCallback callback)
{
  connectedCallback_ = std::move(callback);
}

void EspBleClassicSpp::onDisconnected(SessionCallback callback)
{
  disconnectedCallback_ = std::move(callback);
}

void EspBleClassicSpp::onData(DataCallback callback)
{
  dataCallback_ = std::move(callback);
}

void EspBleClassicSpp::onWriteCompleted(WriteCompletedCallback callback)
{
  writeCompletedCallback_ = std::move(callback);
}

void EspBleClassicSpp::onConnectionFailed(ConnectionFailureCallback callback)
{
  connectionFailureCallback_ = std::move(callback);
}

bool EspBleClassicSpp::startServer(
  const EspBleClassicSppServerConfig &config)
{
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "Classic stack is not initialized");
    return false;
  }
  if (
    config.serviceName == nullptr || config.serviceName[0] == '\0' ||
    config.channel > 30)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid SPP server configuration");
    return false;
  }
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  owner_->setError(EspBleError::BackendFailure, "Classic SPP is unavailable");
  return false;
#else
  if (!begin()) return false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->serverStartPending || impl_->serverRunning)
    {
      owner_->setError(EspBleError::InvalidState, "SPP server is already active");
      return false;
    }
    impl_->serverName = config.serviceName;
    impl_->serverChannel = config.channel;
    impl_->serverStartPending = true;
  }
  startPendingServer(impl_);
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicSpp::stopServer()
{
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "Classic stack is not initialized");
    return false;
  }
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  owner_->setError(EspBleError::BackendFailure, "Classic SPP is unavailable");
  return false;
#else
  if (impl_ == nullptr)
  {
    owner_->clearError();
    return true;
  }
  bool running = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->serverStartPending)
    {
      impl_->serverStartPending = false;
      owner_->clearError();
      return true;
    }
    running = impl_->serverRunning;
  }
  if (running && esp_spp_stop_srv() != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure, "failed to stop SPP server");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicSpp::serverRunning() const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->serverRunning;
}

bool EspBleClassicSpp::connect(
  const char *address, uint32_t timeoutMilliseconds)
{
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "Classic stack is not initialized");
    return false;
  }
  if (timeoutMilliseconds == 0)
  {
    owner_->setError(EspBleError::InvalidArgument, "SPP timeout must be nonzero");
    return false;
  }
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  (void)address;
  owner_->setError(EspBleError::BackendFailure, "Classic SPP is unavailable");
  return false;
#else
  if (!begin()) return false;
  esp_bd_addr_t backendAddress = {};
  if (!parseAddress(address, backendAddress))
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid Classic address");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->connecting || impl_->backendHandle != 0)
    {
      owner_->setError(EspBleError::InvalidState, "an SPP connection is already active");
      return false;
    }
    impl_->connecting = true;
    impl_->connectAddress = address;
    memcpy(impl_->connectBackendAddress, backendAddress, sizeof(backendAddress));
    impl_->connectDeadlineMs = millis() + timeoutMilliseconds;
  }
  if (esp_spp_start_discovery(backendAddress) != ESP_OK)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->connecting = false;
    impl_->connectAddress = "";
    impl_->connectDeadlineMs = 0;
    owner_->setError(EspBleError::BackendFailure, "failed to start SPP discovery");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicSpp::disconnect(EspBleClassicSppSessionId sessionId)
{
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  (void)sessionId;
  owner_->setError(EspBleError::BackendFailure, "Classic SPP is unavailable");
  return false;
#else
  if (impl_ == nullptr)
  {
    owner_->setError(EspBleError::NotFound, "SPP session was not found");
    return false;
  }
  uint32_t handle = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->backendHandle == 0 || impl_->activeSession.id != sessionId)
    {
      owner_->setError(EspBleError::NotFound, "SPP session was not found");
      return false;
    }
    handle = impl_->backendHandle;
  }
  if (esp_spp_disconnect(handle) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure, "failed to disconnect SPP session");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

size_t EspBleClassicSpp::sessionCount() const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->backendHandle == 0 ? 0 : 1;
}

bool EspBleClassicSpp::session(
  EspBleClassicSppSessionId sessionId,
  EspBleClassicSppSession &session) const
{
  if (impl_ == nullptr || sessionId == 0) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->backendHandle == 0 || impl_->activeSession.id != sessionId) return false;
  session = impl_->activeSession;
  return true;
}

bool EspBleClassicSpp::write(
  EspBleClassicSppSessionId sessionId,
  const uint8_t *data, size_t length)
{
  if (data == nullptr || length == 0 || length > MaximumWriteSize)
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid SPP write length");
    return false;
  }
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  (void)sessionId;
  owner_->setError(EspBleError::BackendFailure, "Classic SPP is unavailable");
  return false;
#else
  if (impl_ == nullptr)
  {
    owner_->setError(EspBleError::NotFound, "SPP session was not found");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->backendHandle == 0 || impl_->activeSession.id != sessionId)
    {
      owner_->setError(EspBleError::NotFound, "SPP session was not found");
      return false;
    }
    if (impl_->txCount == WriteQueueCapacity)
    {
      ++impl_->droppedWrites;
      owner_->setError(EspBleError::ResourceExhausted, "SPP write queue is full");
      return false;
    }
    const size_t tail = (impl_->txHead + impl_->txCount) % WriteQueueCapacity;
    impl_->txQueue[tail] = String(reinterpret_cast<const char *>(data), length);
    if (impl_->txQueue[tail].length() != length)
    {
      impl_->txQueue[tail] = "";
      owner_->setError(EspBleError::ResourceExhausted, "failed to copy SPP data");
      return false;
    }
    ++impl_->txCount;
  }
  startNextWrite(impl_);
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicSpp::write(
  EspBleClassicSppSessionId sessionId, const String &value)
{
  return write(
    sessionId, reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
}

size_t EspBleClassicSpp::pendingWriteCount(
  EspBleClassicSppSessionId sessionId) const
{
  if (impl_ == nullptr || sessionId == 0) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->backendHandle == 0 || impl_->activeSession.id != sessionId) return 0;
  return impl_->txCount;
}

size_t EspBleClassicSpp::droppedWriteCount() const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedWrites;
}

size_t EspBleClassicSpp::available(EspBleClassicSppSessionId sessionId) const
{
  if (impl_ == nullptr || sessionId == 0) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->backendHandle == 0 || impl_->activeSession.id != sessionId) return 0;
  return impl_->rxCount;
}

int EspBleClassicSpp::peek(EspBleClassicSppSessionId sessionId) const
{
  if (impl_ == nullptr || sessionId == 0) return -1;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (
    impl_->backendHandle == 0 || impl_->activeSession.id != sessionId ||
    impl_->rxCount == 0)
  {
    return -1;
  }
  return impl_->rxBuffer[impl_->rxHead];
}

int EspBleClassicSpp::read(EspBleClassicSppSessionId sessionId)
{
  if (impl_ == nullptr || sessionId == 0) return -1;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (
    impl_->backendHandle == 0 || impl_->activeSession.id != sessionId ||
    impl_->rxCount == 0)
  {
    return -1;
  }
  const int value = impl_->rxBuffer[impl_->rxHead];
  impl_->rxHead = (impl_->rxHead + 1) % ReceiveBufferCapacity;
  --impl_->rxCount;
  return value;
}

size_t EspBleClassicSpp::read(
  EspBleClassicSppSessionId sessionId, uint8_t *data, size_t length)
{
  if (impl_ == nullptr || sessionId == 0 || data == nullptr || length == 0)
    return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->backendHandle == 0 || impl_->activeSession.id != sessionId) return 0;
  const size_t count = std::min(length, impl_->rxCount);
  for (size_t index = 0; index < count; ++index)
  {
    data[index] = impl_->rxBuffer[impl_->rxHead];
    impl_->rxHead = (impl_->rxHead + 1) % ReceiveBufferCapacity;
  }
  impl_->rxCount -= count;
  return count;
}

size_t EspBleClassicSpp::droppedReceiveByteCount() const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedReceiveBytes;
}

size_t EspBleClassicSpp::droppedEventCount() const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedEvents;
}

void EspBleClassicSpp::update()
{
  if (impl_ == nullptr) return;
#if ESPBLE_CLASSIC_BACKEND_AVAILABLE
  bool timedOut = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    timedOut = impl_->connecting &&
      static_cast<int32_t>(millis() - impl_->connectDeadlineMs) >= 0;
  }
  if (timedOut)
    failConnection(impl_, EspBleError::Timeout, "SPP connection timed out");
#endif
  while (true)
  {
    EspBleClassicSppImpl::Event event;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->eventCount == 0) break;
      event = std::move(impl_->events[impl_->eventHead]);
      impl_->eventHead = (impl_->eventHead + 1) % EventQueueCapacity;
      --impl_->eventCount;
    }
    switch (event.type)
    {
      case EspBleClassicSppImpl::EventType::ServerStarted:
        if (serverStartedCallback_) serverStartedCallback_();
        break;
      case EspBleClassicSppImpl::EventType::Connected:
        if (connectedCallback_) connectedCallback_(event.session);
        break;
      case EspBleClassicSppImpl::EventType::Disconnected:
        if (disconnectedCallback_) disconnectedCallback_(event.session);
        break;
      case EspBleClassicSppImpl::EventType::Data:
        if (dataCallback_) dataCallback_(event.data);
        break;
      case EspBleClassicSppImpl::EventType::WriteCompleted:
        if (writeCompletedCallback_) writeCompletedCallback_(event.writeResult);
        break;
      case EspBleClassicSppImpl::EventType::ConnectionFailed:
        if (connectionFailureCallback_) connectionFailureCallback_(event.failure);
        break;
    }
  }
}

EspBleClassicInquiry::EspBleClassicInquiry(EspBleClassic *owner) :
  owner_(owner) {}

EspBleClassicInquiry::~EspBleClassicInquiry()
{
  end();
  delete impl_;
}

bool EspBleClassicInquiry::begin()
{
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  return false;
#else
  if (impl_ == nullptr) impl_ = new (std::nothrow) EspBleClassicInquiryImpl();
  if (impl_ == nullptr) return false;
  activeInquiry.store(impl_, std::memory_order_release);
  return true;
#endif
}

void EspBleClassicInquiry::end()
{
  if (impl_ == nullptr) return;
#if ESPBLE_CLASSIC_BACKEND_AVAILABLE
  // Stop the callback path before cancelling, so a result that is already in
  // flight cannot land in a queue nobody will drain.
  activeInquiry.store(nullptr, std::memory_order_release);
  bool running = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    running = impl_->running;
  }
  if (running) (void)esp_bt_gap_cancel_discovery();
#endif
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->head = 0;
  impl_->count = 0;
  impl_->dropped = 0;
  impl_->running = false;
  impl_->stopRequested = false;
  impl_->completionPending = false;
  impl_->completionCancelled = false;
}

bool EspBleClassicInquiry::start(const EspBleClassicInquiryConfig &config)
{
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  (void)config;
  owner_->setError(
    EspBleError::BackendFailure,
    "Classic is supported only by the original ESP32 build");
  return false;
#else
  if (!owner_->initialized() || impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidState, "Classic stack is not started");
    return false;
  }
  // The controller encodes the duration in 1.28 s units and one byte, so 61
  // seconds is the longest scan it can be asked for.
  if (config.durationSeconds == 0 || config.durationSeconds > 61)
  {
    owner_->setError(
      EspBleError::InvalidArgument,
      "inquiry duration must be between 1 and 61 seconds");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->running)
    {
      owner_->setError(EspBleError::InvalidState, "inquiry is already running");
      return false;
    }
    impl_->head = 0;
    impl_->count = 0;
    impl_->dropped = 0;
    impl_->stopRequested = false;
    impl_->completionPending = false;
    impl_->completionCancelled = false;
    impl_->running = true;
  }
  const uint8_t durationUnits = static_cast<uint8_t>(
    (static_cast<uint64_t>(config.durationSeconds) * 100 + 127) / 128);
  if (esp_bt_gap_start_discovery(
        ESP_BT_INQ_MODE_GENERAL_INQUIRY, durationUnits,
        config.maxResponses) != ESP_OK)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->running = false;
    owner_->setError(EspBleError::BackendFailure, "failed to start inquiry");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicInquiry::stop()
{
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  owner_->setError(
    EspBleError::BackendFailure,
    "Classic is supported only by the original ESP32 build");
  return false;
#else
  if (!owner_->initialized() || impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidState, "Classic stack is not started");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->running)
    {
      owner_->setError(EspBleError::InvalidState, "inquiry is not running");
      return false;
    }
    // Remembered so the completion event can say it was cancelled rather than
    // that the duration elapsed.
    impl_->stopRequested = true;
  }
  if (esp_bt_gap_cancel_discovery() != ESP_OK)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->stopRequested = false;
    owner_->setError(EspBleError::BackendFailure, "failed to stop inquiry");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicInquiry::running() const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->running;
}

void EspBleClassicInquiry::onResult(ResultCallback callback)
{
  resultCallback_ = std::move(callback);
}

void EspBleClassicInquiry::onComplete(CompleteCallback callback)
{
  completeCallback_ = std::move(callback);
}

size_t EspBleClassicInquiry::droppedResultCount() const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->dropped;
}

void EspBleClassicInquiry::update()
{
  if (impl_ == nullptr) return;
  for (;;)
  {
    EspBleClassicInquiryResult result;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->count == 0) break;
      result = std::move(impl_->queue[impl_->head]);
      impl_->head = (impl_->head + 1) % InquiryQueueCapacity;
      --impl_->count;
    }
    if (resultCallback_) resultCallback_(result);
  }

  bool completed = false;
  EspBleClassicInquiryComplete completion;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->completionPending)
    {
      impl_->completionPending = false;
      completion.cancelled = impl_->completionCancelled;
      completed = true;
    }
  }
  if (completed && completeCallback_) completeCallback_(completion);
}

EspBleClassic::EspBleClassic() :
  inquiry_(this), spp_(this), hidDevice_(this), hidHost_(this), a2dpSink_(this),
  a2dpSource_(this), avrcp_(this), hfpClient_(this),
  hfpAudioGateway_(this) {}

EspBleClassic::~EspBleClassic()
{
  end();
  delete impl_;
}

bool EspBleClassic::begin(const EspBleClassicConfig &config)
{
  const char *deviceName = config.deviceName == nullptr ? "" : config.deviceName;
  if (deviceName[0] == '\0')
  {
    setError(EspBleError::InvalidArgument, "Classic device name must not be empty");
    return false;
  }
  if (initialized())
  {
    if (impl_->deviceName != deviceName)
    {
      setError(EspBleError::InvalidState, "Classic stack already uses another name");
      return false;
    }
    clearError();
    return true;
  }
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  setError(
    EspBleError::BackendFailure,
    "Classic is supported only by the original ESP32 build");
  return false;
#else
  EspBleClassic *expected = nullptr;
  if (!activeClassic.compare_exchange_strong(expected, this))
  {
    setError(EspBleError::InvalidState, "another Classic owner is active");
    return false;
  }
  const bool controllerStarted = btStarted();
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
  // A running controller the broker already owns belongs to a BLE host that
  // started first; attach to it instead of refusing to start.
  const bool attachAdoptedController = controllerStarted &&
    espble_hci_broker_has_adopted_controller();
#else
  const bool attachAdoptedController = false;
#endif
  if ((controllerStarted && !attachAdoptedController) ||
      esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_UNINITIALIZED)
  {
    activeClassic.store(nullptr, std::memory_order_release);
    setError(EspBleError::InvalidState, "another Bluetooth stack is active");
    return false;
  }
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
  if (!attachAdoptedController && !btStartMode(controllerStartMode()))
#else
  if (!attachAdoptedController && !btStartMode(BT_MODE_CLASSIC_BT))
#endif
  {
    activeClassic.store(nullptr, std::memory_order_release);
    setError(
      EspBleError::BackendFailure,
      "failed to start the Classic controller; Classic memory may already be released");
    return false;
  }
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
  if (!attachAdoptedController &&
      espble_hci_broker_adopt_controller(stopAdoptedController) != ESP_OK)
  {
    btStop();
    activeClassic.store(nullptr, std::memory_order_release);
    setError(EspBleError::InvalidState, "failed to transfer controller ownership");
    return false;
  }
#endif
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
  if (!attachClassicHost())
  {
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
    shutdownAdoptedController();
#else
    btStop();
#endif
    activeClassic.store(nullptr, std::memory_order_release);
    setError(EspBleError::BackendFailure, "failed to attach the custom Classic host");
    return false;
  }
#endif
  if (esp_bluedroid_init() != ESP_OK || esp_bluedroid_enable() != ESP_OK)
  {
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED)
      esp_bluedroid_disable();
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED)
      esp_bluedroid_deinit();
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
    detachClassicHost();
#endif
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
    shutdownAdoptedController();
#else
    btStop();
#endif
    activeClassic.store(nullptr, std::memory_order_release);
    setError(EspBleError::BackendFailure, "failed to initialize Bluedroid");
    return false;
  }
  if (
    esp_bt_gap_register_callback(classicGapCallback) != ESP_OK ||
    esp_bt_gap_set_device_name(deviceName) != ESP_OK)
  {
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
    detachClassicHost();
#endif
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
    shutdownAdoptedController();
#else
    btStop();
#endif
    activeClassic.store(nullptr, std::memory_order_release);
    setError(EspBleError::BackendFailure, "failed to initialize Classic GAP");
    return false;
  }
  esp_bt_io_cap_t ioCapability = ESP_BT_IO_CAP_NONE;
  if (config.security.enabled)
  {
    switch (config.security.ioCapability)
    {
      case EspBleClassicSecurityIoCapability::DisplayOnly:
        ioCapability = ESP_BT_IO_CAP_OUT;
        break;
      case EspBleClassicSecurityIoCapability::KeyboardOnly:
        ioCapability = ESP_BT_IO_CAP_IN;
        break;
      case EspBleClassicSecurityIoCapability::DisplayYesNo:
        ioCapability = ESP_BT_IO_CAP_IO;
        break;
      case EspBleClassicSecurityIoCapability::None:
        break;
    }
  }
  if (
    esp_bt_gap_set_security_param(
      ESP_BT_SP_IOCAP_MODE, &ioCapability, sizeof(ioCapability)) != ESP_OK)
  {
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
    detachClassicHost();
#endif
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
    shutdownAdoptedController();
#else
    btStop();
#endif
    activeClassic.store(nullptr, std::memory_order_release);
    setError(EspBleError::BackendFailure, "failed to configure Classic security");
    return false;
  }
  esp_bt_pin_code_t pin = {};
  (void)esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, pin);
  if (impl_ == nullptr) impl_ = new (std::nothrow) EspBleClassicImpl();
  if (impl_ == nullptr)
  {
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
    detachClassicHost();
#endif
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
    shutdownAdoptedController();
#else
    btStop();
#endif
    activeClassic.store(nullptr, std::memory_order_release);
    setError(EspBleError::ResourceExhausted, "failed to allocate Classic state");
    return false;
  }
  impl_->deviceName = deviceName;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->security = config.security;
    impl_->eventHead = 0;
    impl_->eventCount = 0;
    impl_->droppedEvents = 0;
    impl_->numericComparisonPending = false;
    impl_->passkeyPending = false;
    impl_->numericComparisonDeadlineMs = 0;
    impl_->passkeyDeadlineMs = 0;
    impl_->numericComparisonCallbackConfigured =
      static_cast<bool>(numericComparisonCallback_);
    impl_->passkeyRequestedCallbackConfigured =
      static_cast<bool>(passkeyRequestedCallback_);
  }
#if ESPBLE_CLASSIC_BACKEND_AVAILABLE
  activeClassicImpl.store(impl_, std::memory_order_release);
#endif
  impl_->initialized = true;
  // Inquiry shares the GAP callback the stack just registered, so it can only
  // be armed once the stack is up.
  (void)inquiry_.begin();
  clearError();
  return true;
#endif
}

void EspBleClassic::end()
{
  if (!initialized()) return;
#if ESPBLE_CLASSIC_BACKEND_AVAILABLE
  activeClassicImpl.store(nullptr, std::memory_order_release);
#endif
  inquiry_.end();
  hfpAudioGateway_.end();
  hfpClient_.end();
  avrcp_.end();
  a2dpSink_.end();
  a2dpSource_.end();
  hidHost_.end();
  hidDevice_.end();
  spp_.end();
#if ESPBLE_CLASSIC_BACKEND_AVAILABLE
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED)
    esp_bluedroid_disable();
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED)
    esp_bluedroid_deinit();
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
  detachClassicHost();
#endif
#if defined(ESPBLE_CLASSIC_CUSTOM_HOST)
  // The broker stops the adopted controller now if Classic was the final
  // host, or defers it until NimBLE unregisters.
  shutdownAdoptedController();
#else
  btStop();
#endif
  activeClassic.store(nullptr, std::memory_order_release);
#endif
  impl_->initialized = false;
  impl_->deviceName = "";
  clearError();
}

void EspBleClassic::onSecurityChanged(SecurityChangedCallback callback)
{
  securityChangedCallback_ = std::move(callback);
}

void EspBleClassic::onNumericComparisonRequested(
  NumericComparisonCallback callback)
{
  numericComparisonCallback_ = std::move(callback);
  if (impl_ == nullptr) return;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->numericComparisonCallbackConfigured =
    static_cast<bool>(numericComparisonCallback_);
}

void EspBleClassic::onPasskeyDisplayed(PasskeyDisplayedCallback callback)
{
  passkeyDisplayedCallback_ = std::move(callback);
}

void EspBleClassic::onPasskeyRequested(PasskeyRequestedCallback callback)
{
  passkeyRequestedCallback_ = std::move(callback);
  if (impl_ == nullptr) return;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->passkeyRequestedCallbackConfigured =
    static_cast<bool>(passkeyRequestedCallback_);
}

bool EspBleClassic::confirmNumericComparison(
  const char *peerAddress, bool accept)
{
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  (void)peerAddress;
  (void)accept;
  setError(
    EspBleError::BackendFailure,
    "Classic is supported only by the original ESP32 build");
  return false;
#else
  if (!initialized())
  {
    setError(EspBleError::InvalidState, "Classic stack is not started");
    return false;
  }
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    // The answer has to name the peer it answers for: two requests can be
    // outstanding in a sketch that reconnects while pairing.
    if (!impl_->numericComparisonPending ||
        (peerAddress != nullptr &&
         impl_->numericComparisonAddress != peerAddress))
    {
      setError(
        EspBleError::InvalidState, "no numeric comparison is waiting");
      return false;
    }
    memcpy(address, impl_->numericComparisonBackendAddress, sizeof(address));
    impl_->numericComparisonPending = false;
    impl_->numericComparisonAddress = "";
    impl_->numericComparisonDeadlineMs = 0;
  }
  if (esp_bt_gap_ssp_confirm_reply(address, accept) != ESP_OK)
  {
    setError(EspBleError::BackendFailure, "failed to answer the comparison");
    return false;
  }
  clearError();
  return true;
#endif
}

bool EspBleClassic::providePasskey(const char *peerAddress, uint32_t passkey)
{
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  (void)peerAddress;
  (void)passkey;
  setError(
    EspBleError::BackendFailure,
    "Classic is supported only by the original ESP32 build");
  return false;
#else
  if (!initialized())
  {
    setError(EspBleError::InvalidState, "Classic stack is not started");
    return false;
  }
  if (passkey > 999999)
  {
    setError(EspBleError::InvalidArgument, "passkey must be six digits");
    return false;
  }
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->passkeyPending ||
        (peerAddress != nullptr && impl_->passkeyAddress != peerAddress))
    {
      setError(EspBleError::InvalidState, "no passkey is waiting");
      return false;
    }
    memcpy(address, impl_->passkeyBackendAddress, sizeof(address));
    impl_->passkeyPending = false;
    impl_->passkeyAddress = "";
    impl_->passkeyDeadlineMs = 0;
  }
  if (esp_bt_gap_ssp_passkey_reply(address, true, passkey) != ESP_OK)
  {
    setError(EspBleError::BackendFailure, "failed to answer the passkey");
    return false;
  }
  clearError();
  return true;
#endif
}

size_t EspBleClassic::bondCount() const
{
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  return 0;
#else
  if (!initialized()) return 0;
  const int count = esp_bt_gap_get_bond_device_num();
  return count > 0 ? static_cast<size_t>(count) : 0;
#endif
}

bool EspBleClassic::bond(size_t index, EspBleClassicBond &bond) const
{
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  (void)index;
  (void)bond;
  return false;
#else
  if (!initialized()) return false;
  const int total = esp_bt_gap_get_bond_device_num();
  if (total <= 0 || index >= static_cast<size_t>(total)) return false;
  // The list is read whole because the backend has no indexed accessor.
  int count = total;
  esp_bd_addr_t *addresses = static_cast<esp_bd_addr_t *>(
    malloc(sizeof(esp_bd_addr_t) * static_cast<size_t>(total)));
  if (addresses == nullptr) return false;
  const bool read =
    esp_bt_gap_get_bond_device_list(&count, addresses) == ESP_OK &&
    index < static_cast<size_t>(count);
  if (read) bond.peerAddress = formatAddress(addresses[index]);
  free(addresses);
  return read;
#endif
}

bool EspBleClassic::deleteBond(const EspBleClassicBond &bond)
{
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  (void)bond;
  setError(
    EspBleError::BackendFailure,
    "Classic is supported only by the original ESP32 build");
  return false;
#else
  if (!initialized())
  {
    setError(EspBleError::InvalidState, "Classic stack is not started");
    return false;
  }
  esp_bd_addr_t address = {};
  if (!parseAddress(bond.peerAddress.c_str(), address))
  {
    setError(EspBleError::InvalidArgument, "invalid bond address");
    return false;
  }
  if (esp_bt_gap_remove_bond_device(address) != ESP_OK)
  {
    setError(EspBleError::NotFound, "no such bond");
    return false;
  }
  clearError();
  return true;
#endif
}

bool EspBleClassic::deleteAllBonds()
{
#if !ESPBLE_CLASSIC_BACKEND_AVAILABLE
  setError(
    EspBleError::BackendFailure,
    "Classic is supported only by the original ESP32 build");
  return false;
#else
  if (!initialized())
  {
    setError(EspBleError::InvalidState, "Classic stack is not started");
    return false;
  }
  bool removedAll = true;
  // Removing an entry renumbers the list, so this always deletes the first.
  for (EspBleClassicBond entry; bond(0, entry);)
  {
    if (!deleteBond(entry))
    {
      removedAll = false;
      break;
    }
  }
  if (removedAll) clearError();
  return removedAll;
#endif
}

void EspBleClassic::update()
{
  if (impl_ != nullptr)
  {
    // A pairing nobody answers must not leave the peer waiting forever.
    uint8_t timedOutComparison[6] = {};
    uint8_t timedOutPasskey[6] = {};
    bool rejectComparison = false;
    bool rejectPasskey = false;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->numericComparisonPending &&
          static_cast<int32_t>(millis() - impl_->numericComparisonDeadlineMs) >= 0)
      {
        memcpy(timedOutComparison, impl_->numericComparisonBackendAddress,
          sizeof(timedOutComparison));
        impl_->numericComparisonPending = false;
        impl_->numericComparisonAddress = "";
        impl_->numericComparisonDeadlineMs = 0;
        rejectComparison = true;
      }
      if (impl_->passkeyPending &&
          static_cast<int32_t>(millis() - impl_->passkeyDeadlineMs) >= 0)
      {
        memcpy(timedOutPasskey, impl_->passkeyBackendAddress,
          sizeof(timedOutPasskey));
        impl_->passkeyPending = false;
        impl_->passkeyAddress = "";
        impl_->passkeyDeadlineMs = 0;
        rejectPasskey = true;
      }
    }
#if ESPBLE_CLASSIC_BACKEND_AVAILABLE
    if (rejectComparison)
      (void)esp_bt_gap_ssp_confirm_reply(timedOutComparison, false);
    if (rejectPasskey)
      (void)esp_bt_gap_ssp_passkey_reply(timedOutPasskey, false, 0);
#else
    (void)rejectComparison;
    (void)rejectPasskey;
#endif

    for (;;)
    {
      EspBleClassicImpl::Event event;
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->eventCount == 0) break;
        event = std::move(impl_->events[impl_->eventHead]);
        impl_->eventHead = (impl_->eventHead + 1) % SecurityEventQueueCapacity;
        --impl_->eventCount;
      }
      switch (event.type)
      {
        case EspBleClassicImpl::EventType::SecurityChanged:
          if (securityChangedCallback_)
            securityChangedCallback_(event.securityChanged);
          break;
        case EspBleClassicImpl::EventType::NumericComparison:
          if (numericComparisonCallback_)
            numericComparisonCallback_(event.numericComparison);
          break;
        case EspBleClassicImpl::EventType::PasskeyDisplayed:
          if (passkeyDisplayedCallback_)
            passkeyDisplayedCallback_(event.passkeyDisplayed);
          break;
        case EspBleClassicImpl::EventType::PasskeyRequested:
          if (passkeyRequestedCallback_)
            passkeyRequestedCallback_(event.passkeyRequested);
          break;
      }
    }
  }

  inquiry_.update();
  spp_.update();
  hidDevice_.update();
  hidHost_.update();
  a2dpSink_.update();
  a2dpSource_.update();
  avrcp_.update();
  hfpClient_.update();
  hfpAudioGateway_.update();
}

bool EspBleClassic::initialized() const
{
  return impl_ != nullptr && impl_->initialized;
}

EspBleClassicInquiry &EspBleClassic::inquiry()
{
  return inquiry_;
}

EspBleClassicSpp &EspBleClassic::spp()
{
  return spp_;
}

EspBleClassicHidDevice &EspBleClassic::hidDevice()
{
  return hidDevice_;
}

EspBleClassicHidHost &EspBleClassic::hidHost()
{
  return hidHost_;
}

EspBleClassicA2dpSink &EspBleClassic::a2dpSink()
{
  return a2dpSink_;
}

EspBleClassicA2dpSource &EspBleClassic::a2dpSource()
{
  return a2dpSource_;
}

EspBleClassicAvrcp &EspBleClassic::avrcp()
{
  return avrcp_;
}

EspBleClassicHfpClient &EspBleClassic::hfpClient()
{
  return hfpClient_;
}

EspBleClassicHfpAudioGateway &EspBleClassic::hfpAudioGateway()
{
  return hfpAudioGateway_;
}

EspBleError EspBleClassic::lastError() const
{
  return lastError_;
}

const char *EspBleClassic::lastErrorName() const
{
  return errorName(lastError_);
}

const String &EspBleClassic::lastErrorDetail() const
{
  return lastErrorDetail_;
}

void EspBleClassic::clearError()
{
  lastError_ = EspBleError::None;
  lastErrorDetail_ = "";
}

void EspBleClassic::setError(EspBleError error, const char *detail)
{
  lastError_ = error;
  lastErrorDetail_ = detail == nullptr ? "" : detail;
}
