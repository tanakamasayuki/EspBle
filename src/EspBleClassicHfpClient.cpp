#include "EspBleClassic.h"
#include "EspBleClassicBuild.h"
#include "EspBleClassicHfpInternal.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <utility>

#if defined(CONFIG_IDF_TARGET_ESP32) && \
  defined(ESPBLE_CLASSIC_CUSTOM_HOST) && \
  (defined(ESPBLE_CLASSIC_ONLY) || defined(ESPBLE_ENABLE_CLASSIC))
#define ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE 1
#define esp_hf_client_answer_call espble_bd_esp_hf_client_answer_call
#define esp_hf_client_audio_buff_alloc espble_bd_esp_hf_client_audio_buff_alloc
#define esp_hf_client_audio_buff_free espble_bd_esp_hf_client_audio_buff_free
#define esp_hf_client_audio_data_send espble_bd_esp_hf_client_audio_data_send
#define esp_hf_client_connect espble_bd_esp_hf_client_connect
#define esp_hf_client_connect_audio espble_bd_esp_hf_client_connect_audio
#define esp_hf_client_deinit espble_bd_esp_hf_client_deinit
#define esp_hf_client_dial espble_bd_esp_hf_client_dial
#define esp_hf_client_dial_memory espble_bd_esp_hf_client_dial_memory
#define esp_hf_client_query_current_operator_name \
  espble_bd_esp_hf_client_query_current_operator_name
#define esp_hf_client_retrieve_subscriber_info \
  espble_bd_esp_hf_client_retrieve_subscriber_info
#define esp_hf_client_request_last_voice_tag_number \
  espble_bd_esp_hf_client_request_last_voice_tag_number
#define esp_hf_client_send_nrec espble_bd_esp_hf_client_send_nrec
#define esp_hf_client_send_xapl espble_bd_esp_hf_client_send_xapl
#define esp_hf_client_send_iphoneaccev \
  espble_bd_esp_hf_client_send_iphoneaccev
#define esp_hf_client_disconnect espble_bd_esp_hf_client_disconnect
#define esp_hf_client_disconnect_audio espble_bd_esp_hf_client_disconnect_audio
#define esp_hf_client_init espble_bd_esp_hf_client_init
#define esp_hf_client_pkt_stat_nums_get \
  espble_bd_esp_hf_client_pkt_stat_nums_get
#define esp_hf_client_query_current_calls \
  espble_bd_esp_hf_client_query_current_calls
#define esp_hf_client_register_audio_data_callback \
  espble_bd_esp_hf_client_register_audio_data_callback
#define esp_hf_client_register_callback \
  espble_bd_esp_hf_client_register_callback
#define esp_hf_client_reject_call espble_bd_esp_hf_client_reject_call
#define esp_hf_client_send_dtmf espble_bd_esp_hf_client_send_dtmf
#define esp_hf_client_start_voice_recognition \
  espble_bd_esp_hf_client_start_voice_recognition
#define esp_hf_client_stop_voice_recognition \
  espble_bd_esp_hf_client_stop_voice_recognition
#define esp_hf_client_volume_update espble_bd_esp_hf_client_volume_update
#include "esp32/include/esp_hf_client_api.h"
#else
#define ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE 0
#endif

namespace
{
constexpr size_t HfpClientEventQueueCapacity = 32;

#if ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
String hfpClientAddress(const esp_bd_addr_t address)
{
  char value[18];
  snprintf(value, sizeof(value), "%02x:%02x:%02x:%02x:%02x:%02x",
    address[0], address[1], address[2], address[3], address[4], address[5]);
  return String(value);
}

bool parseHfpClientAddress(const char *value, esp_bd_addr_t address)
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

bool waitForHfpClientFlag(
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

struct EspBleClassicHfpClientImpl
{
  enum class EventType : uint8_t
  {
    Connection,
    AudioConnection,
    CallState,
    Caller,
    Ring,
    CurrentCall,
    Volume,
    AtResponse,
    PacketStatistics,
    OperatorName,
    SubscriberNumber,
    VoiceTagNumber,
    InBandRingTone,
  };

  struct Event
  {
    EventType type = EventType::Connection;
    EspBleClassicHfpConnection connection;
    EspBleClassicHfpAudioConnection audioConnection;
    EspBleClassicHfpCallState callState;
    EspBleClassicHfpCaller caller;
    EspBleClassicHfpCurrentCall currentCall;
    EspBleClassicHfpVolume volume;
    EspBleClassicHfpAtResponse atResponse;
    EspBleClassicHfpPacketStatistics packetStatistics;
    String text;
    EspBleClassicHfpSubscriberNumber subscriberNumber;
    bool flag = false;
  };

  bool enqueue(Event event)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (eventCount == HfpClientEventQueueCapacity)
    {
      ++droppedEvents;
      return false;
    }
    events[(eventHead + eventCount) % HfpClientEventQueueCapacity] =
      std::move(event);
    ++eventCount;
    return true;
  }

  mutable std::mutex mutex;
  std::atomic<size_t> callbackUsers{0};
  Event events[HfpClientEventQueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  bool ending = false;
  bool requested = false;
  bool stateCompleted = false;
  bool initialized = false;
  EspBleClassicHfpConnection connection;
  EspBleClassicHfpAudioConnection audioConnection;
  EspBleClassicHfpCallState callState;
  std::shared_ptr<EspBleClassicHfpClient::AudioCallback> audioCallback;
};

#if ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
namespace
{
std::atomic<EspBleClassicHfpClientImpl *> activeHfpClient{nullptr};
std::mutex hfpClientTargetMutex;

class HfpClientCallbackLease
{
public:
  HfpClientCallbackLease()
  {
    std::lock_guard<std::mutex> lock(hfpClientTargetMutex);
    impl_ = activeHfpClient.load(std::memory_order_relaxed);
    if (impl_) impl_->callbackUsers.fetch_add(1, std::memory_order_acq_rel);
  }
  ~HfpClientCallbackLease()
  {
    if (impl_) impl_->callbackUsers.fetch_sub(1, std::memory_order_acq_rel);
  }
  EspBleClassicHfpClientImpl *get() const { return impl_; }
private:
  EspBleClassicHfpClientImpl *impl_ = nullptr;
};

bool activateHfpClient(EspBleClassicHfpClientImpl *impl)
{
  if (!espBleClassicAcquireHfpProfile(impl)) return false;
  std::lock_guard<std::mutex> lock(hfpClientTargetMutex);
  if (activeHfpClient.load(std::memory_order_relaxed))
  {
    espBleClassicReleaseHfpProfile(impl);
    return false;
  }
  activeHfpClient.store(impl, std::memory_order_release);
  return true;
}

void deactivateHfpClient(EspBleClassicHfpClientImpl *impl)
{
  {
    std::lock_guard<std::mutex> lock(hfpClientTargetMutex);
    if (activeHfpClient.load(std::memory_order_relaxed) == impl)
      activeHfpClient.store(nullptr, std::memory_order_release);
  }
  while (impl->callbackUsers.load(std::memory_order_acquire) != 0) delay(1);
  espBleClassicReleaseHfpProfile(impl);
}

bool hfpInitSucceeded(esp_hf_prof_state_t state)
{ return state == ESP_HF_INIT_SUCCESS || state == ESP_HF_INIT_ALREADY; }
bool hfpDeinitSucceeded(esp_hf_prof_state_t state)
{ return state == ESP_HF_DEINIT_SUCCESS || state == ESP_HF_DEINIT_ALREADY; }

void enqueueHfpCallState(EspBleClassicHfpClientImpl *impl)
{
  EspBleClassicHfpClientImpl::Event queued;
  queued.type = EspBleClassicHfpClientImpl::EventType::CallState;
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (impl->ending) return;
    queued.callState = impl->callState;
  }
  impl->enqueue(std::move(queued));
}

void hfpClientCallback(
  esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *parameter)
{
  HfpClientCallbackLease lease;
  EspBleClassicHfpClientImpl *impl = lease.get();
  if (!impl || !parameter) return;

  if (event == ESP_HF_CLIENT_PROF_STATE_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    const esp_hf_prof_state_t state = parameter->prof_stat.state;
    if (hfpInitSucceeded(state)) impl->initialized = true;
    else if (hfpDeinitSucceeded(state)) impl->initialized = false;
    impl->stateCompleted = true;
    return;
  }
  if (event == ESP_HF_CLIENT_CONNECTION_STATE_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::Connection;
    queued.connection.peerAddress =
      hfpClientAddress(parameter->conn_stat.remote_bda);
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
      }
    }
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_HF_CLIENT_AUDIO_STATE_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::AudioConnection;
    queued.audioConnection.peerAddress =
      hfpClientAddress(parameter->audio_stat.remote_bda);
    queued.audioConnection.id = parameter->audio_stat.sync_conn_handle;
    queued.audioConnection.preferredFrameSize =
      parameter->audio_stat.preferred_frame_size;
    if (parameter->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC)
    {
      queued.audioConnection.state = EspBleClassicHfpAudioState::Connected;
      queued.audioConnection.codec = EspBleClassicAudioCodec::Msbc;
    }
    else
    {
      queued.audioConnection.state = static_cast<EspBleClassicHfpAudioState>(
        parameter->audio_stat.state);
      queued.audioConnection.codec =
        parameter->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED
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
  if (event == ESP_HF_CLIENT_CIND_CALL_EVT)
  {
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->callState.active =
        parameter->call.status == ESP_HF_CALL_STATUS_CALL_IN_PROGRESS;
    }
    enqueueHfpCallState(impl);
    return;
  }
  if (event == ESP_HF_CLIENT_CIND_CALL_SETUP_EVT)
  {
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->callState.setup = static_cast<EspBleClassicHfpCallSetupState>(
        parameter->call_setup.status);
    }
    enqueueHfpCallState(impl);
    return;
  }
  if (event == ESP_HF_CLIENT_CIND_CALL_HELD_EVT)
  {
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->callState.held = static_cast<EspBleClassicHfpCallHeldState>(
        parameter->call_held.status);
    }
    enqueueHfpCallState(impl);
    return;
  }
  if (event == ESP_HF_CLIENT_CLIP_EVT || event == ESP_HF_CLIENT_CCWA_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::Caller;
    queued.caller.waiting = event == ESP_HF_CLIENT_CCWA_EVT;
    const char *number = queued.caller.waiting
      ? parameter->ccwa.number : parameter->clip.number;
    queued.caller.number = String(number ? number : "");
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_HF_CLIENT_RING_IND_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::Ring;
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_HF_CLIENT_CLCC_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::CurrentCall;
    queued.currentCall.index = parameter->clcc.idx;
    queued.currentCall.incoming =
      parameter->clcc.dir == ESP_HF_CURRENT_CALL_DIRECTION_INCOMING;
    queued.currentCall.state = static_cast<EspBleClassicHfpCurrentCallState>(
      parameter->clcc.status);
    queued.currentCall.multiparty =
      parameter->clcc.mpty == ESP_HF_CURRENT_CALL_MPTY_TYPE_MULTI;
    queued.currentCall.number = String(
      parameter->clcc.number ? parameter->clcc.number : "");
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_HF_CLIENT_VOLUME_CONTROL_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::Volume;
    queued.volume.target = static_cast<EspBleClassicHfpVolumeTarget>(
      parameter->volume_control.type);
    queued.volume.value = static_cast<uint8_t>(parameter->volume_control.volume);
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_HF_CLIENT_AT_RESPONSE_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::AtResponse;
    queued.atResponse.code = static_cast<uint8_t>(parameter->at_response.code);
    queued.atResponse.extendedError =
      static_cast<uint16_t>(parameter->at_response.cme);
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_HF_CLIENT_COPS_CURRENT_OPERATOR_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::OperatorName;
    queued.text = String(parameter->cops.name ? parameter->cops.name : "");
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_HF_CLIENT_CNUM_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::SubscriberNumber;
    queued.subscriberNumber.number =
      String(parameter->cnum.number ? parameter->cnum.number : "");
    // Kept as the AT value: an AG reporting something outside the two known
    // service types arrives as Unknown rather than as a guess.
    queued.subscriberNumber.serviceType =
      parameter->cnum.type == ESP_HF_SUBSCRIBER_SERVICE_TYPE_VOICE
        ? EspBleClassicHfpSubscriberServiceType::Voice
        : (parameter->cnum.type == ESP_HF_SUBSCRIBER_SERVICE_TYPE_FAX
            ? EspBleClassicHfpSubscriberServiceType::Fax
            : EspBleClassicHfpSubscriberServiceType::Unknown);
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_HF_CLIENT_BSIR_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::InBandRingTone;
    queued.flag = parameter->bsir.state ==
      ESP_HF_CLIENT_IN_BAND_RINGTONE_PROVIDED;
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_HF_CLIENT_BINP_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::VoiceTagNumber;
    queued.text = String(parameter->binp.number ? parameter->binp.number : "");
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_HF_CLIENT_PKT_STAT_NUMS_GET_EVT)
  {
    EspBleClassicHfpClientImpl::Event queued;
    queued.type = EspBleClassicHfpClientImpl::EventType::PacketStatistics;
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

void hfpClientAudioCallback(
  esp_hf_sync_conn_hdl_t connection,
  esp_hf_audio_buff_t *audio,
  bool badFrame)
{
  HfpClientCallbackLease lease;
  EspBleClassicHfpClientImpl *impl = lease.get();
  if (!audio) return;
  if (impl)
  {
    bool ending = false;
    EspBleClassicAudioCodec codec = EspBleClassicAudioCodec::Unknown;
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
  esp_hf_client_audio_buff_free(audio);
}
} // namespace
#endif

EspBleClassicHfpClient::EspBleClassicHfpClient(EspBleClassic *owner) :
  owner_(owner) {}

EspBleClassicHfpClient::~EspBleClassicHfpClient()
{
  end();
  delete impl_;
}

bool EspBleClassicHfpClient::begin()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  owner_->setError(EspBleError::BackendFailure,
    "Classic HFP Client requires the custom ESP32 Classic host build");
  return false;
#else
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState,
      "Classic stack is not initialized");
    return false;
  }
  if (initialized()) { owner_->clearError(); return true; }
  if (!impl_)
  {
    impl_ = new (std::nothrow) EspBleClassicHfpClientImpl();
    if (!impl_)
    {
      owner_->setError(EspBleError::ResourceExhausted,
        "failed to allocate HFP Client state");
      return false;
    }
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = false;
    impl_->requested = true;
    impl_->stateCompleted = false;
  }
  if (!activateHfpClient(impl_))
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->requested = false;
    owner_->setError(EspBleError::InvalidState,
      "another HFP profile is active");
    return false;
  }
  if (esp_hf_client_register_callback(hfpClientCallback) != ESP_OK ||
      esp_hf_client_register_audio_data_callback(hfpClientAudioCallback) != ESP_OK ||
      esp_hf_client_init() != ESP_OK ||
      !waitForHfpClientFlag([this]() {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->stateCompleted;
      }) || !initialized())
  {
    end();
    owner_->setError(EspBleError::BackendFailure,
      "failed to initialize HFP Client");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

void EspBleClassicHfpClient::end()
{
  if (!impl_) return;
  onAudio({});
#if ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  bool requested = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = true;
    requested = impl_->requested;
    impl_->stateCompleted = false;
  }
  if (requested && esp_hf_client_deinit() == ESP_OK)
    (void)waitForHfpClientFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return impl_->stateCompleted && !impl_->initialized;
    });
  deactivateHfpClient(impl_);
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
}

bool EspBleClassicHfpClient::initialized() const
{
  if (!impl_) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->initialized;
}

bool EspBleClassicHfpClient::connect(const char *address)
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  (void)address;
  return false;
#else
  if (!initialized())
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP Client is not initialized");
    return false;
  }
  esp_bd_addr_t parsed;
  if (!parseHfpClientAddress(address, parsed))
  {
    owner_->setError(EspBleError::InvalidArgument,
      "invalid Bluetooth address");
    return false;
  }
  if (esp_hf_client_connect(parsed) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to connect HFP Client");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicHfpClient::disconnect()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  const EspBleClassicHfpConnection current = connection();
  esp_bd_addr_t parsed;
  if (current.state == EspBleClassicHfpConnectionState::Disconnected ||
      !parseHfpClientAddress(current.peerAddress.c_str(), parsed))
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP Client is not connected");
    return false;
  }
  if (esp_hf_client_disconnect(parsed) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to disconnect HFP Client");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicHfpClient::connected() const
{
  const auto state = connection().state;
  return state == EspBleClassicHfpConnectionState::Connected ||
    state == EspBleClassicHfpConnectionState::ServiceLevelConnected;
}

bool EspBleClassicHfpClient::serviceLevelConnected() const
{
  return connection().state ==
    EspBleClassicHfpConnectionState::ServiceLevelConnected;
}

EspBleClassicHfpConnection EspBleClassicHfpClient::connection() const
{
  if (!impl_) return EspBleClassicHfpConnection();
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->connection;
}

bool EspBleClassicHfpClient::connectAudio()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  const EspBleClassicHfpConnection current = connection();
  esp_bd_addr_t parsed;
  if (!serviceLevelConnected() ||
      !parseHfpClientAddress(current.peerAddress.c_str(), parsed))
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP service-level connection is not established");
    return false;
  }
  if (esp_hf_client_connect_audio(parsed) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to connect HFP audio");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicHfpClient::disconnectAudio()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  const EspBleClassicHfpConnection current = connection();
  esp_bd_addr_t parsed;
  if (!audioConnected() ||
      !parseHfpClientAddress(current.peerAddress.c_str(), parsed))
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP audio is not connected");
    return false;
  }
  if (esp_hf_client_disconnect_audio(parsed) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to disconnect HFP audio");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicHfpClient::audioConnected() const
{ return audioConnection().state == EspBleClassicHfpAudioState::Connected; }

EspBleClassicHfpAudioConnection
EspBleClassicHfpClient::audioConnection() const
{
  if (!impl_) return EspBleClassicHfpAudioConnection();
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->audioConnection;
}

EspBleClassicAudioSendResult EspBleClassicHfpClient::send(
  const EspBleClassicHfpEncodedAudioPacket &packet)
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  (void)packet;
  return EspBleClassicAudioSendResult::BackendFailure;
#else
  const EspBleClassicHfpAudioConnection audio = audioConnection();
  if (audio.state != EspBleClassicHfpAudioState::Connected)
    return EspBleClassicAudioSendResult::InvalidState;
  if (!packet.data || packet.length == 0 || packet.length > UINT16_MAX)
    return EspBleClassicAudioSendResult::InvalidArgument;
  esp_hf_audio_buff_t *buffer =
    esp_hf_client_audio_buff_alloc(static_cast<uint16_t>(packet.length));
  if (!buffer) return EspBleClassicAudioSendResult::WouldBlock;
  memcpy(buffer->data, packet.data, packet.length);
  buffer->data_len = static_cast<uint16_t>(packet.length);
  if (esp_hf_client_audio_data_send(audio.id, buffer) != ESP_OK)
  {
    esp_hf_client_audio_buff_free(buffer);
    return EspBleClassicAudioSendResult::BackendFailure;
  }
  return EspBleClassicAudioSendResult::Accepted;
#endif
}

bool EspBleClassicHfpClient::requestPacketStatistics()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  const auto audio = audioConnection();
  if (audio.state != EspBleClassicHfpAudioState::Connected)
  {
    owner_->setError(EspBleError::InvalidState, "HFP audio is not connected");
    return false;
  }
  if (esp_hf_client_pkt_stat_nums_get(audio.id) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to request HFP packet statistics");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

namespace
{
bool validDtmf(char code)
{
  return (code >= '0' && code <= '9') || code == '#' || code == '*' ||
    (code >= 'A' && code <= 'D');
}
} // namespace

bool EspBleClassicHfpClient::dial(const char *number)
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  (void)number;
  return false;
#else
  if (!serviceLevelConnected())
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP service-level connection is not established");
    return false;
  }
  if (!number || !number[0])
  {
    owner_->setError(EspBleError::InvalidArgument,
      "dial number is empty; use redial() for the last number");
    return false;
  }
  if (esp_hf_client_dial(number) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure, "failed to dial HFP call");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicHfpClient::redial()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  if (!serviceLevelConnected())
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP service-level connection is not established");
    return false;
  }
  if (esp_hf_client_dial(nullptr) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure, "failed to redial HFP call");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicHfpClient::finishCommand(
  bool connected, bool success, const char *failure)
{
  if (!connected)
  {
    owner_->setError(EspBleError::InvalidState,
      "HFP service-level connection is not established");
    return false;
  }
  if (!success)
  {
    owner_->setError(EspBleError::BackendFailure, failure);
    return false;
  }
  owner_->clearError();
  return true;
}

bool EspBleClassicHfpClient::answerCall()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  return finishCommand(true, esp_hf_client_answer_call() == ESP_OK,
    "failed to answer HFP call");
#endif
}

bool EspBleClassicHfpClient::rejectOrEndCall()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  return finishCommand(true, esp_hf_client_reject_call() == ESP_OK,
    "failed to reject or end HFP call");
#endif
}

bool EspBleClassicHfpClient::queryCurrentCalls()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  return finishCommand(true,
    esp_hf_client_query_current_calls() == ESP_OK,
    "failed to query current HFP calls");
#endif
}

bool EspBleClassicHfpClient::queryOperatorName()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  return finishCommand(true,
    esp_hf_client_query_current_operator_name() == ESP_OK,
    "failed to query the network operator name");
#endif
}

bool EspBleClassicHfpClient::requestSubscriberNumber()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  return finishCommand(true,
    esp_hf_client_retrieve_subscriber_info() == ESP_OK,
    "failed to request the subscriber number");
#endif
}

bool EspBleClassicHfpClient::dialMemory(int location)
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  (void)location;
  return false;
#else
  if (location < 0)
  {
    owner_->setError(
      EspBleError::InvalidArgument, "memory location must not be negative");
    return false;
  }
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  return finishCommand(true, esp_hf_client_dial_memory(location) == ESP_OK,
    "failed to dial from memory");
#endif
}

bool EspBleClassicHfpClient::requestLastVoiceTagNumber()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  return finishCommand(true,
    esp_hf_client_request_last_voice_tag_number() == ESP_OK,
    "failed to request the last voice tag number");
#endif
}

bool EspBleClassicHfpClient::disableNoiseReduction()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  return finishCommand(true, esp_hf_client_send_nrec() == ESP_OK,
    "failed to ask the phone to disable noise reduction");
#endif
}

bool EspBleClassicHfpClient::enableAppleExtensions(
  const char *identification, const EspBleClassicHfpAppleFeatures &features)
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  (void)identification;
  (void)features;
  return false;
#else
  if (identification == nullptr || identification[0] == '\0')
  {
    owner_->setError(
      EspBleError::InvalidArgument,
      "the Apple identification must be vendorId-productId-version");
    return false;
  }
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  uint32_t bits = 0;
  if (features.batteryReporting) bits |= ESP_HF_CLIENT_XAPL_FEAT_BATTERY_REPORT;
  if (features.docked) bits |= ESP_HF_CLIENT_XAPL_FEAT_DOCKED;
  if (features.siriStatus) bits |= ESP_HF_CLIENT_XAPL_FEAT_SIRI_STATUS_REPORT;
  if (features.noiseReductionStatus)
    bits |= ESP_HF_CLIENT_XAPL_NR_STATUS_REPORT;
  // The backend takes a non-const pointer but only reads it, and the copy keeps
  // the caller's string safe from a backend that might not.
  String copy(identification);
  return finishCommand(true,
    esp_hf_client_send_xapl(
      const_cast<char *>(copy.c_str()), bits) == ESP_OK,
    "failed to enable the Apple extensions");
#endif
}

bool EspBleClassicHfpClient::reportBatteryLevel(uint8_t level, bool docked)
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  (void)level;
  (void)docked;
  return false;
#else
  if (level > 9)
  {
    owner_->setError(
      EspBleError::InvalidArgument, "battery level must be between 0 and 9");
    return false;
  }
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  return finishCommand(true,
    esp_hf_client_send_iphoneaccev(level, docked) == ESP_OK,
    "failed to report the battery level");
#endif
}

bool EspBleClassicHfpClient::sendDtmf(char code)
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  (void)code;
  return false;
#else
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  if (!validDtmf(code))
  {
    owner_->setError(EspBleError::InvalidArgument, "invalid HFP DTMF code");
    return false;
  }
  return finishCommand(true, esp_hf_client_send_dtmf(code) == ESP_OK,
    "failed to send HFP DTMF code");
#endif
}

bool EspBleClassicHfpClient::setVolume(
  EspBleClassicHfpVolumeTarget target, uint8_t value)
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  (void)target;
  (void)value;
  return false;
#else
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  if (value > 15)
  {
    owner_->setError(EspBleError::InvalidArgument,
      "HFP volume must be in the range 0..15");
    return false;
  }
  return finishCommand(true,
    esp_hf_client_volume_update(
      static_cast<esp_hf_volume_control_target_t>(target), value) == ESP_OK,
    "failed to update HFP volume");
#endif
}

bool EspBleClassicHfpClient::startVoiceRecognition()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  return finishCommand(true,
    esp_hf_client_start_voice_recognition() == ESP_OK,
    "failed to start HFP voice recognition");
#endif
}

bool EspBleClassicHfpClient::stopVoiceRecognition()
{
#if !ESPBLE_CLASSIC_HFP_CLIENT_BACKEND_AVAILABLE
  return false;
#else
  if (!serviceLevelConnected())
    return finishCommand(false, false, "");
  return finishCommand(true,
    esp_hf_client_stop_voice_recognition() == ESP_OK,
    "failed to stop HFP voice recognition");
#endif
}

EspBleClassicHfpCallState EspBleClassicHfpClient::callState() const
{
  if (!impl_) return EspBleClassicHfpCallState();
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->callState;
}

void EspBleClassicHfpClient::onConnectionChanged(ConnectionCallback callback)
{ connectionCallback_ = std::move(callback); }
void EspBleClassicHfpClient::onAudioConnectionChanged(
  AudioConnectionCallback callback)
{ audioConnectionCallback_ = std::move(callback); }
void EspBleClassicHfpClient::onCallStateChanged(CallStateCallback callback)
{ callStateCallback_ = std::move(callback); }
void EspBleClassicHfpClient::onCaller(CallerCallback callback)
{ callerCallback_ = std::move(callback); }
void EspBleClassicHfpClient::onRing(RingCallback callback)
{ ringCallback_ = std::move(callback); }
void EspBleClassicHfpClient::onCurrentCall(CurrentCallCallback callback)
{ currentCallCallback_ = std::move(callback); }
void EspBleClassicHfpClient::onVolumeChanged(VolumeCallback callback)
{ volumeCallback_ = std::move(callback); }
void EspBleClassicHfpClient::onAtResponse(AtResponseCallback callback)
{ atResponseCallback_ = std::move(callback); }
void EspBleClassicHfpClient::onOperatorName(OperatorNameCallback callback)
{ operatorNameCallback_ = std::move(callback); }

void EspBleClassicHfpClient::onSubscriberNumber(
  SubscriberNumberCallback callback)
{ subscriberNumberCallback_ = std::move(callback); }

void EspBleClassicHfpClient::onVoiceTagNumber(VoiceTagNumberCallback callback)
{ voiceTagNumberCallback_ = std::move(callback); }

void EspBleClassicHfpClient::onInBandRingTone(InBandRingToneCallback callback)
{ inBandRingToneCallback_ = std::move(callback); }

void EspBleClassicHfpClient::onPacketStatistics(
  PacketStatisticsCallback callback)
{ packetStatisticsCallback_ = std::move(callback); }

void EspBleClassicHfpClient::onAudio(AudioCallback callback)
{
  if (!impl_ && callback)
  {
    impl_ = new (std::nothrow) EspBleClassicHfpClientImpl();
    if (!impl_)
    {
      owner_->setError(EspBleError::ResourceExhausted,
        "failed to allocate HFP Client state");
      return;
    }
  }
  if (!impl_) return;
  auto replacement = callback
    ? std::make_shared<AudioCallback>(std::move(callback))
    : std::shared_ptr<AudioCallback>();
  std::atomic_store_explicit(
    &impl_->audioCallback, replacement, std::memory_order_release);
  // A callback may still hold the previous callable after the atomic swap.
  // Waiting provides the same unregister/replacement barrier as A2DP.
  while (impl_->callbackUsers.load(std::memory_order_acquire) != 0) delay(1);
}

size_t EspBleClassicHfpClient::droppedEventCount() const
{
  if (!impl_) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedEvents;
}

void EspBleClassicHfpClient::update()
{
  if (!impl_) return;
  while (true)
  {
    EspBleClassicHfpClientImpl::Event event;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->eventCount == 0) break;
      event = std::move(impl_->events[impl_->eventHead]);
      impl_->eventHead = (impl_->eventHead + 1) % HfpClientEventQueueCapacity;
      --impl_->eventCount;
    }
    if (event.type == EspBleClassicHfpClientImpl::EventType::Connection &&
        connectionCallback_)
      connectionCallback_(event.connection);
    else if (event.type ==
               EspBleClassicHfpClientImpl::EventType::AudioConnection &&
             audioConnectionCallback_)
      audioConnectionCallback_(event.audioConnection);
    else if (event.type == EspBleClassicHfpClientImpl::EventType::CallState &&
             callStateCallback_)
      callStateCallback_(event.callState);
    else if (event.type == EspBleClassicHfpClientImpl::EventType::Caller &&
             callerCallback_)
      callerCallback_(event.caller);
    else if (event.type == EspBleClassicHfpClientImpl::EventType::Ring &&
             ringCallback_)
      ringCallback_();
    else if (event.type == EspBleClassicHfpClientImpl::EventType::CurrentCall &&
             currentCallCallback_)
      currentCallCallback_(event.currentCall);
    else if (event.type == EspBleClassicHfpClientImpl::EventType::Volume &&
             volumeCallback_)
      volumeCallback_(event.volume);
    else if (event.type == EspBleClassicHfpClientImpl::EventType::AtResponse &&
             atResponseCallback_)
      atResponseCallback_(event.atResponse);
    else if (event.type ==
               EspBleClassicHfpClientImpl::EventType::PacketStatistics &&
             packetStatisticsCallback_)
      packetStatisticsCallback_(event.packetStatistics);
    else if (event.type == EspBleClassicHfpClientImpl::EventType::OperatorName &&
             operatorNameCallback_)
      operatorNameCallback_(event.text);
    else if (event.type ==
               EspBleClassicHfpClientImpl::EventType::SubscriberNumber &&
             subscriberNumberCallback_)
      subscriberNumberCallback_(event.subscriberNumber);
    else if (event.type ==
               EspBleClassicHfpClientImpl::EventType::VoiceTagNumber &&
             voiceTagNumberCallback_)
      voiceTagNumberCallback_(event.text);
    else if (event.type ==
               EspBleClassicHfpClientImpl::EventType::InBandRingTone &&
             inBandRingToneCallback_)
      inBandRingToneCallback_(event.flag);
  }
}
