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
#define ESPBLE_CLASSIC_A2DP_BACKEND_AVAILABLE 1
#define esp_a2d_audio_buff_free espble_bd_esp_a2d_audio_buff_free
#define esp_a2d_register_callback espble_bd_esp_a2d_register_callback
#define esp_a2d_sink_connect espble_bd_esp_a2d_sink_connect
#define esp_a2d_sink_get_delay_value espble_bd_esp_a2d_sink_get_delay_value
#define esp_a2d_sink_set_delay_value espble_bd_esp_a2d_sink_set_delay_value
#define esp_a2d_sink_deinit espble_bd_esp_a2d_sink_deinit
#define esp_a2d_sink_disconnect espble_bd_esp_a2d_sink_disconnect
#define esp_a2d_sink_init espble_bd_esp_a2d_sink_init
#define esp_a2d_sink_register_audio_data_callback \
  espble_bd_esp_a2d_sink_register_audio_data_callback
#define esp_a2d_sink_register_stream_endpoint \
  espble_bd_esp_a2d_sink_register_stream_endpoint
#define esp_bt_gap_set_scan_mode espble_bd_esp_bt_gap_set_scan_mode
#include "esp32/include/esp_a2dp_api.h"
#include "esp32/include/esp_gap_bt_api.h"
#else
#define ESPBLE_CLASSIC_A2DP_BACKEND_AVAILABLE 0
#endif

namespace
{
constexpr size_t A2dpEventQueueCapacity = 12;

#if ESPBLE_CLASSIC_A2DP_BACKEND_AVAILABLE
String a2dpAddress(const esp_bd_addr_t address)
{
  char value[18];
  snprintf(
    value, sizeof(value), "%02x:%02x:%02x:%02x:%02x:%02x",
    address[0], address[1], address[2], address[3], address[4], address[5]);
  return String(value);
}

bool a2dpParseAddress(const char *value, esp_bd_addr_t address)
{
  if (value == nullptr) return false;
  unsigned bytes[ESP_BD_ADDR_LEN] = {};
  char trailing = '\0';
  if (sscanf(
        value, "%02x:%02x:%02x:%02x:%02x:%02x%c",
        &bytes[0], &bytes[1], &bytes[2],
        &bytes[3], &bytes[4], &bytes[5], &trailing) != ESP_BD_ADDR_LEN)
    return false;
  for (size_t index = 0; index < ESP_BD_ADDR_LEN; ++index)
  {
    if (bytes[index] > UINT8_MAX) return false;
    address[index] = static_cast<uint8_t>(bytes[index]);
  }
  return true;
}

bool waitForA2dpFlag(
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

struct EspBleClassicA2dpSinkImpl
{
  enum class EventType : uint8_t
  {
    Connected,
    Disconnected,
    CodecConfigured,
    StreamState,
    Delay,
  };

  struct Event
  {
    EventType type = EventType::Connected;
    EspBleClassicA2dpConnection connection;
    EspBleClassicA2dpCodecConfig codec;
    EspBleClassicA2dpStreamEvent stream;
    EspBleClassicA2dpDelay delay;
  };

  bool enqueue(Event event)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (eventCount == A2dpEventQueueCapacity)
    {
      ++droppedEvents;
      return false;
    }
    const size_t tail = (eventHead + eventCount) % A2dpEventQueueCapacity;
    events[tail] = std::move(event);
    ++eventCount;
    return true;
  }

  void deliverMedia(const EspBleClassicEncodedAudioView &view)
  {
    if (owner == nullptr) return;
    const std::shared_ptr<EspBleClassicA2dpSink::MediaCallback> callback =
      std::atomic_load_explicit(
        &owner->mediaCallback_, std::memory_order_acquire);
    if (callback && *callback) (*callback)(view);
  }

  mutable std::mutex mutex;
  std::atomic<size_t> callbackUsers{0};
  Event events[A2dpEventQueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  bool initializationCompleted = false;
  bool initialized = false;
  bool sepRegistrationCompleted = false;
  bool sepRegistered = false;
  bool ending = false;
  bool discoverable = true;
  bool outgoingPending = false;
  bool connected = false;
  bool streaming = false;
  EspBleClassicA2dpConnection connection;
  EspBleClassicA2dpCodecConfig codec;
  EspBleClassicA2dpSink *owner = nullptr;
#if ESPBLE_CLASSIC_A2DP_BACKEND_AVAILABLE
  esp_bd_addr_t backendAddress = {};
#else
  uint8_t backendAddress[6] = {};
#endif
};

#if ESPBLE_CLASSIC_A2DP_BACKEND_AVAILABLE
namespace
{
std::atomic<EspBleClassicA2dpSinkImpl *> activeA2dpSink{nullptr};
std::mutex a2dpCallbackTargetMutex;

class A2dpCallbackLease
{
public:
  A2dpCallbackLease()
  {
    std::lock_guard<std::mutex> lock(a2dpCallbackTargetMutex);
    impl_ = activeA2dpSink.load(std::memory_order_relaxed);
    if (impl_ != nullptr)
      impl_->callbackUsers.fetch_add(1, std::memory_order_acq_rel);
  }

  ~A2dpCallbackLease()
  {
    if (impl_ != nullptr)
      impl_->callbackUsers.fetch_sub(1, std::memory_order_acq_rel);
  }

  EspBleClassicA2dpSinkImpl *get() const { return impl_; }

private:
  EspBleClassicA2dpSinkImpl *impl_ = nullptr;
};

bool activateA2dpCallbackTarget(EspBleClassicA2dpSinkImpl *impl)
{
  std::lock_guard<std::mutex> lock(a2dpCallbackTargetMutex);
  EspBleClassicA2dpSinkImpl *current =
    activeA2dpSink.load(std::memory_order_relaxed);
  if (current != nullptr && current != impl) return false;
  activeA2dpSink.store(impl, std::memory_order_release);
  return true;
}

void deactivateA2dpCallbackTarget(EspBleClassicA2dpSinkImpl *impl)
{
  {
    std::lock_guard<std::mutex> lock(a2dpCallbackTargetMutex);
    if (activeA2dpSink.load(std::memory_order_relaxed) == impl)
      activeA2dpSink.store(nullptr, std::memory_order_release);
  }
  while (impl->callbackUsers.load(std::memory_order_acquire) != 0) delay(1);
}

EspBleClassicA2dpCodecConfig makeCodecConfig(
  EspBleClassicA2dpConnectionId connectionId,
  const esp_a2d_mcc_t &mediaCodec)
{
  EspBleClassicA2dpCodecConfig result;
  result.connectionId = connectionId;
  if (mediaCodec.type != ESP_A2D_MCT_SBC) return result;

  result.codec = EspBleClassicAudioCodec::Sbc;
  const esp_a2d_cie_sbc_t &sbc = mediaCodec.cie.sbc_info;
  if (sbc.samp_freq & ESP_A2D_SBC_CIE_SF_48K)
    result.sampleRate = 48000;
  else if (sbc.samp_freq & ESP_A2D_SBC_CIE_SF_44K)
    result.sampleRate = 44100;
  else if (sbc.samp_freq & ESP_A2D_SBC_CIE_SF_32K)
    result.sampleRate = 32000;
  else if (sbc.samp_freq & ESP_A2D_SBC_CIE_SF_16K)
    result.sampleRate = 16000;
  result.channels =
    sbc.ch_mode == ESP_A2D_SBC_CIE_CH_MODE_MONO ? 1 : 2;
  if (sbc.ch_mode == ESP_A2D_SBC_CIE_CH_MODE_MONO)
    result.sbcChannelMode = EspBleClassicSbcChannelMode::Mono;
  else if (sbc.ch_mode == ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL)
    result.sbcChannelMode = EspBleClassicSbcChannelMode::DualChannel;
  else if (sbc.ch_mode == ESP_A2D_SBC_CIE_CH_MODE_STEREO)
    result.sbcChannelMode = EspBleClassicSbcChannelMode::Stereo;
  else if (sbc.ch_mode == ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO)
    result.sbcChannelMode = EspBleClassicSbcChannelMode::JointStereo;
  if (sbc.block_len == ESP_A2D_SBC_CIE_BLOCK_LEN_4) result.sbcBlockLength = 4;
  else if (sbc.block_len == ESP_A2D_SBC_CIE_BLOCK_LEN_8) result.sbcBlockLength = 8;
  else if (sbc.block_len == ESP_A2D_SBC_CIE_BLOCK_LEN_12) result.sbcBlockLength = 12;
  else if (sbc.block_len == ESP_A2D_SBC_CIE_BLOCK_LEN_16) result.sbcBlockLength = 16;
  if (sbc.num_subbands == ESP_A2D_SBC_CIE_NUM_SUBBANDS_4)
    result.sbcSubbands = 4;
  else if (sbc.num_subbands == ESP_A2D_SBC_CIE_NUM_SUBBANDS_8)
    result.sbcSubbands = 8;
  if (sbc.alloc_mthd == ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR)
    result.sbcAllocationMethod = EspBleClassicSbcAllocationMethod::Snr;
  else if (sbc.alloc_mthd == ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS)
    result.sbcAllocationMethod = EspBleClassicSbcAllocationMethod::Loudness;
  result.minimumBitpool = sbc.min_bitpool;
  result.maximumBitpool = sbc.max_bitpool;
  result.rawLength = sizeof(sbc) < sizeof(result.raw) ?
    sizeof(sbc) : sizeof(result.raw);
  memcpy(result.raw, &sbc, result.rawLength);
  return result;
}

void a2dpProfileCallback(
  esp_a2d_cb_event_t event, esp_a2d_cb_param_t *parameter)
{
  A2dpCallbackLease lease;
  EspBleClassicA2dpSinkImpl *impl = lease.get();
  if (impl == nullptr || parameter == nullptr) return;

  if (event == ESP_A2D_PROF_STATE_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->initialized =
      parameter->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS;
    impl->initializationCompleted = true;
    return;
  }
  if (event == ESP_A2D_SEP_REG_STATE_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->sepRegistered =
      parameter->a2d_sep_reg_stat.reg_state == ESP_A2D_SEP_REG_SUCCESS;
    impl->sepRegistrationCompleted = true;
    return;
  }
  if (event == ESP_A2D_CONNECTION_STATE_EVT)
  {
    if (parameter->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED)
    {
      EspBleClassicA2dpSinkImpl::Event queued;
      queued.type = EspBleClassicA2dpSinkImpl::EventType::Connected;
      queued.connection.id = parameter->conn_stat.conn_hdl;
      queued.connection.peerAddress =
        a2dpAddress(parameter->conn_stat.remote_bda);
      queued.connection.mediaMtu = parameter->conn_stat.audio_mtu;
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (impl->ending) return;
        queued.connection.incoming = !impl->outgoingPending;
        impl->outgoingPending = false;
        impl->connected = true;
        impl->streaming = false;
        impl->connection = queued.connection;
        memcpy(
          impl->backendAddress, parameter->conn_stat.remote_bda,
          sizeof(impl->backendAddress));
      }
      (void)EspBleClassicVisibilityOwner::hideWhileExclusivelyConnected();
      impl->enqueue(std::move(queued));
    }
    else if (parameter->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
    {
      EspBleClassicA2dpSinkImpl::Event queued;
      queued.type = EspBleClassicA2dpSinkImpl::EventType::Disconnected;
      bool restoreDiscoverability = false;
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (!impl->connected && !impl->outgoingPending) return;
        queued.connection = impl->connection;
        if (queued.connection.peerAddress.isEmpty())
          queued.connection.peerAddress =
            a2dpAddress(parameter->conn_stat.remote_bda);
        impl->outgoingPending = false;
        impl->connected = false;
        impl->streaming = false;
        impl->connection = EspBleClassicA2dpConnection();
        impl->codec = EspBleClassicA2dpCodecConfig();
        memset(impl->backendAddress, 0, sizeof(impl->backendAddress));
        restoreDiscoverability = impl->discoverable && !impl->ending;
      }
      if (restoreDiscoverability)
        (void)EspBleClassicVisibilityOwner::apply();
      impl->enqueue(std::move(queued));
    }
    return;
  }
  if (event == ESP_A2D_AUDIO_CFG_EVT)
  {
    EspBleClassicA2dpSinkImpl::Event queued;
    queued.type = EspBleClassicA2dpSinkImpl::EventType::CodecConfigured;
    queued.codec = makeCodecConfig(
      parameter->audio_cfg.conn_hdl, parameter->audio_cfg.mcc);
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->ending) return;
      impl->codec = queued.codec;
    }
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_A2D_AUDIO_STATE_EVT)
  {
    EspBleClassicA2dpSinkImpl::Event queued;
    queued.type = EspBleClassicA2dpSinkImpl::EventType::StreamState;
    queued.stream.connectionId = parameter->audio_stat.conn_hdl;
    queued.stream.state = parameter->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED ?
      EspBleClassicA2dpStreamState::Started :
      EspBleClassicA2dpStreamState::Suspended;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->ending) return;
      impl->streaming =
        queued.stream.state == EspBleClassicA2dpStreamState::Started;
    }
    impl->enqueue(std::move(queued));
  }
  if (
    event == ESP_A2D_SNK_SET_DELAY_VALUE_EVT ||
    event == ESP_A2D_SNK_GET_DELAY_VALUE_EVT)
  {
    EspBleClassicA2dpSinkImpl::Event queued;
    queued.type = EspBleClassicA2dpSinkImpl::EventType::Delay;
    if (event == ESP_A2D_SNK_SET_DELAY_VALUE_EVT)
    {
      queued.delay.success = parameter->a2d_set_delay_value_stat.set_state ==
        ESP_A2D_SET_SUCCESS;
      queued.delay.tenthsOfMilliseconds =
        parameter->a2d_set_delay_value_stat.delay_value;
    }
    else
    {
      // The get has no status of its own: a value arriving is the success.
      queued.delay.success = true;
      queued.delay.tenthsOfMilliseconds =
        parameter->a2d_get_delay_value_stat.delay_value;
    }
    impl->enqueue(std::move(queued));
  }
}

void a2dpMediaCallback(
  esp_a2d_conn_hdl_t connectionId,
  esp_a2d_audio_buff_t *audio)
{
  if (audio == nullptr) return;
  A2dpCallbackLease lease;
  EspBleClassicA2dpSinkImpl *impl = lease.get();
  if (impl != nullptr)
  {
    EspBleClassicEncodedAudioView view;
    bool deliver = false;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      deliver = !impl->ending && impl->connected;
      view.codec = impl->codec.codec;
    }
    if (deliver)
    {
      view.connectionId = connectionId;
      view.timestamp = audio->timestamp;
      view.frameCount = audio->number_frame;
      view.data = audio->data;
      view.length = audio->data_len;
      impl->deliverMedia(view);
    }
  }
  esp_a2d_audio_buff_free(audio);
}
} // namespace
#endif

EspBleClassicA2dpSink::EspBleClassicA2dpSink(EspBleClassic *owner) :
  owner_(owner) {}

EspBleClassicA2dpSink::~EspBleClassicA2dpSink()
{
  end();
  delete impl_;
}

bool EspBleClassicA2dpSink::begin(
  const EspBleClassicA2dpSinkConfig &config)
{
#if !ESPBLE_CLASSIC_A2DP_BACKEND_AVAILABLE
  (void)config;
  owner_->setError(
    EspBleError::BackendFailure,
    "Classic A2DP requires the custom ESP32 Classic host build");
  return false;
#else
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "Classic stack is not initialized");
    return false;
  }
  if (owner_->a2dpSource().initialized())
  {
    owner_->setError(
      EspBleError::InvalidState,
      "A2DP Sink and Source roles are mutually exclusive");
    return false;
  }
  if (initialized())
  {
    owner_->clearError();
    return true;
  }
  if (impl_ == nullptr)
  {
    impl_ = new (std::nothrow) EspBleClassicA2dpSinkImpl();
    if (impl_ == nullptr)
    {
      owner_->setError(
        EspBleError::ResourceExhausted,
        "failed to allocate A2DP Sink state");
      return false;
    }
    impl_->owner = this;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = false;
    impl_->discoverable = config.discoverable;
    impl_->initializationCompleted = false;
    impl_->sepRegistrationCompleted = false;
  }
  if (!activateA2dpCallbackTarget(impl_))
  {
    owner_->setError(
      EspBleError::InvalidState, "another A2DP Sink profile is active");
    return false;
  }
  if (
    esp_a2d_register_callback(a2dpProfileCallback) != ESP_OK ||
    esp_a2d_sink_register_audio_data_callback(a2dpMediaCallback) != ESP_OK ||
    esp_a2d_sink_init() != ESP_OK ||
    !waitForA2dpFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return impl_->initializationCompleted;
    }) || !initialized())
  {
    deactivateA2dpCallbackTarget(impl_);
    owner_->setError(
      EspBleError::BackendFailure, "failed to initialize A2DP Sink profile");
    return false;
  }

  esp_a2d_mcc_t mediaCodec = {};
  mediaCodec.type = ESP_A2D_MCT_SBC;
  mediaCodec.cie.sbc_info.samp_freq =
    ESP_A2D_SBC_CIE_SF_16K | ESP_A2D_SBC_CIE_SF_32K |
    ESP_A2D_SBC_CIE_SF_44K | ESP_A2D_SBC_CIE_SF_48K;
  mediaCodec.cie.sbc_info.ch_mode =
    ESP_A2D_SBC_CIE_CH_MODE_MONO |
    ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL |
    ESP_A2D_SBC_CIE_CH_MODE_STEREO |
    ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO;
  mediaCodec.cie.sbc_info.block_len =
    ESP_A2D_SBC_CIE_BLOCK_LEN_4 | ESP_A2D_SBC_CIE_BLOCK_LEN_8 |
    ESP_A2D_SBC_CIE_BLOCK_LEN_12 | ESP_A2D_SBC_CIE_BLOCK_LEN_16;
  mediaCodec.cie.sbc_info.num_subbands =
    ESP_A2D_SBC_CIE_NUM_SUBBANDS_4 | ESP_A2D_SBC_CIE_NUM_SUBBANDS_8;
  mediaCodec.cie.sbc_info.alloc_mthd =
    ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR |
    ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS;
  mediaCodec.cie.sbc_info.min_bitpool = 2;
  mediaCodec.cie.sbc_info.max_bitpool = 250;
  if (
    esp_a2d_sink_register_stream_endpoint(0, &mediaCodec) != ESP_OK ||
    !waitForA2dpFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return impl_->sepRegistrationCompleted;
    }) || !impl_->sepRegistered)
  {
    end();
    owner_->setError(
      EspBleError::BackendFailure,
      "failed to register the A2DP SBC stream endpoint");
    return false;
  }
  if (config.discoverable && !EspBleClassicVisibilityOwner::apply())
  {
    end();
    owner_->setError(
      EspBleError::BackendFailure, "failed to make A2DP Sink discoverable");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

void EspBleClassicA2dpSink::end()
{
  if (impl_ == nullptr) return;
#if ESPBLE_CLASSIC_A2DP_BACKEND_AVAILABLE
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
  if (wasConnected) (void)esp_a2d_sink_disconnect(address);
  if (wasConnected)
  {
    (void)waitForA2dpFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return !impl_->connected;
    });
  }
  if (wasInitialized) (void)esp_a2d_sink_deinit();
  if (wasInitialized)
  {
    (void)waitForA2dpFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return !impl_->initialized;
    });
  }
  deactivateA2dpCallbackTarget(impl_);
#endif
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->eventHead = 0;
  impl_->eventCount = 0;
  impl_->initializationCompleted = false;
  impl_->initialized = false;
  impl_->sepRegistrationCompleted = false;
  impl_->sepRegistered = false;
  impl_->ending = false;
  impl_->outgoingPending = false;
  impl_->connected = false;
  impl_->streaming = false;
  impl_->connection = EspBleClassicA2dpConnection();
  impl_->codec = EspBleClassicA2dpCodecConfig();
  memset(impl_->backendAddress, 0, sizeof(impl_->backendAddress));
}

bool EspBleClassicA2dpSink::initialized() const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->initialized;
}

bool EspBleClassicA2dpSink::connect(const char *address)
{
#if !ESPBLE_CLASSIC_A2DP_BACKEND_AVAILABLE
  (void)address;
  return false;
#else
  if (!initialized())
  {
    owner_->setError(EspBleError::InvalidState, "A2DP Sink is not initialized");
    return false;
  }
  esp_bd_addr_t backendAddress = {};
  if (!a2dpParseAddress(address, backendAddress))
  {
    owner_->setError(EspBleError::InvalidArgument, "A2DP peer address is malformed");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->connected || impl_->outgoingPending)
    {
      owner_->setError(EspBleError::InvalidState, "an A2DP connection is already active");
      return false;
    }
    impl_->outgoingPending = true;
    memcpy(impl_->backendAddress, backendAddress, sizeof(backendAddress));
  }
  if (esp_a2d_sink_connect(backendAddress) != ESP_OK)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->outgoingPending = false;
    memset(impl_->backendAddress, 0, sizeof(impl_->backendAddress));
    owner_->setError(EspBleError::BackendFailure, "failed to request A2DP connection");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicA2dpSink::disconnect()
{
#if !ESPBLE_CLASSIC_A2DP_BACKEND_AVAILABLE
  return false;
#else
  if (impl_ == nullptr)
  {
    owner_->setError(EspBleError::InvalidState, "A2DP Sink is not initialized");
    return false;
  }
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->connected) return false;
    memcpy(address, impl_->backendAddress, sizeof(address));
  }
  if (esp_a2d_sink_disconnect(address) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure, "failed to disconnect A2DP Sink");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicA2dpSink::connected() const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->connected;
}

bool EspBleClassicA2dpSink::streaming() const
{
  if (impl_ == nullptr) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->streaming;
}

EspBleClassicA2dpConnection EspBleClassicA2dpSink::connection() const
{
  if (impl_ == nullptr) return EspBleClassicA2dpConnection();
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->connection;
}

EspBleClassicA2dpCodecConfig EspBleClassicA2dpSink::codecConfig() const
{
  if (impl_ == nullptr) return EspBleClassicA2dpCodecConfig();
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->codec;
}

void EspBleClassicA2dpSink::onConnected(ConnectionCallback callback)
{
  connectedCallback_ = std::move(callback);
}

void EspBleClassicA2dpSink::onDisconnected(ConnectionCallback callback)
{
  disconnectedCallback_ = std::move(callback);
}

void EspBleClassicA2dpSink::onCodecConfigured(
  CodecConfiguredCallback callback)
{
  codecConfiguredCallback_ = std::move(callback);
}

void EspBleClassicA2dpSink::onStreamStateChanged(StreamCallback callback)
{
  streamCallback_ = std::move(callback);
}

void EspBleClassicA2dpSink::onMedia(MediaCallback callback)
{
  std::shared_ptr<MediaCallback> replacement;
  if (callback)
    replacement = std::make_shared<MediaCallback>(std::move(callback));
  std::atomic_store_explicit(
    &mediaCallback_, std::move(replacement), std::memory_order_release);

  // A callback which acquired its lease before the atomic replacement may
  // still hold the old callable. Returning only after those leases drain gives
  // adapters a real unregister barrier. Calling this method from onMedia()
  // itself is therefore unsupported (as is end() from that callback).
  if (impl_ != nullptr)
    while (impl_->callbackUsers.load(std::memory_order_acquire) != 0) delay(1);
}

size_t EspBleClassicA2dpSink::droppedEventCount() const
{
  if (impl_ == nullptr) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedEvents;
}

void EspBleClassicA2dpSink::update()
{
  if (impl_ == nullptr) return;
  while (true)
  {
    EspBleClassicA2dpSinkImpl::Event event;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->eventCount == 0) break;
      event = std::move(impl_->events[impl_->eventHead]);
      impl_->eventHead = (impl_->eventHead + 1) % A2dpEventQueueCapacity;
      --impl_->eventCount;
    }
    if (event.type == EspBleClassicA2dpSinkImpl::EventType::Connected &&
        connectedCallback_)
      connectedCallback_(event.connection);
    else if (event.type == EspBleClassicA2dpSinkImpl::EventType::Disconnected &&
             disconnectedCallback_)
      disconnectedCallback_(event.connection);
    else if (event.type == EspBleClassicA2dpSinkImpl::EventType::CodecConfigured &&
             codecConfiguredCallback_)
      codecConfiguredCallback_(event.codec);
    else if (event.type == EspBleClassicA2dpSinkImpl::EventType::StreamState &&
             streamCallback_)
      streamCallback_(event.stream);
    else if (event.type == EspBleClassicA2dpSinkImpl::EventType::Delay &&
             delayCallback_)
      delayCallback_(event.delay);
  }
}

bool EspBleClassicA2dpSink::setDelay(uint16_t tenthsOfMilliseconds)
{
#if !ESPBLE_CLASSIC_A2DP_BACKEND_AVAILABLE
  (void)tenthsOfMilliseconds;
  owner_->setError(
    EspBleError::BackendFailure, "Classic A2DP is unavailable");
  return false;
#else
  if (!initialized())
  {
    owner_->setError(EspBleError::InvalidState, "A2DP Sink is not started");
    return false;
  }
  if (esp_a2d_sink_set_delay_value(tenthsOfMilliseconds) != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to set the A2DP delay value");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicA2dpSink::requestDelay()
{
#if !ESPBLE_CLASSIC_A2DP_BACKEND_AVAILABLE
  owner_->setError(
    EspBleError::BackendFailure, "Classic A2DP is unavailable");
  return false;
#else
  if (!initialized())
  {
    owner_->setError(EspBleError::InvalidState, "A2DP Sink is not started");
    return false;
  }
  if (esp_a2d_sink_get_delay_value() != ESP_OK)
  {
    owner_->setError(
      EspBleError::BackendFailure, "failed to read the A2DP delay value");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

void EspBleClassicA2dpSink::onDelay(DelayCallback callback)
{
  delayCallback_ = std::move(callback);
}
