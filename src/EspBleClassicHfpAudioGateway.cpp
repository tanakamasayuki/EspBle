#include "EspBleClassic.h"
#include "EspBleClassicHfpInternal.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <utility>

#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32) && \
  defined(ESPBLE_CLASSIC_CUSTOM_HOST) && \
  (defined(ESPBLE_CLASSIC_ONLY) || defined(ESPBLE_ENABLE_CLASSIC))
#define ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE 1
#define esp_hf_ag_answer_call espble_bd_esp_hf_ag_answer_call
#define esp_hf_ag_audio_buff_alloc espble_bd_esp_hf_ag_audio_buff_alloc
#define esp_hf_ag_audio_buff_free espble_bd_esp_hf_ag_audio_buff_free
#define esp_hf_ag_audio_connect espble_bd_esp_hf_ag_audio_connect
#define esp_hf_ag_audio_data_send espble_bd_esp_hf_ag_audio_data_send
#define esp_hf_ag_audio_disconnect espble_bd_esp_hf_ag_audio_disconnect
#define esp_hf_ag_ciev_report espble_bd_esp_hf_ag_ciev_report
#define esp_hf_ag_cind_response espble_bd_esp_hf_ag_cind_response
#define esp_hf_ag_clcc_response espble_bd_esp_hf_ag_clcc_response
#define esp_hf_ag_cmee_send espble_bd_esp_hf_ag_cmee_send
#define esp_hf_ag_cnum_response espble_bd_esp_hf_ag_cnum_response
#define esp_hf_ag_cops_response espble_bd_esp_hf_ag_cops_response
#define esp_hf_ag_deinit espble_bd_esp_hf_ag_deinit
#define esp_hf_ag_end_call espble_bd_esp_hf_ag_end_call
#define esp_hf_ag_init espble_bd_esp_hf_ag_init
#define esp_hf_ag_out_call espble_bd_esp_hf_ag_out_call
#define esp_hf_ag_pkt_stat_nums_get espble_bd_esp_hf_ag_pkt_stat_nums_get
#define esp_hf_ag_register_audio_data_callback \
  espble_bd_esp_hf_ag_register_audio_data_callback
#define esp_hf_ag_register_callback espble_bd_esp_hf_ag_register_callback
#define esp_hf_ag_slc_connect espble_bd_esp_hf_ag_slc_connect
#define esp_hf_ag_slc_disconnect espble_bd_esp_hf_ag_slc_disconnect
#define esp_hf_ag_unknown_at_send espble_bd_esp_hf_ag_unknown_at_send
#define esp_hf_ag_volume_control espble_bd_esp_hf_ag_volume_control
#define esp_hf_ag_vra_control espble_bd_esp_hf_ag_vra_control
#define esp_bt_gap_set_scan_mode espble_bd_esp_bt_gap_set_scan_mode
#include <esp_hf_ag_api.h>
#include <esp_gap_bt_api.h>
#else
#define ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE 0
#endif

namespace
{
constexpr size_t HfpAgEventQueueCapacity = 32;

#if ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
String hfpAgAddress(const esp_bd_addr_t address)
{
  char value[18];
  snprintf(value, sizeof(value), "%02x:%02x:%02x:%02x:%02x:%02x",
    address[0], address[1], address[2], address[3], address[4], address[5]);
  return String(value);
}

bool parseHfpAgAddress(const char *value, esp_bd_addr_t address)
{
  if (!value) return false;
  unsigned parts[6];
  char trailing = 0;
  if (sscanf(value, "%2x:%2x:%2x:%2x:%2x:%2x%c",
        &parts[0], &parts[1], &parts[2], &parts[3], &parts[4], &parts[5],
        &trailing) != 6)
    return false;
  for (size_t index = 0; index < 6; ++index)
  {
    if (parts[index] > 0xff) return false;
    address[index] = static_cast<uint8_t>(parts[index]);
  }
  return true;
}

bool waitForHfpAgFlag(
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
#endif
} // namespace

struct EspBleClassicHfpAudioGatewayImpl
{
  enum class EventType : uint8_t
  {
    Connection,
    AudioConnection,
    CallState,
    Command,
    Volume,
    PacketStatistics,
  };

  struct Event
  {
    EventType type = EventType::Connection;
    EspBleClassicHfpConnection connection;
    EspBleClassicHfpAudioConnection audioConnection;
    EspBleClassicHfpCallState callState;
    EspBleClassicHfpAudioGatewayCommand command;
    EspBleClassicHfpVolume volume;
    EspBleClassicHfpPacketStatistics packetStatistics;
  };

  bool enqueue(Event event)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (eventCount == HfpAgEventQueueCapacity)
    {
      ++droppedEvents;
      return false;
    }
    events[(eventHead + eventCount) % HfpAgEventQueueCapacity] =
      std::move(event);
    ++eventCount;
    return true;
  }

  mutable std::mutex mutex;
  std::atomic<size_t> callbackUsers{0};
  Event events[HfpAgEventQueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  bool ending = false;
  bool requested = false;
  bool stateCompleted = false;
  bool initialized = false;
  String operatorName;
  String subscriberNumber;
  bool networkAvailable = true;
  uint8_t signalStrength = 5;
  bool roaming = false;
  uint8_t batteryLevel = 5;
  EspBleClassicHfpConnection connection;
  EspBleClassicHfpAudioConnection audioConnection;
  EspBleClassicHfpCallState callState;
  EspBleClassicHfpCurrentCall currentCall;
  std::shared_ptr<EspBleClassicHfpAudioGateway::AudioCallback> audioCallback;
};

#if ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
namespace
{
std::atomic<EspBleClassicHfpAudioGatewayImpl *> activeHfpAg{nullptr};
std::mutex hfpAgTargetMutex;

class HfpAgCallbackLease
{
public:
  HfpAgCallbackLease()
  {
    std::lock_guard<std::mutex> lock(hfpAgTargetMutex);
    impl_ = activeHfpAg.load(std::memory_order_relaxed);
    if (impl_) impl_->callbackUsers.fetch_add(1, std::memory_order_acq_rel);
  }
  ~HfpAgCallbackLease()
  {
    if (impl_) impl_->callbackUsers.fetch_sub(1, std::memory_order_acq_rel);
  }
  EspBleClassicHfpAudioGatewayImpl *get() const { return impl_; }
private:
  EspBleClassicHfpAudioGatewayImpl *impl_ = nullptr;
};

bool activateHfpAg(EspBleClassicHfpAudioGatewayImpl *impl)
{
  if (!espBleClassicAcquireHfpProfile(impl)) return false;
  std::lock_guard<std::mutex> lock(hfpAgTargetMutex);
  if (activeHfpAg.load(std::memory_order_relaxed))
  {
    espBleClassicReleaseHfpProfile(impl);
    return false;
  }
  activeHfpAg.store(impl, std::memory_order_release);
  return true;
}

void deactivateHfpAg(EspBleClassicHfpAudioGatewayImpl *impl)
{
  {
    std::lock_guard<std::mutex> lock(hfpAgTargetMutex);
    if (activeHfpAg.load(std::memory_order_relaxed) == impl)
      activeHfpAg.store(nullptr, std::memory_order_release);
  }
  while (impl->callbackUsers.load(std::memory_order_acquire) != 0) delay(1);
  espBleClassicReleaseHfpProfile(impl);
}

bool hfpAgInitSucceeded(esp_hf_prof_state_t state)
{ return state == ESP_HF_INIT_SUCCESS || state == ESP_HF_INIT_ALREADY; }
bool hfpAgDeinitSucceeded(esp_hf_prof_state_t state)
{ return state == ESP_HF_DEINIT_SUCCESS || state == ESP_HF_DEINIT_ALREADY; }

void enqueueHfpAgCallState(EspBleClassicHfpAudioGatewayImpl *impl)
{
  EspBleClassicHfpAudioGatewayImpl::Event queued;
  queued.type = EspBleClassicHfpAudioGatewayImpl::EventType::CallState;
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (impl->ending) return;
    queued.callState = impl->callState;
  }
  impl->enqueue(std::move(queued));
}

void enqueueHfpAgCommand(
  EspBleClassicHfpAudioGatewayImpl *impl,
  EspBleClassicHfpAudioGatewayCommandType type,
  const char *value = nullptr, bool enabled = false)
{
  EspBleClassicHfpAudioGatewayImpl::Event queued;
  queued.type = EspBleClassicHfpAudioGatewayImpl::EventType::Command;
  queued.command.type = type;
  queued.command.value = String(value ? value : "");
  queued.command.enabled = enabled;
  impl->enqueue(std::move(queued));
}

void hfpAgCallback(esp_hf_cb_event_t event, esp_hf_cb_param_t *parameter)
{
  HfpAgCallbackLease lease;
  EspBleClassicHfpAudioGatewayImpl *impl = lease.get();
  if (!impl || !parameter) return;

  if (event == ESP_HF_PROF_STATE_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    const esp_hf_prof_state_t state = parameter->prof_stat.state;
    if (hfpAgInitSucceeded(state)) impl->initialized = true;
    else if (hfpAgDeinitSucceeded(state)) impl->initialized = false;
    impl->stateCompleted = true;
    return;
  }
  if (event == ESP_HF_CONNECTION_STATE_EVT)
  {
    EspBleClassicHfpAudioGatewayImpl::Event queued;
    queued.type = EspBleClassicHfpAudioGatewayImpl::EventType::Connection;
    queued.connection.peerAddress = hfpAgAddress(parameter->conn_stat.remote_bda);
    queued.connection.state = static_cast<EspBleClassicHfpConnectionState>(
      parameter->conn_stat.state);
    queued.connection.peerFeatures = parameter->conn_stat.peer_feat;
    queued.connection.callHoldFeatures = parameter->conn_stat.chld_feat;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->ending) return;
      impl->connection = queued.connection;
      if (queued.connection.state ==
          EspBleClassicHfpConnectionState::Disconnected)
      {
        impl->audioConnection = EspBleClassicHfpAudioConnection();
        impl->callState = EspBleClassicHfpCallState();
        impl->currentCall = EspBleClassicHfpCurrentCall();
      }
    }
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_HF_AUDIO_STATE_EVT)
  {
    EspBleClassicHfpAudioGatewayImpl::Event queued;
    queued.type = EspBleClassicHfpAudioGatewayImpl::EventType::AudioConnection;
    queued.audioConnection.peerAddress =
      hfpAgAddress(parameter->audio_stat.remote_addr);
    queued.audioConnection.id = parameter->audio_stat.sync_conn_handle;
    queued.audioConnection.preferredFrameSize =
      parameter->audio_stat.preferred_frame_size;
    if (parameter->audio_stat.state == ESP_HF_AUDIO_STATE_CONNECTED_MSBC)
    {
      queued.audioConnection.state = EspBleClassicHfpAudioState::Connected;
      queued.audioConnection.codec = EspBleClassicAudioCodec::Msbc;
    }
    else
    {
      queued.audioConnection.state = static_cast<EspBleClassicHfpAudioState>(
        parameter->audio_stat.state);
      queued.audioConnection.codec =
        parameter->audio_stat.state == ESP_HF_AUDIO_STATE_CONNECTED
          ? EspBleClassicAudioCodec::Cvsd
          : EspBleClassicAudioCodec::Unknown;
    }
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->ending) return;
      impl->audioConnection = queued.audioConnection;
    }
    impl->enqueue(std::move(queued));
    return;
  }

  if (event == ESP_HF_CIND_RESPONSE_EVT || event == ESP_HF_IND_UPDATE_EVT)
  {
    EspBleClassicHfpCallState call;
    bool networkAvailable;
    uint8_t signal;
    bool roaming;
    uint8_t battery;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      call = impl->callState;
      networkAvailable = impl->networkAvailable;
      signal = impl->signalStrength;
      roaming = impl->roaming;
      battery = impl->batteryLevel;
    }
    esp_bd_addr_t &address = event == ESP_HF_CIND_RESPONSE_EVT
      ? parameter->cind_rep.remote_addr : parameter->ind_upd.remote_addr;
    if (event == ESP_HF_CIND_RESPONSE_EVT)
      (void)esp_hf_ag_cind_response(address,
        call.active ? ESP_HF_CALL_STATUS_CALL_IN_PROGRESS
                    : ESP_HF_CALL_STATUS_NO_CALLS,
        static_cast<esp_hf_call_setup_status_t>(call.setup),
        networkAvailable ? ESP_HF_NETWORK_STATE_AVAILABLE
                         : ESP_HF_NETWORK_STATE_NOT_AVAILABLE,
        signal,
        roaming ? ESP_HF_ROAMING_STATUS_ACTIVE
                : ESP_HF_ROAMING_STATUS_INACTIVE,
        battery, static_cast<esp_hf_call_held_status_t>(call.held));
    else
    {
      (void)esp_hf_ag_ciev_report(address, ESP_HF_IND_TYPE_SERVICE,
        networkAvailable ? ESP_HF_NETWORK_STATE_AVAILABLE
                         : ESP_HF_NETWORK_STATE_NOT_AVAILABLE);
      (void)esp_hf_ag_ciev_report(address, ESP_HF_IND_TYPE_SIGNAL, signal);
      (void)esp_hf_ag_ciev_report(address, ESP_HF_IND_TYPE_ROAM,
        roaming ? ESP_HF_ROAMING_STATUS_ACTIVE
                : ESP_HF_ROAMING_STATUS_INACTIVE);
      (void)esp_hf_ag_ciev_report(address, ESP_HF_IND_TYPE_BATTCHG, battery);
    }
    return;
  }
  if (event == ESP_HF_COPS_RESPONSE_EVT)
  {
    String value;
    { std::lock_guard<std::mutex> lock(impl->mutex); value = impl->operatorName; }
    (void)esp_hf_ag_cops_response(parameter->cops_rep.remote_addr,
      const_cast<char *>(value.c_str()));
    return;
  }
  if (event == ESP_HF_CNUM_RESPONSE_EVT)
  {
    String value;
    { std::lock_guard<std::mutex> lock(impl->mutex); value = impl->subscriberNumber; }
    (void)esp_hf_ag_cnum_response(parameter->cnum_rep.remote_addr,
      const_cast<char *>(value.c_str()), 0x81,
      ESP_HF_SUBSCRIBER_SERVICE_TYPE_VOICE);
    return;
  }
  if (event == ESP_HF_CLCC_RESPONSE_EVT)
  {
    EspBleClassicHfpCurrentCall call;
    { std::lock_guard<std::mutex> lock(impl->mutex); call = impl->currentCall; }
    if (call.index > 0)
      (void)esp_hf_ag_clcc_response(parameter->clcc_rep.remote_addr,
        call.index,
        call.incoming ? ESP_HF_CURRENT_CALL_DIRECTION_INCOMING
                      : ESP_HF_CURRENT_CALL_DIRECTION_OUTGOING,
        static_cast<esp_hf_current_call_status_t>(call.state),
        ESP_HF_CURRENT_CALL_MODE_VOICE,
        call.multiparty ? ESP_HF_CURRENT_CALL_MPTY_TYPE_MULTI
                        : ESP_HF_CURRENT_CALL_MPTY_TYPE_SINGLE,
        const_cast<char *>(call.number.c_str()), ESP_HF_CALL_ADDR_TYPE_UNKNOWN);
    (void)esp_hf_ag_clcc_response(parameter->clcc_rep.remote_addr, 0,
      ESP_HF_CURRENT_CALL_DIRECTION_OUTGOING,
      ESP_HF_CURRENT_CALL_STATUS_ACTIVE, ESP_HF_CURRENT_CALL_MODE_VOICE,
      ESP_HF_CURRENT_CALL_MPTY_TYPE_SINGLE, nullptr,
      ESP_HF_CALL_ADDR_TYPE_UNKNOWN);
    return;
  }
  if (event == ESP_HF_DIAL_EVT)
    enqueueHfpAgCommand(impl, EspBleClassicHfpAudioGatewayCommandType::Dial,
      parameter->out_call.num_or_loc);
  else if (event == ESP_HF_ATA_RESPONSE_EVT)
    enqueueHfpAgCommand(impl, EspBleClassicHfpAudioGatewayCommandType::Answer);
  else if (event == ESP_HF_CHUP_RESPONSE_EVT)
    enqueueHfpAgCommand(impl, EspBleClassicHfpAudioGatewayCommandType::Hangup);
  else if (event == ESP_HF_VTS_RESPONSE_EVT)
    enqueueHfpAgCommand(impl, EspBleClassicHfpAudioGatewayCommandType::Dtmf,
      parameter->vts_rep.code);
  else if (event == ESP_HF_BVRA_RESPONSE_EVT)
    enqueueHfpAgCommand(impl,
      EspBleClassicHfpAudioGatewayCommandType::VoiceRecognition, nullptr,
      parameter->vra_rep.value == ESP_HF_VR_STATE_ENABLED);
  else if (event == ESP_HF_NREC_RESPONSE_EVT)
    enqueueHfpAgCommand(impl,
      EspBleClassicHfpAudioGatewayCommandType::NoiseReduction, nullptr,
      parameter->nrec.state == ESP_HF_NREC_START);
  else if (event == ESP_HF_UNAT_RESPONSE_EVT)
    enqueueHfpAgCommand(impl,
      EspBleClassicHfpAudioGatewayCommandType::UnknownAt,
      parameter->unat_rep.unat);
  else if (event == ESP_HF_VOLUME_CONTROL_EVT)
  {
    EspBleClassicHfpAudioGatewayImpl::Event queued;
    queued.type = EspBleClassicHfpAudioGatewayImpl::EventType::Volume;
    queued.volume.target = static_cast<EspBleClassicHfpVolumeTarget>(
      parameter->volume_control.type);
    queued.volume.value = static_cast<uint8_t>(parameter->volume_control.volume);
    impl->enqueue(std::move(queued));
  }
  else if (event == ESP_HF_PKT_STAT_NUMS_GET_EVT)
  {
    EspBleClassicHfpAudioGatewayImpl::Event queued;
    queued.type = EspBleClassicHfpAudioGatewayImpl::EventType::PacketStatistics;
    queued.packetStatistics.received = parameter->pkt_nums.rx_total;
    queued.packetStatistics.receivedCorrect = parameter->pkt_nums.rx_correct;
    queued.packetStatistics.receivedError = parameter->pkt_nums.rx_err;
    queued.packetStatistics.receivedMissing = parameter->pkt_nums.rx_none;
    queued.packetStatistics.receivedLost = parameter->pkt_nums.rx_lost;
    queued.packetStatistics.sent = parameter->pkt_nums.tx_total;
    queued.packetStatistics.sentDiscarded = parameter->pkt_nums.tx_discarded;
    impl->enqueue(std::move(queued));
  }
}

void hfpAgAudioCallback(
  esp_hf_sync_conn_hdl_t connection,
  esp_hf_audio_buff_t *audio, bool badFrame)
{
  HfpAgCallbackLease lease;
  EspBleClassicHfpAudioGatewayImpl *impl = lease.get();
  if (!audio) return;
  if (impl)
  {
    bool ending;
    EspBleClassicAudioCodec codec;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      ending = impl->ending;
      codec = impl->audioConnection.codec;
    }
    const auto callback = std::atomic_load_explicit(
      &impl->audioCallback, std::memory_order_acquire);
    if (!ending && callback && *callback)
    {
      EspBleClassicHfpEncodedAudioView view;
      view.connectionId = connection;
      view.codec = codec;
      view.badFrame = badFrame;
      view.data = audio->data;
      view.length = audio->data_len;
      (*callback)(view);
    }
  }
  esp_hf_ag_audio_buff_free(audio);
}
} // namespace
#endif

EspBleClassicHfpAudioGateway::EspBleClassicHfpAudioGateway(
  EspBleClassic *owner) : owner_(owner) {}

EspBleClassicHfpAudioGateway::~EspBleClassicHfpAudioGateway()
{
  end();
  delete impl_;
}

bool EspBleClassicHfpAudioGateway::begin(
  const EspBleClassicHfpAudioGatewayConfig &config)
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  (void)config;
  owner_->setError(EspBleError::BackendFailure,
    "Classic HFP Audio Gateway requires the custom ESP32 Classic host build");
  return false;
#else
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState,
      "Classic stack is not initialized");
    return false;
  }
  if (initialized()) { owner_->clearError(); return true; }
  if (config.signalStrength > 5 || config.batteryLevel > 5)
  {
    owner_->setError(EspBleError::InvalidArgument,
      "HFP signal strength and battery level must be in the range 0..5");
    return false;
  }
  if (!impl_) impl_ = new (std::nothrow) EspBleClassicHfpAudioGatewayImpl();
  if (!impl_)
  {
    owner_->setError(EspBleError::ResourceExhausted,
      "failed to allocate HFP Audio Gateway state");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = false;
    impl_->requested = true;
    impl_->stateCompleted = false;
    impl_->operatorName = config.operatorName ? config.operatorName : "";
    impl_->subscriberNumber =
      config.subscriberNumber ? config.subscriberNumber : "";
    impl_->networkAvailable = config.networkAvailable;
    impl_->signalStrength = config.signalStrength;
    impl_->roaming = config.roaming;
    impl_->batteryLevel = config.batteryLevel;
  }
  if (!activateHfpAg(impl_))
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->requested = false;
    owner_->setError(EspBleError::InvalidState, "another HFP profile is active");
    return false;
  }
  if (esp_hf_ag_register_callback(hfpAgCallback) != ESP_OK ||
      esp_hf_ag_register_audio_data_callback(hfpAgAudioCallback) != ESP_OK ||
      esp_hf_ag_init() != ESP_OK ||
      !waitForHfpAgFlag([this]() {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->stateCompleted;
      }) || !initialized())
  {
    end();
    owner_->setError(EspBleError::BackendFailure,
      "failed to initialize HFP Audio Gateway");
    return false;
  }
  if (config.discoverable && esp_bt_gap_set_scan_mode(
        ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE) != ESP_OK)
  {
    end();
    owner_->setError(EspBleError::BackendFailure,
      "failed to make HFP Audio Gateway discoverable");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

void EspBleClassicHfpAudioGateway::end()
{
  if (!impl_) return;
  onAudio({});
#if ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  bool requested;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = true;
    requested = impl_->requested;
    impl_->stateCompleted = false;
  }
  if (requested && esp_hf_ag_deinit() == ESP_OK)
    (void)waitForHfpAgFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return impl_->stateCompleted && !impl_->initialized;
    });
  deactivateHfpAg(impl_);
#endif
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->eventHead = 0;
  impl_->eventCount = 0;
  impl_->ending = false;
  impl_->requested = false;
  impl_->stateCompleted = false;
  impl_->initialized = false;
  impl_->connection = EspBleClassicHfpConnection();
  impl_->audioConnection = EspBleClassicHfpAudioConnection();
  impl_->callState = EspBleClassicHfpCallState();
  impl_->currentCall = EspBleClassicHfpCurrentCall();
}

bool EspBleClassicHfpAudioGateway::initialized() const
{
  if (!impl_) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->initialized;
}

EspBleClassicHfpConnection EspBleClassicHfpAudioGateway::connection() const
{
  if (!impl_) return EspBleClassicHfpConnection();
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->connection;
}

bool EspBleClassicHfpAudioGateway::connected() const
{
  const auto state = connection().state;
  return state == EspBleClassicHfpConnectionState::Connected ||
    state == EspBleClassicHfpConnectionState::ServiceLevelConnected;
}

bool EspBleClassicHfpAudioGateway::serviceLevelConnected() const
{ return connection().state == EspBleClassicHfpConnectionState::ServiceLevelConnected; }

namespace
{
#if ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
bool currentHfpAgAddress(
  const EspBleClassicHfpConnection &connection, esp_bd_addr_t address)
{
  return connection.state != EspBleClassicHfpConnectionState::Disconnected &&
    parseHfpAgAddress(connection.peerAddress.c_str(), address);
}
#endif
} // namespace

bool EspBleClassicHfpAudioGateway::connect(const char *address)
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  (void)address; return false;
#else
  esp_bd_addr_t parsed;
  if (!initialized())
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP Audio Gateway is not initialized");
    return false;
  }
  if (!parseHfpAgAddress(address, parsed))
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid Bluetooth address");
    return false;
  }
  if (esp_hf_ag_slc_connect(parsed) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to connect HFP Audio Gateway");
    return false;
  }
  owner_->clearError(); return true;
#endif
}

bool EspBleClassicHfpAudioGateway::disconnect()
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  return false;
#else
  esp_bd_addr_t address;
  if (!currentHfpAgAddress(connection(), address))
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP Audio Gateway is not connected");
    return false;
  }
  if (esp_hf_ag_slc_disconnect(address) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to disconnect HFP Audio Gateway");
    return false;
  }
  owner_->clearError(); return true;
#endif
}

EspBleClassicHfpAudioConnection
EspBleClassicHfpAudioGateway::audioConnection() const
{
  if (!impl_) return EspBleClassicHfpAudioConnection();
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->audioConnection;
}

bool EspBleClassicHfpAudioGateway::audioConnected() const
{ return audioConnection().state == EspBleClassicHfpAudioState::Connected; }

bool EspBleClassicHfpAudioGateway::connectAudio()
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  return false;
#else
  esp_bd_addr_t address;
  if (!serviceLevelConnected() || !currentHfpAgAddress(connection(), address))
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP service-level connection is not established");
    return false;
  }
  if (esp_hf_ag_audio_connect(address) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to connect HFP audio");
    return false;
  }
  owner_->clearError(); return true;
#endif
}

bool EspBleClassicHfpAudioGateway::disconnectAudio()
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  return false;
#else
  esp_bd_addr_t address;
  if (!audioConnected() || !currentHfpAgAddress(connection(), address))
  {
    owner_->setError(EspBleError::InvalidState, "HFP audio is not connected");
    return false;
  }
  if (esp_hf_ag_audio_disconnect(address) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to disconnect HFP audio");
    return false;
  }
  owner_->clearError(); return true;
#endif
}

EspBleClassicAudioSendResult EspBleClassicHfpAudioGateway::send(
  const EspBleClassicHfpEncodedAudioPacket &packet)
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  (void)packet; return EspBleClassicAudioSendResult::BackendFailure;
#else
  const auto audio = audioConnection();
  if (audio.state != EspBleClassicHfpAudioState::Connected)
    return EspBleClassicAudioSendResult::InvalidState;
  if (!packet.data || packet.length == 0 || packet.length > UINT16_MAX)
    return EspBleClassicAudioSendResult::InvalidArgument;
  esp_hf_audio_buff_t *buffer =
    esp_hf_ag_audio_buff_alloc(static_cast<uint16_t>(packet.length));
  if (!buffer) return EspBleClassicAudioSendResult::WouldBlock;
  memcpy(buffer->data, packet.data, packet.length);
  buffer->data_len = static_cast<uint16_t>(packet.length);
  if (esp_hf_ag_audio_data_send(audio.id, buffer) != ESP_OK)
  {
    esp_hf_ag_audio_buff_free(buffer);
    return EspBleClassicAudioSendResult::BackendFailure;
  }
  return EspBleClassicAudioSendResult::Accepted;
#endif
}

bool EspBleClassicHfpAudioGateway::requestPacketStatistics()
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  return false;
#else
  const auto audio = audioConnection();
  if (audio.state != EspBleClassicHfpAudioState::Connected)
  {
    owner_->setError(EspBleError::InvalidState, "HFP audio is not connected");
    return false;
  }
  if (esp_hf_ag_pkt_stat_nums_get(audio.id) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to request HFP packet statistics");
    return false;
  }
  owner_->clearError(); return true;
#endif
}

bool EspBleClassicHfpAudioGateway::setNetworkStatus(
  bool available, uint8_t signalStrength, bool roaming, uint8_t batteryLevel)
{
  if (!impl_ || signalStrength > 5 || batteryLevel > 5)
  {
    owner_->setError(EspBleError::InvalidArgument,
      "HFP signal strength and battery level must be in the range 0..5");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->networkAvailable = available;
    impl_->signalStrength = signalStrength;
    impl_->roaming = roaming;
    impl_->batteryLevel = batteryLevel;
  }
#if ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  if (serviceLevelConnected())
  {
    esp_bd_addr_t address;
    if (!currentHfpAgAddress(connection(), address) ||
        esp_hf_ag_ciev_report(address, ESP_HF_IND_TYPE_SERVICE,
          available ? ESP_HF_NETWORK_STATE_AVAILABLE
                    : ESP_HF_NETWORK_STATE_NOT_AVAILABLE) != ESP_OK ||
        esp_hf_ag_ciev_report(address, ESP_HF_IND_TYPE_SIGNAL,
          signalStrength) != ESP_OK ||
        esp_hf_ag_ciev_report(address, ESP_HF_IND_TYPE_ROAM,
          roaming ? ESP_HF_ROAMING_STATUS_ACTIVE
                  : ESP_HF_ROAMING_STATUS_INACTIVE) != ESP_OK ||
        esp_hf_ag_ciev_report(address, ESP_HF_IND_TYPE_BATTCHG,
          batteryLevel) != ESP_OK)
    {
      owner_->setError(EspBleError::BackendFailure,
        "failed to report HFP network status");
      return false;
    }
  }
#endif
  owner_->clearError(); return true;
}

bool EspBleClassicHfpAudioGateway::respondToCommand(
  bool accepted, uint16_t extendedError)
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  (void)accepted; (void)extendedError; return false;
#else
  esp_bd_addr_t address;
  if (!serviceLevelConnected() || !currentHfpAgAddress(connection(), address))
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP service-level connection is not established");
    return false;
  }
  const auto response = accepted ? ESP_HF_AT_RESPONSE_CODE_OK
    : (extendedError ? ESP_HF_AT_RESPONSE_CODE_CME
                     : ESP_HF_AT_RESPONSE_CODE_ERR);
  if (esp_hf_ag_cmee_send(address, response,
        static_cast<esp_hf_cme_err_t>(extendedError)) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to send HFP command response");
    return false;
  }
  owner_->clearError(); return true;
#endif
}

bool EspBleClassicHfpAudioGateway::respondToUnknownAt(const char *response)
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  (void)response; return false;
#else
  esp_bd_addr_t address;
  if (!serviceLevelConnected() || !currentHfpAgAddress(connection(), address))
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP service-level connection is not established");
    return false;
  }
  if (esp_hf_ag_unknown_at_send(
        address, const_cast<char *>(response)) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to send unknown HFP AT response");
    return false;
  }
  owner_->clearError(); return true;
#endif
}

namespace
{
#if ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
using AgCallFunction = esp_err_t (*)(esp_bd_addr_t, int, int,
  esp_hf_call_status_t, esp_hf_call_setup_status_t, char *,
  esp_hf_call_addr_type_t);
#endif
}

static bool reportHfpAgCall(
  EspBleClassicHfpAudioGateway *gateway,
  EspBleClassicHfpAudioGatewayImpl *impl,
#if ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  AgCallFunction function,
#else
  void *function,
#endif
  const char *number, bool active, EspBleClassicHfpCallSetupState setup,
  bool incoming, EspBleClassicHfpCurrentCallState currentState)
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  (void)gateway; (void)impl; (void)function; (void)number; (void)active;
  (void)setup; (void)incoming; (void)currentState;
  return false;
#else
  esp_bd_addr_t address;
  const auto connection = gateway->connection();
  if (connection.state != EspBleClassicHfpConnectionState::ServiceLevelConnected ||
      !parseHfpAgAddress(connection.peerAddress.c_str(), address))
    return false;
  String copy = number ? number : "";
  if (function(address, active ? 1 : 0, 0,
        active ? ESP_HF_CALL_STATUS_CALL_IN_PROGRESS
               : ESP_HF_CALL_STATUS_NO_CALLS,
        static_cast<esp_hf_call_setup_status_t>(setup),
        const_cast<char *>(copy.c_str()), ESP_HF_CALL_ADDR_TYPE_UNKNOWN) != ESP_OK)
    return false;
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->callState.active = active;
    impl->callState.setup = setup;
    impl->callState.held = EspBleClassicHfpCallHeldState::None;
    if (active || setup != EspBleClassicHfpCallSetupState::Idle)
    {
      impl->currentCall.index = 1;
      impl->currentCall.incoming = incoming;
      impl->currentCall.state = currentState;
      impl->currentCall.number = copy;
    }
    else impl->currentCall = EspBleClassicHfpCurrentCall();
  }
  enqueueHfpAgCallState(impl);
  return true;
#endif
}

bool EspBleClassicHfpAudioGateway::reportIncomingCall(const char *number)
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  (void)number; return false;
#else
  if (!number || !number[0])
  {
    owner_->setError(EspBleError::InvalidArgument, "incoming number is empty");
    return false;
  }
  const bool success = reportHfpAgCall(this, impl_, esp_hf_ag_answer_call,
    number, false, EspBleClassicHfpCallSetupState::Incoming, true,
    EspBleClassicHfpCurrentCallState::Incoming);
  if (!success) owner_->setError(EspBleError::BackendFailure,
    "failed to report incoming HFP call"); else owner_->clearError();
  return success;
#endif
}

bool EspBleClassicHfpAudioGateway::reportOutgoingCall(const char *number)
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  (void)number; return false;
#else
  if (!number || !number[0])
  {
    owner_->setError(EspBleError::InvalidArgument, "outgoing number is empty");
    return false;
  }
  const bool success = reportHfpAgCall(this, impl_, esp_hf_ag_out_call,
    number, false, EspBleClassicHfpCallSetupState::OutgoingDialing, false,
    EspBleClassicHfpCurrentCallState::Dialing);
  if (!success) owner_->setError(EspBleError::BackendFailure,
    "failed to report outgoing HFP call"); else owner_->clearError();
  return success;
#endif
}

bool EspBleClassicHfpAudioGateway::reportCallActive()
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  return false;
#else
  const auto call = currentCall();
  if (call.index == 0)
  {
    owner_->setError(EspBleError::InvalidState, "there is no HFP call");
    return false;
  }
  const bool success = reportHfpAgCall(this, impl_,
    call.incoming ? esp_hf_ag_answer_call : esp_hf_ag_out_call,
    call.number.c_str(), true, EspBleClassicHfpCallSetupState::Idle,
    call.incoming, EspBleClassicHfpCurrentCallState::Active);
  if (!success) owner_->setError(EspBleError::BackendFailure,
    "failed to report active HFP call"); else owner_->clearError();
  return success;
#endif
}

bool EspBleClassicHfpAudioGateway::reportCallEnded()
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  return false;
#else
  const auto call = currentCall();
  const bool success = reportHfpAgCall(this, impl_, esp_hf_ag_end_call,
    call.number.c_str(), false, EspBleClassicHfpCallSetupState::Idle,
    call.incoming, EspBleClassicHfpCurrentCallState::Active);
  if (!success) owner_->setError(EspBleError::BackendFailure,
    "failed to report ended HFP call"); else owner_->clearError();
  return success;
#endif
}

bool EspBleClassicHfpAudioGateway::setVolume(
  EspBleClassicHfpVolumeTarget target, uint8_t value)
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  (void)target; (void)value; return false;
#else
  esp_bd_addr_t address;
  if (value > 15)
  {
    owner_->setError(EspBleError::InvalidArgument,
      "HFP volume must be in the range 0..15");
    return false;
  }
  if (!serviceLevelConnected() || !currentHfpAgAddress(connection(), address))
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP service-level connection is not established");
    return false;
  }
  if (esp_hf_ag_volume_control(address,
        static_cast<esp_hf_volume_control_target_t>(target), value) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to update HFP volume");
    return false;
  }
  owner_->clearError(); return true;
#endif
}

bool EspBleClassicHfpAudioGateway::setVoiceRecognition(bool enabled)
{
#if !ESPBLE_CLASSIC_HFP_AG_BACKEND_AVAILABLE
  (void)enabled; return false;
#else
  esp_bd_addr_t address;
  if (!serviceLevelConnected() || !currentHfpAgAddress(connection(), address))
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP service-level connection is not established");
    return false;
  }
  if (esp_hf_ag_vra_control(address,
        enabled ? ESP_HF_VR_STATE_ENABLED : ESP_HF_VR_STATE_DISABLED) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to update HFP voice recognition");
    return false;
  }
  owner_->clearError(); return true;
#endif
}

EspBleClassicHfpCallState EspBleClassicHfpAudioGateway::callState() const
{
  if (!impl_) return EspBleClassicHfpCallState();
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->callState;
}

EspBleClassicHfpCurrentCall EspBleClassicHfpAudioGateway::currentCall() const
{
  if (!impl_) return EspBleClassicHfpCurrentCall();
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->currentCall;
}

void EspBleClassicHfpAudioGateway::onConnectionChanged(ConnectionCallback callback)
{ connectionCallback_ = std::move(callback); }
void EspBleClassicHfpAudioGateway::onAudioConnectionChanged(
  AudioConnectionCallback callback)
{ audioConnectionCallback_ = std::move(callback); }
void EspBleClassicHfpAudioGateway::onCallStateChanged(CallStateCallback callback)
{ callStateCallback_ = std::move(callback); }
void EspBleClassicHfpAudioGateway::onCommand(CommandCallback callback)
{ commandCallback_ = std::move(callback); }
void EspBleClassicHfpAudioGateway::onVolumeChanged(VolumeCallback callback)
{ volumeCallback_ = std::move(callback); }
void EspBleClassicHfpAudioGateway::onPacketStatistics(
  PacketStatisticsCallback callback)
{ packetStatisticsCallback_ = std::move(callback); }

void EspBleClassicHfpAudioGateway::onAudio(AudioCallback callback)
{
  if (!impl_ && callback)
  {
    impl_ = new (std::nothrow) EspBleClassicHfpAudioGatewayImpl();
    if (!impl_)
    {
      owner_->setError(EspBleError::ResourceExhausted,
        "failed to allocate HFP Audio Gateway state");
      return;
    }
  }
  if (!impl_) return;
  auto replacement = callback
    ? std::make_shared<AudioCallback>(std::move(callback))
    : std::shared_ptr<AudioCallback>();
  std::atomic_store_explicit(
    &impl_->audioCallback, replacement, std::memory_order_release);
  while (impl_->callbackUsers.load(std::memory_order_acquire) != 0) delay(1);
}

size_t EspBleClassicHfpAudioGateway::droppedEventCount() const
{
  if (!impl_) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedEvents;
}

void EspBleClassicHfpAudioGateway::update()
{
  if (!impl_) return;
  while (true)
  {
    EspBleClassicHfpAudioGatewayImpl::Event event;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->eventCount == 0) break;
      event = std::move(impl_->events[impl_->eventHead]);
      impl_->eventHead = (impl_->eventHead + 1) % HfpAgEventQueueCapacity;
      --impl_->eventCount;
    }
    if (event.type == EspBleClassicHfpAudioGatewayImpl::EventType::Connection &&
        connectionCallback_)
      connectionCallback_(event.connection);
    else if (event.type ==
               EspBleClassicHfpAudioGatewayImpl::EventType::AudioConnection &&
             audioConnectionCallback_)
      audioConnectionCallback_(event.audioConnection);
    else if (event.type == EspBleClassicHfpAudioGatewayImpl::EventType::CallState &&
             callStateCallback_)
      callStateCallback_(event.callState);
    else if (event.type == EspBleClassicHfpAudioGatewayImpl::EventType::Command &&
             commandCallback_)
      commandCallback_(event.command);
    else if (event.type == EspBleClassicHfpAudioGatewayImpl::EventType::Volume &&
             volumeCallback_)
      volumeCallback_(event.volume);
    else if (event.type ==
               EspBleClassicHfpAudioGatewayImpl::EventType::PacketStatistics &&
             packetStatisticsCallback_)
      packetStatisticsCallback_(event.packetStatistics);
  }
}
