#ifndef ESP_BLE_CLASSIC_H
#define ESP_BLE_CLASSIC_H

#include <Arduino.h>
#include <functional>
#include <memory>
#include <stdint.h>

#include "EspBleTypes.h"

using EspBleClassicSppSessionId = uint32_t;
using EspBleClassicA2dpConnectionId = uint16_t;
using EspBleClassicHfpSyncConnectionId = uint16_t;

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

enum class EspBleClassicAvrcpCommand : uint8_t
{
  Select = 0x00,
  Up = 0x01,
  Down = 0x02,
  Left = 0x03,
  Right = 0x04,
  VolumeUp = 0x41,
  VolumeDown = 0x42,
  Mute = 0x43,
  Play = 0x44,
  Stop = 0x45,
  Pause = 0x46,
  Rewind = 0x48,
  FastForward = 0x49,
  Next = 0x4b,
  Previous = 0x4c,
};

enum class EspBleClassicAvrcpKeyState : uint8_t
{
  Pressed = 0,
  Released = 1,
};

enum EspBleClassicAvrcpMetadataAttribute : uint8_t
{
  EspBleClassicAvrcpMetadataTitle = 0x01,
  EspBleClassicAvrcpMetadataArtist = 0x02,
  EspBleClassicAvrcpMetadataAlbum = 0x04,
  EspBleClassicAvrcpMetadataTrackNumber = 0x08,
  EspBleClassicAvrcpMetadataTrackCount = 0x10,
  EspBleClassicAvrcpMetadataGenre = 0x20,
  EspBleClassicAvrcpMetadataPlayingTime = 0x40,
};

enum class EspBleClassicAvrcpPlaybackState : uint8_t
{
  Stopped = 0,
  Playing,
  Paused,
  ForwardSeek,
  ReverseSeek,
  Error = 0xff,
};

struct EspBleClassicAvrcpConfig
{
  bool controller = true;
  bool target = true;
  uint8_t initialVolume = 64;
};

struct EspBleClassicAvrcpConnection
{
  String peerAddress;
  bool controller = false;
  bool connected = false;
};

struct EspBleClassicAvrcpRemoteFeatures
{
  String peerAddress;
  bool controller = false;
  uint32_t featureMask = 0;
  uint16_t featureFlags = 0;
};

struct EspBleClassicAvrcpPassthrough
{
  EspBleClassicAvrcpCommand command = EspBleClassicAvrcpCommand::Play;
  EspBleClassicAvrcpKeyState state = EspBleClassicAvrcpKeyState::Pressed;
};

struct EspBleClassicAvrcpPassthroughResponse
{
  uint8_t transactionLabel = 0;
  EspBleClassicAvrcpCommand command = EspBleClassicAvrcpCommand::Play;
  EspBleClassicAvrcpKeyState state = EspBleClassicAvrcpKeyState::Pressed;
  uint8_t responseCode = 0;
  bool accepted = false;
};

struct EspBleClassicAvrcpMetadata
{
  uint8_t attribute = 0;
  String value;
};

struct EspBleClassicAvrcpPlayStatus
{
  uint32_t trackLengthMilliseconds = 0;
  uint32_t positionMilliseconds = 0;
  EspBleClassicAvrcpPlaybackState state =
    EspBleClassicAvrcpPlaybackState::Error;
};

struct EspBleClassicAvrcpVolume
{
  uint8_t value = 0;
  bool remoteCommand = false;
};

enum class EspBleClassicHfpConnectionState : uint8_t
{
  Disconnected = 0,
  Connecting,
  Connected,
  ServiceLevelConnected,
  Disconnecting,
};

enum class EspBleClassicHfpAudioState : uint8_t
{
  Disconnected = 0,
  Connecting,
  Connected,
};

enum class EspBleClassicHfpCallSetupState : uint8_t
{
  Idle = 0,
  Incoming,
  OutgoingDialing,
  OutgoingAlerting,
};

enum class EspBleClassicHfpCallHeldState : uint8_t
{
  None = 0,
  HeldAndActive,
  Held,
};

enum class EspBleClassicHfpCurrentCallState : uint8_t
{
  Active = 0,
  Held,
  Dialing,
  Alerting,
  Incoming,
  Waiting,
  HeldByResponseAndHold,
};

enum class EspBleClassicHfpVolumeTarget : uint8_t
{
  Speaker = 0,
  Microphone,
};

struct EspBleClassicHfpConnection
{
  String peerAddress;
  EspBleClassicHfpConnectionState state =
    EspBleClassicHfpConnectionState::Disconnected;
  uint32_t peerFeatures = 0;
  uint32_t callHoldFeatures = 0;
};

struct EspBleClassicHfpAudioConnection
{
  String peerAddress;
  EspBleClassicHfpAudioState state =
    EspBleClassicHfpAudioState::Disconnected;
  EspBleClassicHfpSyncConnectionId id = 0;
  EspBleClassicAudioCodec codec = EspBleClassicAudioCodec::Unknown;
  uint16_t preferredFrameSize = 0;
};

struct EspBleClassicHfpCallState
{
  bool active = false;
  EspBleClassicHfpCallSetupState setup =
    EspBleClassicHfpCallSetupState::Idle;
  EspBleClassicHfpCallHeldState held =
    EspBleClassicHfpCallHeldState::None;
};

struct EspBleClassicHfpCaller
{
  String number;
  bool waiting = false;
};

struct EspBleClassicHfpCurrentCall
{
  int index = 0;
  bool incoming = false;
  EspBleClassicHfpCurrentCallState state =
    EspBleClassicHfpCurrentCallState::Active;
  bool multiparty = false;
  String number;
};

struct EspBleClassicHfpVolume
{
  EspBleClassicHfpVolumeTarget target =
    EspBleClassicHfpVolumeTarget::Speaker;
  uint8_t value = 0;
};

struct EspBleClassicHfpAtResponse
{
  uint8_t code = 0;
  uint16_t extendedError = 0;
};

struct EspBleClassicHfpPacketStatistics
{
  uint32_t received = 0;
  uint32_t receivedCorrect = 0;
  uint32_t receivedError = 0;
  uint32_t receivedMissing = 0;
  uint32_t receivedLost = 0;
  uint32_t sent = 0;
  uint32_t sentDiscarded = 0;
};

// data is a callback-lifetime view of an encoded CVSD/mSBC HCI synchronous
// payload. mSBC input may contain controller padding after its 57-byte frame.
struct EspBleClassicHfpEncodedAudioView
{
  EspBleClassicHfpSyncConnectionId connectionId = 0;
  EspBleClassicAudioCodec codec = EspBleClassicAudioCodec::Unknown;
  bool badFrame = false;
  const uint8_t *data = nullptr;
  size_t length = 0;
};

struct EspBleClassicHfpEncodedAudioPacket
{
  const uint8_t *data = nullptr;
  size_t length = 0;
};

struct EspBleClassicHfpAudioGatewayConfig
{
  bool discoverable = true;
  // mSBC is the normal HFP 1.6+ choice. Select Cvsd to require narrowband
  // codec negotiation for peers and applications that need it.
  EspBleClassicAudioCodec preferredAudioCodec =
    EspBleClassicAudioCodec::Msbc;
  const char *operatorName = "EspBle";
  const char *subscriberNumber = "";
  bool networkAvailable = true;
  uint8_t signalStrength = 5;
  bool roaming = false;
  uint8_t batteryLevel = 5;
};

enum class EspBleClassicHfpAudioGatewayCommandType : uint8_t
{
  Dial,
  Answer,
  Hangup,
  Dtmf,
  VoiceRecognition,
  NoiseReduction,
  UnknownAt,
};

struct EspBleClassicHfpAudioGatewayCommand
{
  EspBleClassicHfpAudioGatewayCommandType type =
    EspBleClassicHfpAudioGatewayCommandType::Dial;
  String value;
  bool enabled = false;
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
struct EspBleClassicAvrcpImpl;
struct EspBleClassicHfpClientImpl;
struct EspBleClassicHfpAudioGatewayImpl;
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

class EspBleClassicAvrcp
{
public:
  using ConnectionCallback =
    std::function<void(const EspBleClassicAvrcpConnection &)>;
  using PassthroughCallback =
    std::function<void(const EspBleClassicAvrcpPassthrough &)>;
  using RemoteFeaturesCallback =
    std::function<void(const EspBleClassicAvrcpRemoteFeatures &)>;
  using PassthroughResponseCallback =
    std::function<void(const EspBleClassicAvrcpPassthroughResponse &)>;
  using MetadataCallback =
    std::function<void(const EspBleClassicAvrcpMetadata &)>;
  using PlayStatusCallback =
    std::function<void(const EspBleClassicAvrcpPlayStatus &)>;
  using VolumeCallback =
    std::function<void(const EspBleClassicAvrcpVolume &)>;

  // Start AVRCP before starting A2DP. The selected roles remain fixed until
  // end(); callbacks below are dispatched from EspBleClassic::update().
  bool begin(
    const EspBleClassicAvrcpConfig &config = EspBleClassicAvrcpConfig());
  void end();
  bool initialized() const;
  bool controllerInitialized() const;
  bool targetInitialized() const;
  bool controllerConnected() const;
  bool targetConnected() const;
  uint8_t volume() const;

  bool sendPassthrough(
    EspBleClassicAvrcpCommand command,
    EspBleClassicAvrcpKeyState state);
  bool sendKey(EspBleClassicAvrcpCommand command);
  bool requestMetadata(uint8_t attributeMask);
  bool requestPlayStatus();
  bool setAbsoluteVolume(uint8_t volume);
  // AVRCP notifications are one-shot. Register again after each Changed event.
  bool registerVolumeNotifications();
  bool setLocalVolume(uint8_t volume);

  void onConnectionChanged(ConnectionCallback callback);
  void onRemoteFeatures(RemoteFeaturesCallback callback);
  void onPassthrough(PassthroughCallback callback);
  void onPassthroughResponse(PassthroughResponseCallback callback);
  void onMetadata(MetadataCallback callback);
  void onPlayStatus(PlayStatusCallback callback);
  void onVolumeChanged(VolumeCallback callback);
  size_t droppedEventCount() const;

private:
  friend class EspBleClassic;
  friend struct EspBleClassicAvrcpImpl;
  explicit EspBleClassicAvrcp(EspBleClassic *owner);
  ~EspBleClassicAvrcp();
  void update();

  EspBleClassic *owner_;
  EspBleClassicAvrcpImpl *impl_ = nullptr;
  ConnectionCallback connectionCallback_;
  RemoteFeaturesCallback remoteFeaturesCallback_;
  PassthroughCallback passthroughCallback_;
  PassthroughResponseCallback passthroughResponseCallback_;
  MetadataCallback metadataCallback_;
  PlayStatusCallback playStatusCallback_;
  VolumeCallback volumeCallback_;
};

class EspBleClassicHfpClient
{
public:
  using ConnectionCallback =
    std::function<void(const EspBleClassicHfpConnection &)>;
  using AudioConnectionCallback =
    std::function<void(const EspBleClassicHfpAudioConnection &)>;
  using CallStateCallback =
    std::function<void(const EspBleClassicHfpCallState &)>;
  using CallerCallback = std::function<void(const EspBleClassicHfpCaller &)>;
  using RingCallback = std::function<void()>;
  using CurrentCallCallback =
    std::function<void(const EspBleClassicHfpCurrentCall &)>;
  using VolumeCallback = std::function<void(const EspBleClassicHfpVolume &)>;
  using AtResponseCallback =
    std::function<void(const EspBleClassicHfpAtResponse &)>;
  using PacketStatisticsCallback =
    std::function<void(const EspBleClassicHfpPacketStatistics &)>;
  // Runs in the Bluetooth host callback context. Copy view.data before return.
  using AudioCallback =
    std::function<void(const EspBleClassicHfpEncodedAudioView &)>;

  bool begin();
  void end();
  bool initialized() const;
  bool connect(const char *address);
  bool disconnect();
  bool connected() const;
  bool serviceLevelConnected() const;
  EspBleClassicHfpConnection connection() const;

  bool connectAudio();
  bool disconnectAudio();
  bool audioConnected() const;
  EspBleClassicHfpAudioConnection audioConnection() const;
  // Copies packet.data. Accepted transfers the copy to the host, but does not
  // guarantee controller delivery; inspect packet statistics for discards.
  // WouldBlock currently means that the local copy could not be allocated.
  EspBleClassicAudioSendResult send(
    const EspBleClassicHfpEncodedAudioPacket &packet);
  bool requestPacketStatistics();

  bool dial(const char *number);
  bool redial();
  bool answerCall();
  bool rejectOrEndCall();
  bool queryCurrentCalls();
  bool sendDtmf(char code);
  bool setVolume(EspBleClassicHfpVolumeTarget target, uint8_t value);
  bool startVoiceRecognition();
  bool stopVoiceRecognition();
  EspBleClassicHfpCallState callState() const;

  void onConnectionChanged(ConnectionCallback callback);
  void onAudioConnectionChanged(AudioConnectionCallback callback);
  void onCallStateChanged(CallStateCallback callback);
  void onCaller(CallerCallback callback);
  void onRing(RingCallback callback);
  void onCurrentCall(CurrentCallCallback callback);
  void onVolumeChanged(VolumeCallback callback);
  void onAtResponse(AtResponseCallback callback);
  void onPacketStatistics(PacketStatisticsCallback callback);
  // Replacement and removal wait for callbacks already in progress. Do not
  // call onAudio() or end() from inside the audio callback.
  void onAudio(AudioCallback callback);
  size_t droppedEventCount() const;

private:
  friend class EspBleClassic;
  friend struct EspBleClassicHfpClientImpl;
  explicit EspBleClassicHfpClient(EspBleClassic *owner);
  ~EspBleClassicHfpClient();
  void update();
  bool finishCommand(bool connected, bool success, const char *failure);

  EspBleClassic *owner_;
  EspBleClassicHfpClientImpl *impl_ = nullptr;
  ConnectionCallback connectionCallback_;
  AudioConnectionCallback audioConnectionCallback_;
  CallStateCallback callStateCallback_;
  CallerCallback callerCallback_;
  RingCallback ringCallback_;
  CurrentCallCallback currentCallCallback_;
  VolumeCallback volumeCallback_;
  AtResponseCallback atResponseCallback_;
  PacketStatisticsCallback packetStatisticsCallback_;
};

class EspBleClassicHfpAudioGateway
{
public:
  using ConnectionCallback =
    std::function<void(const EspBleClassicHfpConnection &)>;
  using AudioConnectionCallback =
    std::function<void(const EspBleClassicHfpAudioConnection &)>;
  using CallStateCallback =
    std::function<void(const EspBleClassicHfpCallState &)>;
  using CommandCallback =
    std::function<void(const EspBleClassicHfpAudioGatewayCommand &)>;
  using VolumeCallback = std::function<void(const EspBleClassicHfpVolume &)>;
  using PacketStatisticsCallback =
    std::function<void(const EspBleClassicHfpPacketStatistics &)>;
  // Runs in the Bluetooth host callback context. Copy view.data before return.
  using AudioCallback =
    std::function<void(const EspBleClassicHfpEncodedAudioView &)>;

  bool begin(
    const EspBleClassicHfpAudioGatewayConfig &config =
      EspBleClassicHfpAudioGatewayConfig());
  void end();
  bool initialized() const;
  bool connect(const char *address);
  bool disconnect();
  bool connected() const;
  bool serviceLevelConnected() const;
  EspBleClassicHfpConnection connection() const;

  bool connectAudio();
  bool disconnectAudio();
  bool audioConnected() const;
  EspBleClassicHfpAudioConnection audioConnection() const;
  // The ownership and delivery semantics match EspBleClassicHfpClient::send().
  EspBleClassicAudioSendResult send(
    const EspBleClassicHfpEncodedAudioPacket &packet);
  bool requestPacketStatistics();

  bool setNetworkStatus(
    bool available, uint8_t signalStrength,
    bool roaming, uint8_t batteryLevel);
  bool respondToCommand(bool accepted, uint16_t extendedError = 0);
  bool respondToUnknownAt(const char *response);
  bool reportIncomingCall(const char *number);
  bool reportOutgoingCall(const char *number);
  bool reportCallActive();
  bool reportCallEnded();
  bool setVolume(EspBleClassicHfpVolumeTarget target, uint8_t value);
  bool setVoiceRecognition(bool enabled);
  EspBleClassicHfpCallState callState() const;
  EspBleClassicHfpCurrentCall currentCall() const;

  void onConnectionChanged(ConnectionCallback callback);
  void onAudioConnectionChanged(AudioConnectionCallback callback);
  void onCallStateChanged(CallStateCallback callback);
  void onCommand(CommandCallback callback);
  void onVolumeChanged(VolumeCallback callback);
  void onPacketStatistics(PacketStatisticsCallback callback);
  // Replacement and removal wait for callbacks already in progress. Do not
  // call onAudio() or end() from inside the audio callback.
  void onAudio(AudioCallback callback);
  size_t droppedEventCount() const;

private:
  friend class EspBleClassic;
  friend struct EspBleClassicHfpAudioGatewayImpl;
  explicit EspBleClassicHfpAudioGateway(EspBleClassic *owner);
  ~EspBleClassicHfpAudioGateway();
  void update();

  EspBleClassic *owner_;
  EspBleClassicHfpAudioGatewayImpl *impl_ = nullptr;
  ConnectionCallback connectionCallback_;
  AudioConnectionCallback audioConnectionCallback_;
  CallStateCallback callStateCallback_;
  CommandCallback commandCallback_;
  VolumeCallback volumeCallback_;
  PacketStatisticsCallback packetStatisticsCallback_;
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
  EspBleClassicAvrcp &avrcp();
  EspBleClassicHfpClient &hfpClient();
  EspBleClassicHfpAudioGateway &hfpAudioGateway();

  EspBleError lastError() const;
  const char *lastErrorName() const;
  const String &lastErrorDetail() const;

private:
  friend class EspBleClassicSpp;
  friend class EspBleClassicHidDevice;
  friend class EspBleClassicHidHost;
  friend class EspBleClassicA2dpSink;
  friend class EspBleClassicA2dpSource;
  friend class EspBleClassicAvrcp;
  friend class EspBleClassicHfpClient;
  friend class EspBleClassicHfpAudioGateway;

  void clearError();
  void setError(EspBleError error, const char *detail);

  EspBleClassicImpl *impl_ = nullptr;
  EspBleClassicSpp spp_;
  EspBleClassicHidDevice hidDevice_;
  EspBleClassicHidHost hidHost_;
  EspBleClassicA2dpSink a2dpSink_;
  EspBleClassicA2dpSource a2dpSource_;
  EspBleClassicAvrcp avrcp_;
  EspBleClassicHfpClient hfpClient_;
  EspBleClassicHfpAudioGateway hfpAudioGateway_;
  EspBleError lastError_ = EspBleError::None;
  String lastErrorDetail_;
};

#endif
