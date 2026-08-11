#ifndef ESP_BLE_CLASSIC_H
#define ESP_BLE_CLASSIC_H

#include <Arduino.h>
#include <functional>
#include <memory>
#include <stdint.h>

#include "EspBleTypes.h"

using EspBleClassicSppSessionId = uint32_t;
using EspBleClassicA2dpConnectionId = uint16_t;

struct EspBleClassicConfig
{
  const char *deviceName = "EspBle Classic";
};

struct EspBleClassicSppServerConfig
{
  const char *serviceName = "EspBle SPP";
  uint8_t channel = 0;
};

struct EspBleClassicSppSession
{
  EspBleClassicSppSessionId id = 0;
  String peerAddress;
  bool incoming = false;
};

struct EspBleClassicSppData
{
  EspBleClassicSppSessionId sessionId = 0;
  String value;
};

struct EspBleClassicSppWriteResult
{
  EspBleClassicSppSessionId sessionId = 0;
  size_t length = 0;
  bool success = false;
  EspBleError error = EspBleError::None;
  String detail;
};

struct EspBleClassicSppConnectionFailure
{
  String peerAddress;
  EspBleError error = EspBleError::BackendFailure;
  String detail;
};

enum class EspBleClassicAudioCodec : uint8_t
{
  Unknown = 0,
  Sbc,
  Msbc,
  Cvsd,
};

enum class EspBleClassicA2dpStreamState : uint8_t
{
  Suspended = 0,
  Started,
};

enum class EspBleClassicSbcChannelMode : uint8_t
{
  Unknown = 0,
  Mono,
  DualChannel,
  Stereo,
  JointStereo,
};

enum class EspBleClassicSbcAllocationMethod : uint8_t
{
  Unknown = 0,
  Snr,
  Loudness,
};

struct EspBleClassicA2dpSinkConfig
{
  bool discoverable = true;
};

struct EspBleClassicA2dpConnection
{
  EspBleClassicA2dpConnectionId id = 0;
  String peerAddress;
  uint16_t mediaMtu = 0;
  bool incoming = false;
};

struct EspBleClassicA2dpCodecConfig
{
  EspBleClassicA2dpConnectionId connectionId = 0;
  EspBleClassicAudioCodec codec = EspBleClassicAudioCodec::Unknown;
  uint32_t sampleRate = 0;
  uint8_t channels = 0;
  EspBleClassicSbcChannelMode sbcChannelMode =
    EspBleClassicSbcChannelMode::Unknown;
  uint8_t sbcBlockLength = 0;
  uint8_t sbcSubbands = 0;
  EspBleClassicSbcAllocationMethod sbcAllocationMethod =
    EspBleClassicSbcAllocationMethod::Unknown;
  uint8_t minimumBitpool = 0;
  uint8_t maximumBitpool = 0;
  uint8_t raw[8] = {};
  size_t rawLength = 0;
};

struct EspBleClassicA2dpStreamEvent
{
  EspBleClassicA2dpConnectionId connectionId = 0;
  EspBleClassicA2dpStreamState state =
    EspBleClassicA2dpStreamState::Suspended;
};

// The payload is an encoded codec-frame sequence. data is a read-only view
// valid only until the media callback returns; copy it there if it must be kept.
struct EspBleClassicEncodedAudioView
{
  EspBleClassicA2dpConnectionId connectionId = 0;
  EspBleClassicAudioCodec codec = EspBleClassicAudioCodec::Unknown;
  uint32_t timestamp = 0;
  uint16_t frameCount = 0;
  const uint8_t *data = nullptr;
  size_t length = 0;
};

struct EspBleClassicA2dpSourceConfig
{
  uint32_t sampleRate = 48000;
  EspBleClassicSbcChannelMode channelMode =
    EspBleClassicSbcChannelMode::Stereo;
  uint8_t blockLength = 16;
  uint8_t subbands = 8;
  EspBleClassicSbcAllocationMethod allocationMethod =
    EspBleClassicSbcAllocationMethod::Loudness;
  uint8_t minimumBitpool = 2;
  uint8_t maximumBitpool = 53;
};

struct EspBleClassicEncodedAudioPacket
{
  uint32_t timestamp = 0;
  uint16_t frameCount = 0;
  const uint8_t *data = nullptr;
  size_t length = 0;
};

enum class EspBleClassicAudioSendResult : uint8_t
{
  Accepted = 0,
  WouldBlock,
  InvalidState,
  InvalidArgument,
  TooLarge,
  BackendFailure,
};

enum class EspBleClassicHidReportType : uint8_t
{
  Input = 1,
  Output = 2,
  Feature = 3,
};

struct EspBleClassicHidDeviceConfig
{
  const char *name = "EspBle HID";
  const char *description = "EspBle Classic HID Device";
  const char *provider = "EspBle";
  uint8_t subclass = 0;
  const uint8_t *reportDescriptor = nullptr;
  size_t reportDescriptorLength = 0;
};

struct EspBleClassicHidConnection
{
  String peerAddress;
  bool incoming = false;
};

struct EspBleClassicHidConnectionFailure
{
  String peerAddress;
  EspBleError error = EspBleError::BackendFailure;
  String detail;
};

struct EspBleClassicHidReport
{
  String peerAddress;
  EspBleClassicHidReportType type = EspBleClassicHidReportType::Input;
  uint8_t reportId = 0;
  String value;
};

struct EspBleClassicImpl;
struct EspBleClassicSppImpl;
struct EspBleClassicHidDeviceImpl;
struct EspBleClassicHidHostImpl;
struct EspBleClassicA2dpSinkImpl;
struct EspBleClassicA2dpSourceImpl;
class EspBleClassic;

class EspBleClassicA2dpSink
{
public:
  using ConnectionCallback =
    std::function<void(const EspBleClassicA2dpConnection &)>;
  using CodecConfiguredCallback =
    std::function<void(const EspBleClassicA2dpCodecConfig &)>;
  using StreamCallback =
    std::function<void(const EspBleClassicA2dpStreamEvent &)>;
  // Runs in the Bluetooth host callback context. It must not block, decode,
  // access a device, or retain view.data without copying it.
  using MediaCallback =
    std::function<void(const EspBleClassicEncodedAudioView &)>;

  bool begin(
    const EspBleClassicA2dpSinkConfig &config =
      EspBleClassicA2dpSinkConfig());
  void end();
  bool initialized() const;
  bool connect(const char *address);
  bool disconnect();
  bool connected() const;
  bool streaming() const;
  EspBleClassicA2dpConnection connection() const;
  EspBleClassicA2dpCodecConfig codecConfig() const;

  void onConnected(ConnectionCallback callback);
  void onDisconnected(ConnectionCallback callback);
  void onCodecConfigured(CodecConfiguredCallback callback);
  void onStreamStateChanged(StreamCallback callback);
  void onMedia(MediaCallback callback);
  size_t droppedEventCount() const;

private:
  friend class EspBleClassic;
  friend struct EspBleClassicA2dpSinkImpl;
  explicit EspBleClassicA2dpSink(EspBleClassic *owner);
  ~EspBleClassicA2dpSink();
  void update();

  EspBleClassic *owner_;
  EspBleClassicA2dpSinkImpl *impl_ = nullptr;
  ConnectionCallback connectedCallback_;
  ConnectionCallback disconnectedCallback_;
  CodecConfiguredCallback codecConfiguredCallback_;
  StreamCallback streamCallback_;
  // Atomically replaced by onMedia(). Keeping the callable in an immutable
  // shared object avoids copying std::function in the Bluetooth hot path.
  std::shared_ptr<MediaCallback> mediaCallback_;
};

class EspBleClassicA2dpSource
{
public:
  using ConnectionCallback =
    std::function<void(const EspBleClassicA2dpConnection &)>;
  using CodecConfiguredCallback =
    std::function<void(const EspBleClassicA2dpCodecConfig &)>;
  using StreamCallback =
    std::function<void(const EspBleClassicA2dpStreamEvent &)>;

  bool begin(
    const EspBleClassicA2dpSourceConfig &config =
      EspBleClassicA2dpSourceConfig());
  void end();
  bool initialized() const;
  bool connect(const char *address);
  bool disconnect();
  bool start();
  bool suspend();
  bool connected() const;
  bool streaming() const;
  EspBleClassicA2dpConnection connection() const;
  EspBleClassicA2dpCodecConfig codecConfig() const;
  EspBleClassicAudioSendResult send(
    const EspBleClassicEncodedAudioPacket &packet);

  void onConnected(ConnectionCallback callback);
  void onDisconnected(ConnectionCallback callback);
  void onCodecConfigured(CodecConfiguredCallback callback);
  void onStreamStateChanged(StreamCallback callback);
  size_t droppedEventCount() const;

private:
  friend class EspBleClassic;
  friend struct EspBleClassicA2dpSourceImpl;
  explicit EspBleClassicA2dpSource(EspBleClassic *owner);
  ~EspBleClassicA2dpSource();
  void update();

  EspBleClassic *owner_;
  EspBleClassicA2dpSourceImpl *impl_ = nullptr;
  ConnectionCallback connectedCallback_;
  ConnectionCallback disconnectedCallback_;
  CodecConfiguredCallback codecConfiguredCallback_;
  StreamCallback streamCallback_;
};

class EspBleClassicSpp
{
public:
  static constexpr size_t MaximumWriteSize = 990;
  static constexpr size_t WriteQueueCapacity = 8;
  static constexpr size_t ReceiveBufferCapacity = 2048;

  using ServerStartedCallback = std::function<void()>;
  using SessionCallback = std::function<void(const EspBleClassicSppSession &)>;
  using DataCallback = std::function<void(const EspBleClassicSppData &)>;
  using WriteCompletedCallback =
    std::function<void(const EspBleClassicSppWriteResult &)>;
  using ConnectionFailureCallback =
    std::function<void(const EspBleClassicSppConnectionFailure &)>;

  void onServerStarted(ServerStartedCallback callback);
  void onConnected(SessionCallback callback);
  void onDisconnected(SessionCallback callback);
  void onData(DataCallback callback);
  void onWriteCompleted(WriteCompletedCallback callback);
  void onConnectionFailed(ConnectionFailureCallback callback);

  bool startServer(
    const EspBleClassicSppServerConfig &config =
      EspBleClassicSppServerConfig());
  bool stopServer();
  bool serverRunning() const;

  bool connect(const char *address, uint32_t timeoutMilliseconds = 10000);
  bool disconnect(EspBleClassicSppSessionId sessionId);
  size_t sessionCount() const;
  bool session(
    EspBleClassicSppSessionId sessionId,
    EspBleClassicSppSession &session) const;

  bool write(
    EspBleClassicSppSessionId sessionId,
    const uint8_t *data, size_t length);
  bool write(EspBleClassicSppSessionId sessionId, const String &value);
  size_t pendingWriteCount(EspBleClassicSppSessionId sessionId) const;
  size_t droppedWriteCount() const;

  size_t available(EspBleClassicSppSessionId sessionId) const;
  int peek(EspBleClassicSppSessionId sessionId) const;
  int read(EspBleClassicSppSessionId sessionId);
  size_t read(
    EspBleClassicSppSessionId sessionId, uint8_t *data, size_t length);
  size_t droppedReceiveByteCount() const;
  size_t droppedEventCount() const;

private:
  friend class EspBleClassic;
  friend struct EspBleClassicSppImpl;

  explicit EspBleClassicSpp(EspBleClassic *owner);
  ~EspBleClassicSpp();
  bool begin();
  void end();
  void update();

  EspBleClassic *owner_;
  EspBleClassicSppImpl *impl_ = nullptr;
  ServerStartedCallback serverStartedCallback_;
  SessionCallback connectedCallback_;
  SessionCallback disconnectedCallback_;
  DataCallback dataCallback_;
  WriteCompletedCallback writeCompletedCallback_;
  ConnectionFailureCallback connectionFailureCallback_;
};

class EspBleClassicHidDevice
{
public:
  static constexpr size_t MaximumReportLength = 1024;
  using ConnectionCallback =
    std::function<void(const EspBleClassicHidConnection &)>;
  using ReportCallback = std::function<void(const EspBleClassicHidReport &)>;

  bool begin(const EspBleClassicHidDeviceConfig &config);
  void end();
  bool initialized() const;
  bool registered() const;
  bool connected() const;
  String peerAddress() const;

  bool sendReport(
    EspBleClassicHidReportType type, uint8_t reportId,
    const uint8_t *data, size_t length);
  bool sendInputReport(
    uint8_t reportId, const uint8_t *data, size_t length);
  bool disconnect();

  void onConnected(ConnectionCallback callback);
  void onDisconnected(ConnectionCallback callback);
  void onOutputReport(ReportCallback callback);
  size_t droppedEventCount() const;

private:
  friend class EspBleClassic;
  explicit EspBleClassicHidDevice(EspBleClassic *owner);
  ~EspBleClassicHidDevice();
  void update();

  EspBleClassic *owner_;
  EspBleClassicHidDeviceImpl *impl_ = nullptr;
  ConnectionCallback connectedCallback_;
  ConnectionCallback disconnectedCallback_;
  ReportCallback outputReportCallback_;
};

class EspBleClassicHidHost
{
public:
  static constexpr size_t MaximumReportLength = 1024;
  using ConnectionCallback =
    std::function<void(const EspBleClassicHidConnection &)>;
  using ConnectionFailureCallback =
    std::function<void(const EspBleClassicHidConnectionFailure &)>;
  using ReportCallback = std::function<void(const EspBleClassicHidReport &)>;

  bool begin();
  void end();
  bool initialized() const;
  bool connect(const char *address);
  bool disconnect();
  bool connected() const;
  String peerAddress() const;

  bool sendOutputReport(const uint8_t *data, size_t length);

  void onConnected(ConnectionCallback callback);
  void onDisconnected(ConnectionCallback callback);
  void onConnectionFailed(ConnectionFailureCallback callback);
  void onInputReport(ReportCallback callback);
  size_t droppedEventCount() const;

private:
  friend class EspBleClassic;
  explicit EspBleClassicHidHost(EspBleClassic *owner);
  ~EspBleClassicHidHost();
  void update();

  EspBleClassic *owner_;
  EspBleClassicHidHostImpl *impl_ = nullptr;
  ConnectionCallback connectedCallback_;
  ConnectionCallback disconnectedCallback_;
  ConnectionFailureCallback connectionFailureCallback_;
  ReportCallback inputReportCallback_;
};

class EspBleClassic
{
public:
  EspBleClassic();
  ~EspBleClassic();
  EspBleClassic(const EspBleClassic &) = delete;
  EspBleClassic &operator=(const EspBleClassic &) = delete;

  bool begin(const EspBleClassicConfig &config = EspBleClassicConfig());
  void end();
  void update();
  bool initialized() const;

  EspBleClassicSpp &spp();
  EspBleClassicHidDevice &hidDevice();
  EspBleClassicHidHost &hidHost();
  EspBleClassicA2dpSink &a2dpSink();
  EspBleClassicA2dpSource &a2dpSource();

  EspBleError lastError() const;
  const char *lastErrorName() const;
  const String &lastErrorDetail() const;

private:
  friend class EspBleClassicSpp;
  friend class EspBleClassicHidDevice;
  friend class EspBleClassicHidHost;
  friend class EspBleClassicA2dpSink;
  friend class EspBleClassicA2dpSource;

  void clearError();
  void setError(EspBleError error, const char *detail);

  EspBleClassicImpl *impl_ = nullptr;
  EspBleClassicSpp spp_;
  EspBleClassicHidDevice hidDevice_;
  EspBleClassicHidHost hidHost_;
  EspBleClassicA2dpSink a2dpSink_;
  EspBleClassicA2dpSource a2dpSource_;
  EspBleError lastError_ = EspBleError::None;
  String lastErrorDetail_;
};

#endif
