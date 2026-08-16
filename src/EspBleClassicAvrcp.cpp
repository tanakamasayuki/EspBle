#include "EspBleClassic.h"
#include "EspBleClassicBuild.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <utility>

#if defined(CONFIG_IDF_TARGET_ESP32) && \
  defined(ESPBLE_CLASSIC_CUSTOM_HOST) && \
  (defined(ESPBLE_CLASSIC_ONLY) || defined(ESPBLE_ENABLE_CLASSIC))
#define ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE 1
#define esp_avrc_ct_deinit espble_bd_esp_avrc_ct_deinit
#define esp_avrc_ct_init espble_bd_esp_avrc_ct_init
#define esp_avrc_ct_register_callback espble_bd_esp_avrc_ct_register_callback
#define esp_avrc_ct_send_get_play_status_cmd \
  espble_bd_esp_avrc_ct_send_get_play_status_cmd
#define esp_avrc_ct_send_get_rn_capabilities_cmd \
  espble_bd_esp_avrc_ct_send_get_rn_capabilities_cmd
#define esp_avrc_ct_send_metadata_cmd espble_bd_esp_avrc_ct_send_metadata_cmd
#define esp_avrc_ct_send_passthrough_cmd \
  espble_bd_esp_avrc_ct_send_passthrough_cmd
#define esp_avrc_ct_send_set_player_value_cmd \
  espble_bd_esp_avrc_ct_send_set_player_value_cmd
#define esp_avrc_ct_send_register_notification_cmd \
  espble_bd_esp_avrc_ct_send_register_notification_cmd
#define esp_avrc_ct_send_set_absolute_volume_cmd \
  espble_bd_esp_avrc_ct_send_set_absolute_volume_cmd
#define esp_avrc_rn_evt_bit_mask_operation \
  espble_bd_esp_avrc_rn_evt_bit_mask_operation
#define esp_avrc_tg_deinit espble_bd_esp_avrc_tg_deinit
#define esp_avrc_tg_get_psth_cmd_filter \
  espble_bd_esp_avrc_tg_get_psth_cmd_filter
#define esp_avrc_tg_init espble_bd_esp_avrc_tg_init
#define esp_avrc_tg_register_callback espble_bd_esp_avrc_tg_register_callback
#define esp_avrc_tg_send_rn_rsp espble_bd_esp_avrc_tg_send_rn_rsp
#define esp_avrc_tg_set_psth_cmd_filter \
  espble_bd_esp_avrc_tg_set_psth_cmd_filter
#define esp_avrc_tg_get_rn_evt_cap espble_bd_esp_avrc_tg_get_rn_evt_cap
#define esp_avrc_tg_set_rn_evt_cap espble_bd_esp_avrc_tg_set_rn_evt_cap
#include "esp32/include/esp_avrc_api.h"
#else
#define ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE 0
#endif

namespace
{
constexpr size_t AvrcpEventQueueCapacity = 24;

#if ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
String avrcpAddress(const esp_bd_addr_t address)
{
  char value[18];
  snprintf(
    value, sizeof(value), "%02x:%02x:%02x:%02x:%02x:%02x",
    address[0], address[1], address[2], address[3], address[4], address[5]);
  return String(value);
}

bool waitForAvrcpFlag(
  const std::function<bool()> &condition,
  uint32_t timeoutMilliseconds = 3000)
{
  const uint32_t deadline = millis() + timeoutMilliseconds;
  while (!condition())
  {
    if (static_cast<int32_t>(millis() - deadline) >= 0) return false;
    delay(1);
  }
  return true;
}

String copyAvrcpText(const uint8_t *data, int length)
{
  String result;
  if (data == nullptr || length <= 0) return result;
  result.reserve(static_cast<unsigned>(length));
  for (int index = 0; index < length; ++index)
    result += static_cast<char>(data[index]);
  return result;
}
#endif
} // namespace

struct EspBleClassicAvrcpImpl
{
  enum class EventType : uint8_t
  {
    Connection,
    RemoteFeatures,
    Passthrough,
    PassthroughResponse,
    Metadata,
    PlayStatus,
    Volume,
    NotificationRegistered,
  };

  struct Event
  {
    EventType type = EventType::Connection;
    EspBleClassicAvrcpConnection connection;
    EspBleClassicAvrcpRemoteFeatures remoteFeatures;
    EspBleClassicAvrcpPassthrough passthrough;
    EspBleClassicAvrcpPassthroughResponse passthroughResponse;
    EspBleClassicAvrcpMetadata metadata;
    EspBleClassicAvrcpPlayStatus playStatus;
    EspBleClassicAvrcpVolume volume;
    EspBleClassicAvrcpNotification notification =
      EspBleClassicAvrcpNotification::PlayStatus;
  };

  bool enqueue(Event event)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (eventCount == AvrcpEventQueueCapacity)
    {
      ++droppedEvents;
      return false;
    }
    events[(eventHead + eventCount) % AvrcpEventQueueCapacity] =
      std::move(event);
    ++eventCount;
    return true;
  }

  uint8_t allocateTransactionLabel()
  {
    std::lock_guard<std::mutex> lock(mutex);
    const uint8_t result = nextTransactionLabel;
    nextTransactionLabel = (nextTransactionLabel + 1) & 0x0f;
    return result;
  }

  mutable std::mutex mutex;
  std::atomic<size_t> callbackUsers{0};
  Event events[AvrcpEventQueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  bool ending = false;
  bool controllerRequested = false;
  bool targetRequested = false;
  bool controllerStateCompleted = false;
  bool targetStateCompleted = false;
  bool controllerInitialized = false;
  bool targetInitialized = false;
  bool controllerConnected = false;
  bool targetConnected = false;
  bool volumeNotificationRegistered = false;
  uint8_t volume = 64;
  uint8_t nextTransactionLabel = 0;
};

#if ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
namespace
{
std::atomic<EspBleClassicAvrcpImpl *> activeAvrcp{nullptr};
std::mutex avrcpTargetMutex;

class AvrcpCallbackLease
{
public:
  AvrcpCallbackLease()
  {
    std::lock_guard<std::mutex> lock(avrcpTargetMutex);
    impl_ = activeAvrcp.load(std::memory_order_relaxed);
    if (impl_) impl_->callbackUsers.fetch_add(1, std::memory_order_acq_rel);
  }
  ~AvrcpCallbackLease()
  {
    if (impl_) impl_->callbackUsers.fetch_sub(1, std::memory_order_acq_rel);
  }
  EspBleClassicAvrcpImpl *get() const { return impl_; }
private:
  EspBleClassicAvrcpImpl *impl_ = nullptr;
};

bool activateAvrcp(EspBleClassicAvrcpImpl *impl)
{
  std::lock_guard<std::mutex> lock(avrcpTargetMutex);
  if (activeAvrcp.load(std::memory_order_relaxed) != nullptr) return false;
  activeAvrcp.store(impl, std::memory_order_release);
  return true;
}

void deactivateAvrcp(EspBleClassicAvrcpImpl *impl)
{
  {
    std::lock_guard<std::mutex> lock(avrcpTargetMutex);
    if (activeAvrcp.load(std::memory_order_relaxed) == impl)
      activeAvrcp.store(nullptr, std::memory_order_release);
  }
  while (impl->callbackUsers.load(std::memory_order_acquire) != 0) delay(1);
}

bool avrcpInitSucceeded(esp_avrc_init_state_t state)
{
  return state == ESP_AVRC_INIT_SUCCESS || state == ESP_AVRC_INIT_ALREADY;
}

bool avrcpDeinitSucceeded(esp_avrc_init_state_t state)
{
  return state == ESP_AVRC_DEINIT_SUCCESS || state == ESP_AVRC_DEINIT_ALREADY;
}

void avrcpControllerCallback(
  esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *parameter)
{
  AvrcpCallbackLease lease;
  EspBleClassicAvrcpImpl *impl = lease.get();
  if (!impl || !parameter) return;
  if (event == ESP_AVRC_CT_PROF_STATE_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    const esp_avrc_init_state_t state = parameter->avrc_ct_init_stat.state;
    if (avrcpInitSucceeded(state)) impl->controllerInitialized = true;
    else if (avrcpDeinitSucceeded(state)) impl->controllerInitialized = false;
    impl->controllerStateCompleted = true;
    return;
  }
  if (event == ESP_AVRC_CT_CONNECTION_STATE_EVT)
  {
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::Connection;
    queued.connection.controller = true;
    queued.connection.connected = parameter->conn_stat.connected;
    queued.connection.peerAddress = avrcpAddress(parameter->conn_stat.remote_bda);
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->ending) return;
      impl->controllerConnected = queued.connection.connected;
    }
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_AVRC_CT_REMOTE_FEATURES_EVT)
  {
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::RemoteFeatures;
    queued.remoteFeatures.controller = true;
    queued.remoteFeatures.peerAddress = avrcpAddress(parameter->rmt_feats.remote_bda);
    queued.remoteFeatures.featureMask = parameter->rmt_feats.feat_mask;
    queued.remoteFeatures.featureFlags = parameter->rmt_feats.tg_feat_flag;
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_AVRC_CT_PASSTHROUGH_RSP_EVT)
  {
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::PassthroughResponse;
    queued.passthroughResponse.transactionLabel = parameter->psth_rsp.tl;
    queued.passthroughResponse.command =
      static_cast<EspBleClassicAvrcpCommand>(parameter->psth_rsp.key_code);
    queued.passthroughResponse.state =
      static_cast<EspBleClassicAvrcpKeyState>(parameter->psth_rsp.key_state);
    queued.passthroughResponse.responseCode = parameter->psth_rsp.rsp_code;
    queued.passthroughResponse.accepted =
      parameter->psth_rsp.rsp_code == ESP_AVRC_RSP_ACCEPT ||
      parameter->psth_rsp.rsp_code == ESP_AVRC_RSP_IMPL_STBL;
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_AVRC_CT_METADATA_RSP_EVT)
  {
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::Metadata;
    queued.metadata.attribute = parameter->meta_rsp.attr_id;
    queued.metadata.value = copyAvrcpText(
      parameter->meta_rsp.attr_text, parameter->meta_rsp.attr_length);
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_AVRC_CT_PLAY_STATUS_RSP_EVT)
  {
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::PlayStatus;
    queued.playStatus.trackLengthMilliseconds =
      parameter->play_status_rsp.song_length;
    queued.playStatus.positionMilliseconds =
      parameter->play_status_rsp.song_position;
    queued.playStatus.state = static_cast<EspBleClassicAvrcpPlaybackState>(
      parameter->play_status_rsp.play_status);
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT)
  {
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::Volume;
    queued.volume.value = parameter->set_volume_rsp.volume;
    queued.volume.remoteCommand = false;
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_AVRC_CT_CHANGE_NOTIFY_EVT &&
      parameter->change_ntf.event_id == ESP_AVRC_RN_VOLUME_CHANGE)
  {
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::Volume;
    queued.volume.value = parameter->change_ntf.event_parameter.volume;
    queued.volume.remoteCommand = false;
    impl->enqueue(std::move(queued));
  }
}

void avrcpTargetCallback(
  esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *parameter)
{
  AvrcpCallbackLease lease;
  EspBleClassicAvrcpImpl *impl = lease.get();
  if (!impl || !parameter) return;
  if (event == ESP_AVRC_TG_PROF_STATE_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    const esp_avrc_init_state_t state = parameter->avrc_tg_init_stat.state;
    if (avrcpInitSucceeded(state)) impl->targetInitialized = true;
    else if (avrcpDeinitSucceeded(state)) impl->targetInitialized = false;
    impl->targetStateCompleted = true;
    return;
  }
  if (event == ESP_AVRC_TG_CONNECTION_STATE_EVT)
  {
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::Connection;
    queued.connection.controller = false;
    queued.connection.connected = parameter->conn_stat.connected;
    queued.connection.peerAddress = avrcpAddress(parameter->conn_stat.remote_bda);
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->ending) return;
      impl->targetConnected = queued.connection.connected;
      if (!queued.connection.connected)
        impl->volumeNotificationRegistered = false;
    }
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_AVRC_TG_REMOTE_FEATURES_EVT)
  {
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::RemoteFeatures;
    queued.remoteFeatures.controller = false;
    queued.remoteFeatures.peerAddress = avrcpAddress(parameter->rmt_feats.remote_bda);
    queued.remoteFeatures.featureMask = parameter->rmt_feats.feat_mask;
    queued.remoteFeatures.featureFlags = parameter->rmt_feats.ct_feat_flag;
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_AVRC_TG_PASSTHROUGH_CMD_EVT)
  {
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::Passthrough;
    queued.passthrough.command =
      static_cast<EspBleClassicAvrcpCommand>(parameter->psth_cmd.key_code);
    queued.passthrough.state =
      static_cast<EspBleClassicAvrcpKeyState>(parameter->psth_cmd.key_state);
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT)
  {
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::Volume;
    queued.volume.value = parameter->set_abs_vol.volume;
    queued.volume.remoteCommand = true;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->volume = queued.volume.value;
    }
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT)
  {
    if (parameter->reg_ntf.event_id == ESP_AVRC_RN_VOLUME_CHANGE)
    {
      // Volume is answered here rather than by the sketch: this object already
      // holds the value, so there is nothing only a sketch could know.
      esp_avrc_rn_param_t response = {};
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->volumeNotificationRegistered = true;
        response.volume = impl->volume;
      }
      (void)esp_avrc_tg_send_rn_rsp(
        ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_INTERIM, &response);
      return;
    }
    // Anything else has to be answered by the sketch, which is why the
    // registration is reported rather than absorbed.
    EspBleClassicAvrcpImpl::Event queued;
    queued.type = EspBleClassicAvrcpImpl::EventType::NotificationRegistered;
    queued.notification = static_cast<EspBleClassicAvrcpNotification>(
      parameter->reg_ntf.event_id);
    impl->enqueue(std::move(queued));
  }
}
} // namespace
#endif

EspBleClassicAvrcp::EspBleClassicAvrcp(EspBleClassic *owner) : owner_(owner) {}

EspBleClassicAvrcp::~EspBleClassicAvrcp()
{
  end();
  delete impl_;
}

bool EspBleClassicAvrcp::begin(const EspBleClassicAvrcpConfig &config)
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  (void)config;
  owner_->setError(EspBleError::BackendFailure,
    "Classic AVRCP requires the custom ESP32 Classic host build");
  return false;
#else
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "Classic stack is not initialized");
    return false;
  }
  if (!config.controller && !config.target)
  {
    owner_->setError(EspBleError::InvalidArgument,
      "at least one AVRCP role must be enabled");
    return false;
  }
  if (config.initialVolume > 127)
  {
    owner_->setError(EspBleError::InvalidArgument,
      "AVRCP absolute volume must be in the range 0..127");
    return false;
  }
  if (initialized()) { owner_->clearError(); return true; }
  if (!impl_)
  {
    impl_ = new (std::nothrow) EspBleClassicAvrcpImpl();
    if (!impl_)
    {
      owner_->setError(EspBleError::ResourceExhausted,
        "failed to allocate AVRCP state");
      return false;
    }
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = false;
    impl_->controllerRequested = config.controller;
    impl_->targetRequested = config.target;
    impl_->controllerStateCompleted = false;
    impl_->targetStateCompleted = false;
    impl_->volume = config.initialVolume;
  }
  if (!activateAvrcp(impl_))
  {
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      impl_->controllerRequested = false;
      impl_->targetRequested = false;
    }
    owner_->setError(EspBleError::InvalidState,
      "another AVRCP profile is active");
    return false;
  }
  if (config.controller &&
      (esp_avrc_ct_register_callback(avrcpControllerCallback) != ESP_OK ||
       esp_avrc_ct_init() != ESP_OK ||
       !waitForAvrcpFlag([this]() {
         std::lock_guard<std::mutex> lock(impl_->mutex);
         return impl_->controllerStateCompleted;
       }) || !controllerInitialized()))
  {
    end();
    owner_->setError(EspBleError::BackendFailure,
      "failed to initialize AVRCP Controller");
    return false;
  }
  if (config.target &&
      (esp_avrc_tg_register_callback(avrcpTargetCallback) != ESP_OK ||
       esp_avrc_tg_init() != ESP_OK ||
       !waitForAvrcpFlag([this]() {
         std::lock_guard<std::mutex> lock(impl_->mutex);
         return impl_->targetStateCompleted;
       }) || !targetInitialized()))
  {
    end();
    owner_->setError(EspBleError::BackendFailure,
      "failed to initialize AVRCP Target");
    return false;
  }
  if (config.target)
  {
    esp_avrc_psth_bit_mask_t commands = {};
    esp_avrc_rn_evt_cap_mask_t notifications = {};
    if (esp_avrc_tg_get_psth_cmd_filter(
          ESP_AVRC_PSTH_FILTER_ALLOWED_CMD, &commands) != ESP_OK ||
        esp_avrc_tg_set_psth_cmd_filter(
          ESP_AVRC_PSTH_FILTER_SUPPORTED_CMD, &commands) != ESP_OK ||
        !esp_avrc_rn_evt_bit_mask_operation(
          ESP_AVRC_BIT_MASK_OP_SET, &notifications,
          ESP_AVRC_RN_VOLUME_CHANGE) ||
        esp_avrc_tg_set_rn_evt_cap(&notifications) != ESP_OK)
    {
      end();
      owner_->setError(EspBleError::BackendFailure,
        "failed to configure AVRCP Target capabilities");
      return false;
    }
  }
  owner_->clearError();
  return true;
#endif
}

void EspBleClassicAvrcp::end()
{
  if (!impl_) return;
#if ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  bool controller = false;
  bool target = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = true;
    // A failed or timed-out asynchronous begin may have reached the backend
    // before its completion callback. Deinitialize every requested role; the
    // backend treats an already-deinitialized role as a harmless result.
    controller = impl_->controllerRequested;
    target = impl_->targetRequested;
  }
  if (target)
  {
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      impl_->targetStateCompleted = false;
    }
    if (esp_avrc_tg_deinit() == ESP_OK)
      (void)waitForAvrcpFlag([this]() {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->targetStateCompleted && !impl_->targetInitialized;
      });
  }
  if (controller)
  {
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      impl_->controllerStateCompleted = false;
    }
    if (esp_avrc_ct_deinit() == ESP_OK)
      (void)waitForAvrcpFlag([this]() {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->controllerStateCompleted && !impl_->controllerInitialized;
      });
  }
  deactivateAvrcp(impl_);
#endif
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->eventHead = 0;
  impl_->eventCount = 0;
  impl_->ending = false;
  impl_->controllerRequested = false;
  impl_->targetRequested = false;
  impl_->controllerStateCompleted = false;
  impl_->targetStateCompleted = false;
  impl_->controllerInitialized = false;
  impl_->targetInitialized = false;
  impl_->controllerConnected = false;
  impl_->targetConnected = false;
  impl_->volumeNotificationRegistered = false;
}

bool EspBleClassicAvrcp::initialized() const
{ return controllerInitialized() || targetInitialized(); }
bool EspBleClassicAvrcp::controllerInitialized() const
{
  if (!impl_) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->controllerInitialized;
}
bool EspBleClassicAvrcp::targetInitialized() const
{
  if (!impl_) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->targetInitialized;
}
bool EspBleClassicAvrcp::controllerConnected() const
{
  if (!impl_) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->controllerConnected;
}
bool EspBleClassicAvrcp::targetConnected() const
{
  if (!impl_) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->targetConnected;
}
uint8_t EspBleClassicAvrcp::volume() const
{
  if (!impl_) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->volume;
}

bool EspBleClassicAvrcp::sendPassthrough(
  EspBleClassicAvrcpCommand command,
  EspBleClassicAvrcpKeyState state)
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  (void)command;
  (void)state;
  return false;
#else
  if (!controllerConnected())
  {
    owner_->setError(EspBleError::InvalidState,
      "AVRCP Controller is not connected");
    return false;
  }
  const esp_err_t result = esp_avrc_ct_send_passthrough_cmd(
    impl_->allocateTransactionLabel(), static_cast<uint8_t>(command),
    static_cast<uint8_t>(state));
  if (result != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to send AVRCP passthrough command");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicAvrcp::sendKey(EspBleClassicAvrcpCommand command)
{
  return sendPassthrough(command, EspBleClassicAvrcpKeyState::Pressed) &&
    sendPassthrough(command, EspBleClassicAvrcpKeyState::Released);
}

bool EspBleClassicAvrcp::requestMetadata(uint8_t attributeMask)
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  (void)attributeMask;
  return false;
#else
  if (!controllerConnected())
  {
    owner_->setError(EspBleError::InvalidState,
      "AVRCP Controller is not connected");
    return false;
  }
  if (attributeMask == 0 || (attributeMask & 0x80))
  {
    owner_->setError(EspBleError::InvalidArgument,
      "AVRCP metadata attribute mask is invalid");
    return false;
  }
  if (esp_avrc_ct_send_metadata_cmd(
        impl_->allocateTransactionLabel(), attributeMask) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to request AVRCP metadata");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicAvrcp::requestPlayStatus()
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  return false;
#else
  if (!controllerConnected())
  {
    owner_->setError(EspBleError::InvalidState,
      "AVRCP Controller is not connected");
    return false;
  }
  if (esp_avrc_ct_send_get_play_status_cmd(
        impl_->allocateTransactionLabel()) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to request AVRCP play status");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicAvrcp::setAbsoluteVolume(uint8_t value)
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  (void)value;
  return false;
#else
  if (!controllerConnected())
  {
    owner_->setError(EspBleError::InvalidState,
      "AVRCP Controller is not connected");
    return false;
  }
  if (value > 127)
  {
    owner_->setError(EspBleError::InvalidArgument,
      "AVRCP absolute volume must be in the range 0..127");
    return false;
  }
  if (esp_avrc_ct_send_set_absolute_volume_cmd(
        impl_->allocateTransactionLabel(), value) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to set AVRCP absolute volume");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicAvrcp::registerVolumeNotifications()
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  return false;
#else
  if (!controllerConnected())
  {
    owner_->setError(EspBleError::InvalidState,
      "AVRCP Controller is not connected");
    return false;
  }
  if (esp_avrc_ct_send_register_notification_cmd(
        impl_->allocateTransactionLabel(), ESP_AVRC_RN_VOLUME_CHANGE, 0) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to register AVRCP volume notification");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicAvrcp::setLocalVolume(uint8_t value)
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  (void)value;
  return false;
#else
  if (!targetInitialized())
  {
    owner_->setError(EspBleError::InvalidState,
      "AVRCP Target is not initialized");
    return false;
  }
  if (value > 127)
  {
    owner_->setError(EspBleError::InvalidArgument,
      "AVRCP absolute volume must be in the range 0..127");
    return false;
  }
  bool notify = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->volume = value;
    notify = impl_->volumeNotificationRegistered;
    if (notify) impl_->volumeNotificationRegistered = false;
  }
  if (notify)
  {
    esp_avrc_rn_param_t response = {};
    response.volume = value;
    if (esp_avrc_tg_send_rn_rsp(
          ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_CHANGED,
          &response) != ESP_OK)
    {
      owner_->setError(EspBleError::BackendFailure,
        "failed to notify AVRCP volume change");
      return false;
    }
  }
  owner_->clearError();
  return true;
#endif
}

void EspBleClassicAvrcp::onConnectionChanged(ConnectionCallback callback)
{ connectionCallback_ = std::move(callback); }
void EspBleClassicAvrcp::onRemoteFeatures(RemoteFeaturesCallback callback)
{ remoteFeaturesCallback_ = std::move(callback); }
void EspBleClassicAvrcp::onPassthrough(PassthroughCallback callback)
{ passthroughCallback_ = std::move(callback); }
void EspBleClassicAvrcp::onPassthroughResponse(
  PassthroughResponseCallback callback)
{ passthroughResponseCallback_ = std::move(callback); }
void EspBleClassicAvrcp::onMetadata(MetadataCallback callback)
{ metadataCallback_ = std::move(callback); }
void EspBleClassicAvrcp::onPlayStatus(PlayStatusCallback callback)
{ playStatusCallback_ = std::move(callback); }
void EspBleClassicAvrcp::onVolumeChanged(VolumeCallback callback)
{ volumeCallback_ = std::move(callback); }

size_t EspBleClassicAvrcp::droppedEventCount() const
{
  if (!impl_) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedEvents;
}

void EspBleClassicAvrcp::update()
{
  if (!impl_) return;
  while (true)
  {
    EspBleClassicAvrcpImpl::Event event;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->eventCount == 0) break;
      event = std::move(impl_->events[impl_->eventHead]);
      impl_->eventHead = (impl_->eventHead + 1) % AvrcpEventQueueCapacity;
      --impl_->eventCount;
    }
    if (event.type == EspBleClassicAvrcpImpl::EventType::Connection &&
        connectionCallback_)
      connectionCallback_(event.connection);
    else if (event.type == EspBleClassicAvrcpImpl::EventType::RemoteFeatures &&
             remoteFeaturesCallback_)
      remoteFeaturesCallback_(event.remoteFeatures);
    else if (event.type == EspBleClassicAvrcpImpl::EventType::Passthrough &&
             passthroughCallback_)
      passthroughCallback_(event.passthrough);
    else if (event.type == EspBleClassicAvrcpImpl::EventType::PassthroughResponse &&
             passthroughResponseCallback_)
      passthroughResponseCallback_(event.passthroughResponse);
    else if (event.type == EspBleClassicAvrcpImpl::EventType::Metadata &&
             metadataCallback_)
      metadataCallback_(event.metadata);
    else if (event.type == EspBleClassicAvrcpImpl::EventType::PlayStatus &&
             playStatusCallback_)
      playStatusCallback_(event.playStatus);
    else if (event.type == EspBleClassicAvrcpImpl::EventType::Volume &&
             volumeCallback_)
      volumeCallback_(event.volume);
    else if (event.type ==
               EspBleClassicAvrcpImpl::EventType::NotificationRegistered &&
             notificationRegisteredCallback_)
      notificationRegisteredCallback_(event.notification);
  }
}

void EspBleClassicAvrcp::onNotificationRegistered(
  NotificationRegisteredCallback callback)
{
  notificationRegisteredCallback_ = std::move(callback);
}

bool EspBleClassicAvrcp::registerNotifications(
  EspBleClassicAvrcpNotification event)
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  (void)event;
  return false;
#else
  if (!controllerConnected())
  {
    owner_->setError(
      EspBleError::InvalidState, "AVRCP Controller is not connected");
    return false;
  }
  if (
    esp_avrc_ct_send_register_notification_cmd(
      impl_->allocateTransactionLabel(),
      static_cast<uint8_t>(event), 0) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to register an AVRCP notification");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicAvrcp::setPlayerSetting(uint8_t attributeId, uint8_t value)
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  (void)attributeId;
  (void)value;
  return false;
#else
  if (!controllerConnected())
  {
    owner_->setError(
      EspBleError::InvalidState, "AVRCP Controller is not connected");
    return false;
  }
  if (
    esp_avrc_ct_send_set_player_value_cmd(
      impl_->allocateTransactionLabel(), attributeId, value) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to set the AVRCP player setting");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

#if ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
namespace
{
// Fills the profile's parameter union for the event being answered.
void fillNotificationParameter(
  const EspBleClassicAvrcpNotificationValue &value,
  esp_avrc_rn_param_t &parameter)
{
  switch (value.event)
  {
    case EspBleClassicAvrcpNotification::PlayStatus:
      parameter.playback =
        static_cast<esp_avrc_playback_stat_t>(value.playbackStatus);
      break;
    case EspBleClassicAvrcpNotification::TrackChange:
      memcpy(parameter.elm_id, value.trackId, sizeof(parameter.elm_id));
      break;
    case EspBleClassicAvrcpNotification::PlaybackPosition:
      parameter.play_pos = value.playbackPosition;
      break;
    case EspBleClassicAvrcpNotification::VolumeChange:
      parameter.volume = value.volume;
      break;
    case EspBleClassicAvrcpNotification::BatteryStatus:
      parameter.batt = static_cast<esp_avrc_batt_stat_t>(value.batteryStatus);
      break;
    default:
      // Reached-end and reached-start carry no value of their own.
      break;
  }
}
} // namespace
#endif

size_t EspBleClassicAvrcp::supportedNotifications(
  EspBleClassicAvrcpNotification *events, size_t capacity) const
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  (void)events;
  (void)capacity;
  return 0;
#else
  esp_avrc_rn_evt_cap_mask_t allowed = {};
  if (esp_avrc_tg_get_rn_evt_cap(ESP_AVRC_RN_CAP_ALLOWED_EVT, &allowed) !=
      ESP_OK)
  {
    return 0;
  }
  static const EspBleClassicAvrcpNotification candidates[] = {
    EspBleClassicAvrcpNotification::PlayStatus,
    EspBleClassicAvrcpNotification::TrackChange,
    EspBleClassicAvrcpNotification::TrackReachedEnd,
    EspBleClassicAvrcpNotification::TrackReachedStart,
    EspBleClassicAvrcpNotification::PlaybackPosition,
    EspBleClassicAvrcpNotification::BatteryStatus,
    EspBleClassicAvrcpNotification::VolumeChange,
  };
  size_t found = 0;
  for (EspBleClassicAvrcpNotification candidate : candidates)
  {
    if (!esp_avrc_rn_evt_bit_mask_operation(
          ESP_AVRC_BIT_MASK_OP_TEST, &allowed,
          static_cast<esp_avrc_rn_event_ids_t>(candidate)))
    {
      continue;
    }
    if (events != nullptr)
    {
      if (found >= capacity) break;
      events[found] = candidate;
    }
    ++found;
  }
  return found;
#endif
}

bool EspBleClassicAvrcp::setNotificationCapabilities(
  const EspBleClassicAvrcpNotification *events, size_t count)
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  (void)events;
  (void)count;
  return false;
#else
  if (events == nullptr || count == 0 || count > MaximumNotifications)
  {
    owner_->setError(
      EspBleError::InvalidArgument, "invalid AVRCP notification list");
    return false;
  }
  if (!targetInitialized())
  {
    owner_->setError(
      EspBleError::InvalidState, "AVRCP Target is not initialized");
    return false;
  }
  // Checked against what the host build actually allows, so an unsupported
  // event is named here instead of arriving as a bare backend failure.
  esp_avrc_rn_evt_cap_mask_t allowed = {};
  if (esp_avrc_tg_get_rn_evt_cap(ESP_AVRC_RN_CAP_ALLOWED_EVT, &allowed) !=
      ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure,
      "failed to read the allowed AVRCP notifications");
    return false;
  }
  esp_avrc_rn_evt_cap_mask_t mask = {};
  for (size_t index = 0; index < count; ++index)
  {
    const esp_avrc_rn_event_ids_t event =
      static_cast<esp_avrc_rn_event_ids_t>(events[index]);
    if (!esp_avrc_rn_evt_bit_mask_operation(
          ESP_AVRC_BIT_MASK_OP_TEST, &allowed, event))
    {
      owner_->setError(
        EspBleError::InvalidArgument,
        "this host build does not allow that AVRCP notification; "
        "supportedNotifications() lists the ones it does");
      return false;
    }
    if (!esp_avrc_rn_evt_bit_mask_operation(
          ESP_AVRC_BIT_MASK_OP_SET, &mask, event))
    {
      owner_->setError(
        EspBleError::InvalidArgument, "invalid AVRCP notification");
      return false;
    }
  }
  if (esp_avrc_tg_set_rn_evt_cap(&mask) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure,
      "failed to declare AVRCP notification capabilities");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicAvrcp::respondToNotification(
  const EspBleClassicAvrcpNotificationValue &value)
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  (void)value;
  return false;
#else
  if (!targetConnected())
  {
    owner_->setError(
      EspBleError::InvalidState, "AVRCP Target is not connected");
    return false;
  }
  esp_avrc_rn_param_t parameter = {};
  fillNotificationParameter(value, parameter);
  if (
    esp_avrc_tg_send_rn_rsp(
      static_cast<esp_avrc_rn_event_ids_t>(value.event),
      ESP_AVRC_RN_RSP_INTERIM, &parameter) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure,
      "failed to answer the AVRCP notification registration");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicAvrcp::sendNotificationChanged(
  const EspBleClassicAvrcpNotificationValue &value)
{
#if !ESPBLE_CLASSIC_AVRCP_BACKEND_AVAILABLE
  (void)value;
  return false;
#else
  if (!targetConnected())
  {
    owner_->setError(
      EspBleError::InvalidState, "AVRCP Target is not connected");
    return false;
  }
  esp_avrc_rn_param_t parameter = {};
  fillNotificationParameter(value, parameter);
  // A Changed response also ends the subscription, as the profile defines it:
  // the Controller registers again if it still wants to be told.
  if (
    esp_avrc_tg_send_rn_rsp(
      static_cast<esp_avrc_rn_event_ids_t>(value.event),
      ESP_AVRC_RN_RSP_CHANGED, &parameter) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure,
      "failed to send the AVRCP notification change");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}
