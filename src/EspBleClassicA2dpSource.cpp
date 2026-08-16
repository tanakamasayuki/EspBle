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
#define ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE 1
#define esp_a2d_audio_buff_alloc espble_bd_esp_a2d_audio_buff_alloc
#define esp_a2d_audio_buff_free espble_bd_esp_a2d_audio_buff_free
#define esp_a2d_media_ctrl espble_bd_esp_a2d_media_ctrl
#define esp_a2d_register_callback espble_bd_esp_a2d_register_callback
#define esp_a2d_source_audio_data_send espble_bd_esp_a2d_source_audio_data_send
#define esp_a2d_source_connect espble_bd_esp_a2d_source_connect
#define esp_a2d_source_deinit espble_bd_esp_a2d_source_deinit
#define esp_a2d_source_disconnect espble_bd_esp_a2d_source_disconnect
#define esp_a2d_source_init espble_bd_esp_a2d_source_init
#define esp_a2d_source_register_stream_endpoint \
  espble_bd_esp_a2d_source_register_stream_endpoint
#include <esp_a2dp_api.h>
#include "EspBleClassicCoreCompat.h"
#else
#define ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE 0
#endif

namespace
{
constexpr size_t A2dpSourceEventQueueCapacity = 12;

#if ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE
String sourceAddress(const esp_bd_addr_t address)
{
  char value[18];
  snprintf(
    value, sizeof(value), "%02x:%02x:%02x:%02x:%02x:%02x",
    address[0], address[1], address[2], address[3], address[4], address[5]);
  return String(value);
}

bool parseSourceAddress(const char *value, esp_bd_addr_t address)
{
  if (value == nullptr) return false;
  unsigned bytes[ESP_BD_ADDR_LEN] = {};
  char trailing = '\0';
  if (sscanf(
        value, "%02x:%02x:%02x:%02x:%02x:%02x%c",
        &bytes[0], &bytes[1], &bytes[2], &bytes[3],
        &bytes[4], &bytes[5], &trailing) != ESP_BD_ADDR_LEN)
    return false;
  for (size_t index = 0; index < ESP_BD_ADDR_LEN; ++index)
  {
    if (bytes[index] > UINT8_MAX) return false;
    address[index] = static_cast<uint8_t>(bytes[index]);
  }
  return true;
}

bool waitForSourceFlag(
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

bool makeSourceCodec(
  const EspBleClassicA2dpSourceConfig &config,
  esp_a2d_mcc_t &codec)
{
  codec = {};
  codec.type = ESP_A2D_MCT_SBC;
  switch (config.sampleRate)
  {
    case 16000: codec.cie.sbc_info.samp_freq = ESP_A2D_SBC_CIE_SF_16K; break;
    case 32000: codec.cie.sbc_info.samp_freq = ESP_A2D_SBC_CIE_SF_32K; break;
    case 44100: codec.cie.sbc_info.samp_freq = ESP_A2D_SBC_CIE_SF_44K; break;
    case 48000: codec.cie.sbc_info.samp_freq = ESP_A2D_SBC_CIE_SF_48K; break;
    default: return false;
  }
  switch (config.channelMode)
  {
    case EspBleClassicSbcChannelMode::Mono:
      codec.cie.sbc_info.ch_mode = ESP_A2D_SBC_CIE_CH_MODE_MONO; break;
    case EspBleClassicSbcChannelMode::DualChannel:
      codec.cie.sbc_info.ch_mode = ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL; break;
    case EspBleClassicSbcChannelMode::Stereo:
      codec.cie.sbc_info.ch_mode = ESP_A2D_SBC_CIE_CH_MODE_STEREO; break;
    case EspBleClassicSbcChannelMode::JointStereo:
      codec.cie.sbc_info.ch_mode = ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO; break;
    default: return false;
  }
  switch (config.blockLength)
  {
    case 4: codec.cie.sbc_info.block_len = ESP_A2D_SBC_CIE_BLOCK_LEN_4; break;
    case 8: codec.cie.sbc_info.block_len = ESP_A2D_SBC_CIE_BLOCK_LEN_8; break;
    case 12: codec.cie.sbc_info.block_len = ESP_A2D_SBC_CIE_BLOCK_LEN_12; break;
    case 16: codec.cie.sbc_info.block_len = ESP_A2D_SBC_CIE_BLOCK_LEN_16; break;
    default: return false;
  }
  if (config.subbands == 4)
    codec.cie.sbc_info.num_subbands = ESP_A2D_SBC_CIE_NUM_SUBBANDS_4;
  else if (config.subbands == 8)
    codec.cie.sbc_info.num_subbands = ESP_A2D_SBC_CIE_NUM_SUBBANDS_8;
  else
    return false;
  if (config.allocationMethod == EspBleClassicSbcAllocationMethod::Snr)
    codec.cie.sbc_info.alloc_mthd = ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR;
  else if (config.allocationMethod == EspBleClassicSbcAllocationMethod::Loudness)
    codec.cie.sbc_info.alloc_mthd = ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS;
  else
    return false;
  if (config.minimumBitpool < 2 ||
      config.maximumBitpool < config.minimumBitpool ||
      config.maximumBitpool > 250)
    return false;
  codec.cie.sbc_info.min_bitpool = config.minimumBitpool;
  codec.cie.sbc_info.max_bitpool = config.maximumBitpool;
  return true;
}

EspBleClassicA2dpCodecConfig sourceCodecConfig(
  EspBleClassicA2dpConnectionId connectionId,
  const esp_a2d_mcc_t &mediaCodec)
{
  EspBleClassicA2dpCodecConfig result;
  result.connectionId = connectionId;
  if (mediaCodec.type != ESP_A2D_MCT_SBC) return result;
  result.codec = EspBleClassicAudioCodec::Sbc;
  const esp_a2d_cie_sbc_t &sbc = mediaCodec.cie.sbc_info;
  if (sbc.samp_freq & ESP_A2D_SBC_CIE_SF_48K) result.sampleRate = 48000;
  else if (sbc.samp_freq & ESP_A2D_SBC_CIE_SF_44K) result.sampleRate = 44100;
  else if (sbc.samp_freq & ESP_A2D_SBC_CIE_SF_32K) result.sampleRate = 32000;
  else if (sbc.samp_freq & ESP_A2D_SBC_CIE_SF_16K) result.sampleRate = 16000;
  if (sbc.ch_mode == ESP_A2D_SBC_CIE_CH_MODE_MONO)
    result.sbcChannelMode = EspBleClassicSbcChannelMode::Mono;
  else if (sbc.ch_mode == ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL)
    result.sbcChannelMode = EspBleClassicSbcChannelMode::DualChannel;
  else if (sbc.ch_mode == ESP_A2D_SBC_CIE_CH_MODE_STEREO)
    result.sbcChannelMode = EspBleClassicSbcChannelMode::Stereo;
  else if (sbc.ch_mode == ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO)
    result.sbcChannelMode = EspBleClassicSbcChannelMode::JointStereo;
  result.channels = result.sbcChannelMode == EspBleClassicSbcChannelMode::Mono ? 1 : 2;
  if (sbc.block_len == ESP_A2D_SBC_CIE_BLOCK_LEN_4) result.sbcBlockLength = 4;
  else if (sbc.block_len == ESP_A2D_SBC_CIE_BLOCK_LEN_8) result.sbcBlockLength = 8;
  else if (sbc.block_len == ESP_A2D_SBC_CIE_BLOCK_LEN_12) result.sbcBlockLength = 12;
  else if (sbc.block_len == ESP_A2D_SBC_CIE_BLOCK_LEN_16) result.sbcBlockLength = 16;
  result.sbcSubbands = sbc.num_subbands == ESP_A2D_SBC_CIE_NUM_SUBBANDS_4 ? 4 : 8;
  result.sbcAllocationMethod =
    sbc.alloc_mthd == ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR ?
      EspBleClassicSbcAllocationMethod::Snr :
      EspBleClassicSbcAllocationMethod::Loudness;
  result.minimumBitpool = sbc.min_bitpool;
  result.maximumBitpool = sbc.max_bitpool;
  result.rawLength = sizeof(sbc) < sizeof(result.raw) ? sizeof(sbc) : sizeof(result.raw);
  memcpy(result.raw, &sbc, result.rawLength);
  return result;
}
#endif
} // namespace

struct EspBleClassicA2dpSourceImpl
{
  enum class EventType : uint8_t
  {
    Connected,
    Disconnected,
    Codec,
    Stream,
    SinkDelay,
  };
  struct Event
  {
    EventType type = EventType::Connected;
    EspBleClassicA2dpConnection connection;
    EspBleClassicA2dpCodecConfig codec;
    EspBleClassicA2dpStreamEvent stream;
    EspBleClassicA2dpDelay sinkDelay;
  };
  bool enqueue(Event event)
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (eventCount == A2dpSourceEventQueueCapacity)
    {
      ++droppedEvents;
      return false;
    }
    events[(eventHead + eventCount) % A2dpSourceEventQueueCapacity] =
      std::move(event);
    ++eventCount;
    return true;
  }
  mutable std::mutex mutex;
  std::atomic<size_t> callbackUsers{0};
  Event events[A2dpSourceEventQueueCapacity];
  size_t eventHead = 0;
  size_t eventCount = 0;
  size_t droppedEvents = 0;
  bool initializationCompleted = false;
  bool initialized = false;
  bool sepRegistrationCompleted = false;
  bool sepRegistered = false;
  bool ending = false;
  bool outgoingPending = false;
  bool connected = false;
  bool streaming = false;
  bool startPending = false;
  EspBleClassicA2dpConnection connection;
  EspBleClassicA2dpCodecConfig codec;
  EspBleClassicA2dpSource *owner = nullptr;
#if ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE
  esp_bd_addr_t backendAddress = {};
#else
  uint8_t backendAddress[6] = {};
#endif
};

#if ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE
namespace
{
std::atomic<EspBleClassicA2dpSourceImpl *> activeA2dpSource{nullptr};
std::mutex sourceTargetMutex;

class SourceCallbackLease
{
public:
  SourceCallbackLease()
  {
    std::lock_guard<std::mutex> lock(sourceTargetMutex);
    impl_ = activeA2dpSource.load(std::memory_order_relaxed);
    if (impl_) impl_->callbackUsers.fetch_add(1, std::memory_order_acq_rel);
  }
  ~SourceCallbackLease()
  {
    if (impl_) impl_->callbackUsers.fetch_sub(1, std::memory_order_acq_rel);
  }
  EspBleClassicA2dpSourceImpl *get() const { return impl_; }
private:
  EspBleClassicA2dpSourceImpl *impl_ = nullptr;
};

bool activateSource(EspBleClassicA2dpSourceImpl *impl)
{
  std::lock_guard<std::mutex> lock(sourceTargetMutex);
  if (activeA2dpSource.load(std::memory_order_relaxed) != nullptr) return false;
  activeA2dpSource.store(impl, std::memory_order_release);
  return true;
}

void deactivateSource(EspBleClassicA2dpSourceImpl *impl)
{
  {
    std::lock_guard<std::mutex> lock(sourceTargetMutex);
    if (activeA2dpSource.load(std::memory_order_relaxed) == impl)
      activeA2dpSource.store(nullptr, std::memory_order_release);
  }
  while (impl->callbackUsers.load(std::memory_order_acquire) != 0) delay(1);
}

void sourceProfileCallback(
  esp_a2d_cb_event_t event, esp_a2d_cb_param_t *parameter)
{
  SourceCallbackLease lease;
  EspBleClassicA2dpSourceImpl *impl = lease.get();
  if (!impl || !parameter) return;
  if (event == ESP_A2D_PROF_STATE_EVT)
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->initialized = parameter->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS;
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
      EspBleClassicA2dpSourceImpl::Event queued;
      queued.type = EspBleClassicA2dpSourceImpl::EventType::Connected;
      queued.connection.id = parameter->conn_stat.conn_hdl;
      queued.connection.peerAddress = sourceAddress(parameter->conn_stat.remote_bda);
      queued.connection.mediaMtu = parameter->conn_stat.audio_mtu;
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (impl->ending) return;
        queued.connection.incoming = !impl->outgoingPending;
        impl->outgoingPending = false;
        impl->connected = true;
        impl->streaming = false;
        impl->connection = queued.connection;
        memcpy(impl->backendAddress, parameter->conn_stat.remote_bda,
          sizeof(impl->backendAddress));
      }
      impl->enqueue(std::move(queued));
    }
    else if (parameter->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
    {
      EspBleClassicA2dpSourceImpl::Event queued;
      queued.type = EspBleClassicA2dpSourceImpl::EventType::Disconnected;
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (!impl->connected && !impl->outgoingPending) return;
        queued.connection = impl->connection;
        if (queued.connection.peerAddress.isEmpty())
          queued.connection.peerAddress = sourceAddress(parameter->conn_stat.remote_bda);
        impl->outgoingPending = false;
        impl->connected = false;
        impl->streaming = false;
        impl->startPending = false;
        impl->connection = {};
        impl->codec = {};
        memset(impl->backendAddress, 0, sizeof(impl->backendAddress));
      }
      impl->enqueue(std::move(queued));
    }
    return;
  }
  if (event == ESP_A2D_AUDIO_CFG_EVT)
  {
    EspBleClassicA2dpSourceImpl::Event queued;
    queued.type = EspBleClassicA2dpSourceImpl::EventType::Codec;
    queued.codec = sourceCodecConfig(
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
    EspBleClassicA2dpSourceImpl::Event queued;
    queued.type = EspBleClassicA2dpSourceImpl::EventType::Stream;
    queued.stream.connectionId = parameter->audio_stat.conn_hdl;
    queued.stream.state = parameter->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED ?
      EspBleClassicA2dpStreamState::Started : EspBleClassicA2dpStreamState::Suspended;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->ending) return;
      impl->streaming = queued.stream.state == EspBleClassicA2dpStreamState::Started;
      impl->startPending = false;
    }
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_A2D_REPORT_SNK_DELAY_VALUE_EVT)
  {
    EspBleClassicA2dpSourceImpl::Event queued;
    queued.type = EspBleClassicA2dpSourceImpl::EventType::SinkDelay;
    // The Sink sends this on its own; nothing on this side asked for it, so
    // there is no request status to report — the value arriving is the answer.
    queued.sinkDelay.success = true;
    queued.sinkDelay.tenthsOfMilliseconds =
      parameter->a2d_report_delay_value_stat.delay_value;
    impl->enqueue(std::move(queued));
    return;
  }
  if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT)
  {
    bool requestStart = false;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (impl->ending) return;
      if (parameter->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY)
      {
        requestStart = impl->startPending &&
          parameter->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS;
        if (!requestStart) impl->startPending = false;
      }
      else if (parameter->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START &&
               parameter->media_ctrl_stat.status != ESP_A2D_MEDIA_CTRL_ACK_SUCCESS)
        impl->startPending = false;
    }
    if (requestStart && esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START) != ESP_OK)
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->startPending = false;
    }
  }
}
} // namespace
#endif

EspBleClassicA2dpSource::EspBleClassicA2dpSource(EspBleClassic *owner) :
  owner_(owner) {}

EspBleClassicA2dpSource::~EspBleClassicA2dpSource()
{
  end();
  delete impl_;
}

bool EspBleClassicA2dpSource::begin(
  const EspBleClassicA2dpSourceConfig &config)
{
#if !ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE
  (void)config;
  owner_->setError(EspBleError::BackendFailure,
    "Classic A2DP requires the custom ESP32 Classic host build");
  return false;
#else
  if (!owner_->initialized())
  {
    owner_->setError(EspBleError::InvalidState, "Classic stack is not initialized");
    return false;
  }
  if (owner_->a2dpSink().initialized())
  {
    owner_->setError(EspBleError::InvalidState,
      "A2DP Sink and Source roles are mutually exclusive");
    return false;
  }
  if (initialized()) { owner_->clearError(); return true; }
  esp_a2d_mcc_t backendCodec = {};
  if (!makeSourceCodec(config, backendCodec))
  {
    owner_->setError(EspBleError::InvalidArgument,
      "A2DP Source SBC configuration is invalid");
    return false;
  }
  if (!impl_)
  {
    impl_ = new (std::nothrow) EspBleClassicA2dpSourceImpl();
    if (!impl_)
    {
      owner_->setError(EspBleError::ResourceExhausted,
        "failed to allocate A2DP Source state");
      return false;
    }
    impl_->owner = this;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->ending = false;
    impl_->initializationCompleted = false;
    impl_->sepRegistrationCompleted = false;
  }
  if (!activateSource(impl_))
  {
    owner_->setError(EspBleError::InvalidState,
      "another A2DP Source profile is active");
    return false;
  }
  if (esp_a2d_register_callback(sourceProfileCallback) != ESP_OK ||
      esp_a2d_source_init() != ESP_OK ||
      !waitForSourceFlag([this]() {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->initializationCompleted;
      }) || !initialized())
  {
    deactivateSource(impl_);
    owner_->setError(EspBleError::BackendFailure,
      "failed to initialize A2DP Source profile");
    return false;
  }
  if (esp_a2d_source_register_stream_endpoint(0, &backendCodec) != ESP_OK ||
      !waitForSourceFlag([this]() {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->sepRegistrationCompleted;
      }) || !impl_->sepRegistered)
  {
    end();
    owner_->setError(EspBleError::BackendFailure,
      "failed to register the A2DP Source SBC stream endpoint");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

void EspBleClassicA2dpSource::end()
{
  if (!impl_) return;
#if ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE
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
  if (wasConnected) (void)esp_a2d_source_disconnect(address);
  if (wasConnected)
    (void)waitForSourceFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return !impl_->connected;
    });
  if (wasInitialized) (void)esp_a2d_source_deinit();
  if (wasInitialized)
    (void)waitForSourceFlag([this]() {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      return !impl_->initialized;
    });
  deactivateSource(impl_);
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
  impl_->startPending = false;
  impl_->connection = {};
  impl_->codec = {};
  memset(impl_->backendAddress, 0, sizeof(impl_->backendAddress));
}

bool EspBleClassicA2dpSource::initialized() const
{
  if (!impl_) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->initialized;
}

bool EspBleClassicA2dpSource::connect(const char *address)
{
#if !ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE
  (void)address;
  return false;
#else
  if (!initialized())
  {
    owner_->setError(EspBleError::InvalidState, "A2DP Source is not initialized");
    return false;
  }
  esp_bd_addr_t backendAddress = {};
  if (!parseSourceAddress(address, backendAddress))
  {
    owner_->setError(EspBleError::InvalidArgument, "A2DP peer address is malformed");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->connected || impl_->outgoingPending)
    {
      owner_->setError(EspBleError::InvalidState,
        "an A2DP connection is already active");
      return false;
    }
    impl_->outgoingPending = true;
    memcpy(impl_->backendAddress, backendAddress, sizeof(backendAddress));
  }
  if (esp_a2d_source_connect(backendAddress) != ESP_OK)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->outgoingPending = false;
    memset(impl_->backendAddress, 0, sizeof(impl_->backendAddress));
    owner_->setError(EspBleError::BackendFailure,
      "failed to request A2DP Source connection");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicA2dpSource::disconnect()
{
#if !ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE
  return false;
#else
  if (!impl_)
  {
    owner_->setError(EspBleError::InvalidState, "A2DP Source is not initialized");
    return false;
  }
  esp_bd_addr_t address = {};
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->connected) return false;
    memcpy(address, impl_->backendAddress, sizeof(address));
  }
  if (esp_a2d_source_disconnect(address) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to disconnect A2DP Source");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicA2dpSource::start()
{
#if !ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE
  return false;
#else
  if (!impl_)
  {
    owner_->setError(EspBleError::InvalidState, "A2DP Source is not initialized");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->connected || impl_->streaming || impl_->startPending)
    {
      owner_->setError(EspBleError::InvalidState,
        "A2DP Source stream cannot be started in its current state");
      return false;
    }
    impl_->startPending = true;
  }
  if (esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) != ESP_OK)
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->startPending = false;
    owner_->setError(EspBleError::BackendFailure,
      "failed to check A2DP Source readiness");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicA2dpSource::suspend()
{
#if !ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE
  return false;
#else
  if (!streaming())
  {
    owner_->setError(EspBleError::InvalidState, "A2DP Source is not streaming");
    return false;
  }
  if (esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND) != ESP_OK)
  {
    owner_->setError(EspBleError::BackendFailure,
      "failed to suspend A2DP Source");
    return false;
  }
  owner_->clearError();
  return true;
#endif
}

bool EspBleClassicA2dpSource::connected() const
{
  if (!impl_) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->connected;
}

bool EspBleClassicA2dpSource::streaming() const
{
  if (!impl_) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->streaming;
}

EspBleClassicA2dpConnection EspBleClassicA2dpSource::connection() const
{
  if (!impl_) return {};
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->connection;
}

EspBleClassicA2dpCodecConfig EspBleClassicA2dpSource::codecConfig() const
{
  if (!impl_) return {};
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->codec;
}

EspBleClassicAudioSendResult EspBleClassicA2dpSource::send(
  const EspBleClassicEncodedAudioPacket &packet)
{
#if !ESPBLE_CLASSIC_A2DP_SOURCE_BACKEND_AVAILABLE
  (void)packet;
  return EspBleClassicAudioSendResult::InvalidState;
#else
  if (!impl_) return EspBleClassicAudioSendResult::InvalidState;
  EspBleClassicA2dpConnectionId id = 0;
  uint16_t mtu = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->connected || !impl_->streaming)
      return EspBleClassicAudioSendResult::InvalidState;
    id = impl_->connection.id;
    mtu = impl_->connection.mediaMtu;
  }
  if (!packet.data || packet.length == 0 || packet.frameCount == 0)
    return EspBleClassicAudioSendResult::InvalidArgument;
  if (packet.length > UINT16_MAX || (mtu != 0 && packet.length > mtu))
    return EspBleClassicAudioSendResult::TooLarge;
  esp_a2d_audio_buff_t *audio =
    esp_a2d_audio_buff_alloc(static_cast<uint16_t>(packet.length));
  if (!audio) return EspBleClassicAudioSendResult::WouldBlock;
  memcpy(audio->data, packet.data, packet.length);
  audio->data_len = static_cast<uint16_t>(packet.length);
  audio->number_frame = packet.frameCount;
  audio->timestamp = packet.timestamp;
  const esp_err_t result = esp_a2d_source_audio_data_send(id, audio);
  if (result == ESP_OK) return EspBleClassicAudioSendResult::Accepted;
  esp_a2d_audio_buff_free(audio);
  if (result == ESP_FAIL) return EspBleClassicAudioSendResult::WouldBlock;
  if (result == ESP_ERR_INVALID_SIZE) return EspBleClassicAudioSendResult::TooLarge;
  if (result == ESP_ERR_INVALID_ARG) return EspBleClassicAudioSendResult::InvalidArgument;
  if (result == ESP_ERR_INVALID_STATE) return EspBleClassicAudioSendResult::InvalidState;
  return EspBleClassicAudioSendResult::BackendFailure;
#endif
}

void EspBleClassicA2dpSource::onConnected(ConnectionCallback callback)
{ connectedCallback_ = std::move(callback); }
void EspBleClassicA2dpSource::onDisconnected(ConnectionCallback callback)
{ disconnectedCallback_ = std::move(callback); }
void EspBleClassicA2dpSource::onCodecConfigured(CodecConfiguredCallback callback)
{ codecConfiguredCallback_ = std::move(callback); }
void EspBleClassicA2dpSource::onStreamStateChanged(StreamCallback callback)
{ streamCallback_ = std::move(callback); }

size_t EspBleClassicA2dpSource::droppedEventCount() const
{
  if (!impl_) return 0;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->droppedEvents;
}

void EspBleClassicA2dpSource::update()
{
  if (!impl_) return;
  while (true)
  {
    EspBleClassicA2dpSourceImpl::Event event;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->eventCount == 0) break;
      event = std::move(impl_->events[impl_->eventHead]);
      impl_->eventHead = (impl_->eventHead + 1) % A2dpSourceEventQueueCapacity;
      --impl_->eventCount;
    }
    if (event.type == EspBleClassicA2dpSourceImpl::EventType::Connected &&
        connectedCallback_)
      connectedCallback_(event.connection);
    else if (event.type == EspBleClassicA2dpSourceImpl::EventType::Disconnected &&
             disconnectedCallback_)
      disconnectedCallback_(event.connection);
    else if (event.type == EspBleClassicA2dpSourceImpl::EventType::Codec &&
             codecConfiguredCallback_)
      codecConfiguredCallback_(event.codec);
    else if (event.type == EspBleClassicA2dpSourceImpl::EventType::Stream &&
             streamCallback_)
      streamCallback_(event.stream);
    else if (event.type == EspBleClassicA2dpSourceImpl::EventType::SinkDelay &&
             sinkDelayCallback_)
      sinkDelayCallback_(event.sinkDelay);
  }
}

void EspBleClassicA2dpSource::onSinkDelay(SinkDelayCallback callback)
{
  sinkDelayCallback_ = std::move(callback);
}
