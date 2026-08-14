#ifndef ESP_BLE_CLASSIC_H
#define ESP_BLE_CLASSIC_H

#include <Arduino.h>
#include <functional>
#include <memory>
#include <stdint.h>

#include "EspBleTypes.h"
#include "EspBleHidProfile.h"
#include "EspBleKeymap.h"
#include "EspBleHidReportMap.h"

using EspBleClassicSppSessionId = uint32_t;
using EspBleClassicA2dpConnectionId = uint16_t;
using EspBleClassicHfpSyncConnectionId = uint16_t;

// Classic pairing. Without this the stack can only accept every pairing
// request with no user check, which is what a hard-coded "always yes" does.
enum class EspBleClassicSecurityIoCapability : uint8_t
{
  // No input and no output: Just Works, accepted without asking anyone.
  None = 0,
  // Shows a passkey the peer types in.
  DisplayOnly,
  // Types in the passkey the peer shows.
  KeyboardOnly,
  // Shows a number both sides compare, and can answer yes or no.
  DisplayYesNo,
};

struct EspBleClassicSecurityConfig
{
  // Off keeps the historical behaviour: Just Works, accepted automatically.
  bool enabled = false;
  EspBleClassicSecurityIoCapability ioCapability =
    EspBleClassicSecurityIoCapability::None;
  // A pairing the application never answers must not hold the peer forever.
  uint32_t responseTimeoutMilliseconds = 30000;
};

struct EspBleClassicSecurityChanged
{
  String peerAddress;
  bool success = false;
  // The backend status, kept so a failure can be told apart from a rejection.
  int status = 0;
};

struct EspBleClassicNumericComparison
{
  String peerAddress;
  uint32_t value = 0;
};

struct EspBleClassicPasskeyDisplayed
{
  String peerAddress;
  uint32_t passkey = 0;
};

struct EspBleClassicPasskeyRequested
{
  String peerAddress;
};

struct EspBleClassicBond
{
  String peerAddress;
};

// What a peer that is looking around can do with this device. A profile makes
// itself reachable when it starts, which is what a sketch usually wants; this
// is for the cases where it does not — a device that should only accept the
// peer it already paired with, or one that must not answer inquiry at all.
enum class EspBleClassicVisibility : uint8_t
{
  // Neither connectable nor discoverable. Existing connections stay.
  Hidden,
  // Reachable by a peer that already knows the address, invisible to inquiry.
  ConnectableOnly,
  // Reachable and visible to inquiry.
  ConnectableDiscoverable,
};

// The Class of Device tells a Host what kind of device this is before any
// profile is queried, which is how a phone or a console decides the icon it
// shows and, on some Hosts, whether it offers to connect at all. A HID device
// that reports the default class can be listed as "uncategorised" and ignored.
struct EspBleClassicClassOfDevice
{
  // Bluetooth Assigned Numbers, Baseband. The common major classes are
  // 0x01 Computer, 0x02 Phone, 0x04 Audio/Video, 0x05 Peripheral.
  uint8_t majorDeviceClass = 0;
  // The 6-bit minor field, not the byte it sits in — the over-the-air value is
  // this shifted left by two. Meaning depends on the major class. For
  // Peripheral: 0x10 keyboard, 0x20 pointing device, 0x01 joystick,
  // 0x02 gamepad. For Audio/Video: 0x01 headset, 0x02 hands-free,
  // 0x05 loudspeaker, 0x06 headphones, 0x08 car audio.
  uint8_t minorDeviceClass = 0;
  // The 11-bit service field: 0x020 Rendering, 0x040 Capturing,
  // 0x080 Object Transfer, 0x100 Audio, 0x200 Telephony. A headset reports
  // Audio | Rendering, which is why a Host groups it with speakers.
  uint16_t serviceClass = 0;
};

struct EspBleClassicConfig
{
  const char *deviceName = "EspBle Classic";
  EspBleClassicSecurityConfig security;
  // Applied at begin(). A profile that starts afterwards keeps whatever the
  // sketch asked for rather than making the device reachable behind its back.
  EspBleClassicVisibility visibility =
    EspBleClassicVisibility::ConnectableDiscoverable;
  // Left at zero the backend's default class is used, which identifies the
  // device as uncategorised.
  EspBleClassicClassOfDevice classOfDevice;
};

struct EspBleClassicSppServerConfig
{
  const char *serviceName = "EspBle SPP";
  // Zero lets the backend pick a free RFCOMM channel. A device that publishes
  // more than one service needs a distinct channel per service, so a fixed
  // value is only useful when a peer expects that exact channel.
  uint8_t channel = 0;
};

// A running SPP server. A device can publish several, each with its own service
// record, which is how one device offers more than one serial service.
struct EspBleClassicSppServer
{
  String serviceName;
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

// Events a Controller can subscribe to on a Target. A Target that declares a
// capability has to answer when a Controller registers for it, so a sketch
// declares only what it can actually report.
enum class EspBleClassicAvrcpNotification : uint8_t
{
  PlayStatus = 0x01,
  TrackChange = 0x02,
  TrackReachedEnd = 0x03,
  TrackReachedStart = 0x04,
  PlaybackPosition = 0x05,
  BatteryStatus = 0x06,
  VolumeChange = 0x0d,
};

enum class EspBleClassicAvrcpPlaybackStatus : uint8_t
{
  Stopped = 0,
  Playing = 1,
  Paused = 2,
  ForwardSeek = 3,
  ReverseSeek = 4,
  Error = 0xff,
};

// The value that goes with a notification. Which member matters follows from
// the event, as the profile defines it; the rest are ignored.
struct EspBleClassicAvrcpNotificationValue
{
  EspBleClassicAvrcpNotification event =
    EspBleClassicAvrcpNotification::PlayStatus;
  EspBleClassicAvrcpPlaybackStatus playbackStatus =
    EspBleClassicAvrcpPlaybackStatus::Stopped;
  // Milliseconds, for PlaybackPosition.
  uint32_t playbackPosition = 0;
  // For TrackChange. All ones means "no track selected", which is what the
  // profile uses when nothing is loaded.
  uint8_t trackId[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  // 0..127, for VolumeChange.
  uint8_t volume = 0;
  // For BatteryStatus, as the profile numbers it.
  uint8_t batteryStatus = 0;
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

// Device identity for the composed HID Device. The Classic profile registers
// one device record, so the first configured profile decides these.
struct EspBleClassicHidProfileConfig
{
  const char *name = "EspBle HID";
  const char *description = "EspBle Classic HID Device";
  const char *provider = "EspBle";
  // Mouse profiles use this to size the button field; ignored elsewhere.
  uint8_t mouseButtonCount = 5;
};

// The LED state a Host writes to a keyboard. Classic identifies the peer by
// address where the BLE side uses a connection id.
struct EspBleClassicHidKeyboardLeds
{
  String peerAddress;
  uint8_t leds = 0;
  bool numLock = false;
  bool capsLock = false;
  bool scrollLock = false;
  bool compose = false;
  bool kana = false;

  void setLeds(uint8_t value)
  {
    leds = value;
    numLock = (value & 0x01) != 0;
    capsLock = (value & 0x02) != 0;
    scrollLock = (value & 0x04) != 0;
    compose = (value & 0x08) != 0;
    kana = (value & 0x10) != 0;
  }
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
  // On the HID Device side this is the payload alone, with reportId carrying
  // the identifier whichever channel delivered the report. On the HID Host side
  // an incoming report arrives exactly as the device sent it, so a device that
  // declares report IDs puts one in front of the payload there.
  String value;
};

// A Host asking the device for the current contents of a report rather than
// waiting for the device to send one. Real Hosts do this after connecting, and a
// device that never answers looks broken to them.
struct EspBleClassicHidReportRequest
{
  String peerAddress;
  EspBleClassicHidReportType type = EspBleClassicHidReportType::Input;
  uint8_t reportId = 0;
  // The largest reply the Host will accept.
  uint16_t maximumLength = 0;
};

// Why a request could not be answered. Sent instead of a report so the Host
// stops waiting; the names follow the HID handshake codes.
enum class EspBleClassicHidRequestError : uint8_t
{
  NotReady = 1,
  InvalidReportId = 2,
  UnsupportedRequest = 3,
  InvalidParameter = 4,
  Unknown = 14,
  Fatal = 15,
};

// Report Protocol is the normal mode. Boot Protocol is a fixed keyboard or
// mouse layout that restricted Hosts such as a BIOS use.
enum class EspBleClassicHidProtocolMode : uint8_t
{
  Report = 0,
  Boot = 1,
};

// Inquiry is how a sketch finds a peer it has no address for. Every profile
// here takes a Bluetooth address to connect to, so without this the address has
// to come from outside the device.
struct EspBleClassicInquiryConfig
{
  // The controller counts inquiry time in 1.28 s units, so the requested
  // seconds are rounded up to the next unit. 0 responses means no limit.
  uint32_t durationSeconds = 10;
  uint8_t maxResponses = 0;
};

struct EspBleClassicInquiryResult
{
  String address;
  // Empty when the peer answered without a name and published none in its EIR.
  String name;
  uint32_t classOfDevice = 0;
  int rssi = 0;
  bool hasClassOfDevice = false;
  bool hasRssi = false;
};

struct EspBleClassicInquiryComplete
{
  // True when the scan ended because stop() was called rather than by the
  // duration running out.
  bool cancelled = false;
};

struct EspBleClassicImpl;
struct EspBleClassicInquiryImpl;
struct EspBleClassicSppImpl;
struct EspBleClassicHidDeviceImpl;
struct EspBleClassicHidHostImpl;
struct EspBleClassicA2dpSinkImpl;
struct EspBleClassicA2dpSourceImpl;
struct EspBleClassicAvrcpImpl;
struct EspBleClassicHfpClientImpl;
struct EspBleClassicHfpAudioGatewayImpl;
class EspBleClassic;

// How long after receiving audio the Sink actually plays it. A video player on
// the Source side uses this to keep pictures in step with sound; without it the
// Source has no idea how far behind the Sink is. The unit is tenths of a
// millisecond, as the profile defines it.
struct EspBleClassicA2dpDelay
{
  bool success = false;
  uint16_t tenthsOfMilliseconds = 0;
};

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

  // Tells the Source how long this Sink takes to play what it receives, and
  // reads back what the backend has stored. Both are round trips, so the answer
  // arrives at onDelay(). A Sink that decodes in another library knows its own
  // latency; this library cannot measure it.
  using DelayCallback = std::function<void(const EspBleClassicA2dpDelay &)>;
  bool setDelay(uint16_t tenthsOfMilliseconds);
  bool requestDelay();
  void onDelay(DelayCallback callback);

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
  DelayCallback delayCallback_;
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

  // The delay the Sink reported for itself. Arrives when the Sink sends it,
  // which it may do at any time while connected — nothing here asks for it.
  using SinkDelayCallback =
    std::function<void(const EspBleClassicA2dpDelay &)>;
  void onSinkDelay(SinkDelayCallback callback);

  size_t droppedEventCount() const;

private:
  friend class EspBleClassic;
  friend struct EspBleClassicA2dpSourceImpl;
  explicit EspBleClassicA2dpSource(EspBleClassic *owner);
  ~EspBleClassicA2dpSource();
  void update();

  EspBleClassic *owner_;
  EspBleClassicA2dpSourceImpl *impl_ = nullptr;
  SinkDelayCallback sinkDelayCallback_;
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
  // Controller side: subscribe to any of the events above, not just volume.
  bool registerNotifications(EspBleClassicAvrcpNotification event);
  // Controller side: change a player setting such as repeat or shuffle. The
  // attribute and value numbers are the profile's; a Target may refuse any of
  // them, which arrives as the failure of this command rather than here.
  bool setPlayerSetting(uint8_t attributeId, uint8_t value);
  bool setLocalVolume(uint8_t volume);

  // Target side. A Target must answer a registration it advertised support for,
  // and must send a Changed response when the value moves — a Controller that
  // gets neither waits, and some Controllers stop asking afterwards.
  //
  // Volume is handled by the library already: it answers the registration and
  // sends the Changed response from setLocalVolume(). Everything else is the
  // sketch's, because only the sketch knows the values.
  //
  // The bundled Classic host allows a Target to declare volume changes only, so
  // reporting play status or track changes as a Target is not reachable with
  // this build no matter what the profile permits. supportedNotifications()
  // reports what is allowed; declaring anything else is refused with a message
  // that says so. The Controller side has no such limit.
  static constexpr size_t MaximumNotifications = 8;
  // Which notifications this backend allows a Target to declare. The set is
  // fixed by the host build, and setNotificationCapabilities() only accepts a
  // subset of it, so a sketch can check instead of guessing. Returns the number
  // written, or the number available when events is null.
  size_t supportedNotifications(
    EspBleClassicAvrcpNotification *events, size_t capacity) const;
  bool setNotificationCapabilities(
    const EspBleClassicAvrcpNotification *events, size_t count);
  bool respondToNotification(
    const EspBleClassicAvrcpNotificationValue &value);
  bool sendNotificationChanged(
    const EspBleClassicAvrcpNotificationValue &value);
  using NotificationRegisteredCallback =
    std::function<void(EspBleClassicAvrcpNotification)>;
  void onNotificationRegistered(NotificationRegisteredCallback callback);

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
  NotificationRegisteredCallback notificationRegisteredCallback_;
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

  // Carries which server started: with more than one server the channel is
  // the only way to tell them apart, and it is not known until the backend
  // assigns it.
  using ServerStartedCallback =
    std::function<void(const EspBleClassicSppServer &)>;
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

  // Can be called more than once to publish several services. Each call adds a
  // service record; the channel arrives with onServerStarted() because the
  // backend assigns it.
  static constexpr size_t MaximumServers = 4;
  bool startServer(
    const EspBleClassicSppServerConfig &config =
      EspBleClassicSppServerConfig());
  // Stops every server this object started. There is deliberately no
  // per-channel stop: retiring one service is rare enough that stopping all of
  // them and starting the ones still wanted covers it, and it keeps one way of
  // undoing startServer() instead of two.
  bool stopServer();
  // True while at least one server is listening.
  bool serverRunning() const;
  size_t serverCount() const;
  bool server(size_t index, EspBleClassicSppServer &server) const;

  bool connect(const char *address, uint32_t timeoutMilliseconds = 10000);
  // One outgoing connection at a time: a second call while a session is open
  // is refused. Incoming sessions are not limited this way.
  //
  // Connects to a specific RFCOMM channel. Needed once the peer publishes more
  // than one service: discovery returns every channel it offers and there is no
  // way to tell from the outside which one is wanted. Skips discovery, so the
  // caller has to know the channel — from the peer's own report, or from a
  // service whose channel is fixed by agreement.
  bool connectToChannel(
    const char *address, uint8_t channel,
    uint32_t timeoutMilliseconds = 10000);
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
  // Drops the Host's pairing for this device as well as the connection, so the
  // Host forgets it instead of trying to reconnect to a device that is gone.
  bool virtualCableUnplug();

  // Answers a Get_Report request. The reply must use the type and report ID the
  // Host asked for, so a sketch normally passes the request's own fields back.
  bool respondToReportRequest(
    const EspBleClassicHidReportRequest &request,
    const uint8_t *data, size_t length);
  // Refuses a request. Without either answer the Host waits for its own
  // timeout, and some Hosts drop the connection at that point.
  bool refuseReportRequest(EspBleClassicHidRequestError error);

  // The Host decides the protocol mode. A device cannot change it, only observe
  // it, which is why there is no setter.
  EspBleClassicHidProtocolMode protocolMode() const;

  using ReportRequestCallback =
    std::function<void(const EspBleClassicHidReportRequest &)>;
  using ProtocolModeCallback =
    std::function<void(EspBleClassicHidProtocolMode)>;

  void onConnected(ConnectionCallback callback);
  void onDisconnected(ConnectionCallback callback);
  void onOutputReport(ReportCallback callback);
  // Set_Report carries a value the Host wants stored, including Feature
  // reports. Output reports also arrive through onOutputReport().
  void onSetReport(ReportCallback callback);
  void onReportRequested(ReportRequestCallback callback);
  void onProtocolMode(ProtocolModeCallback callback);
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
  ReportCallback setReportCallback_;
  ReportRequestCallback reportRequestedCallback_;
  ProtocolModeCallback protocolModeCallback_;
};

// The Classic HID Device profiles. They take the same reports and expose the
// same calls as their BLE counterparts, because HID over BR/EDR and HID over
// GATT differ in transport rather than in content: a sketch that drives a
// keyboard should not have to be rewritten to change radios.
class EspBleClassicHidKeyboard
{
public:
  using OutputReportCallback =
    std::function<void(const EspBleClassicHidKeyboardLeds &)>;

  // Call before EspBleClassic::begin(): the descriptor is registered with the
  // profile when the stack starts, exactly like the BLE side realizes its HID
  // service at begin().
  bool configure(
    const EspBleClassicHidProfileConfig &config =
      EspBleClassicHidProfileConfig());
  bool configured() const;
  void enableNkro(bool enable = true);
  bool nkroEnabled() const;

  bool sendReport(const EspBleHidKeyboardInputReport &report);
  bool sendReport(const EspBleHidKeyboardNkroReport &report);
  const EspBleHidKeyboardNkroReport &heldState() const;
  bool ready() const;

  bool pressUsage(uint8_t usage, uint8_t modifiers = 0, uint32_t holdMs = 10);
  bool releaseUsage(uint8_t usage);
  bool tapUsage(uint8_t usage, uint8_t modifiers = 0, uint32_t holdMs = 10);
  bool pressKey(char key, uint32_t holdMs = 10);
  bool tapKey(char key, uint32_t holdMs = 10);
  bool write(const char *text, uint32_t interKeyDelayMs = 5);
  bool releaseAll();

  void setLayout(EspBleKeyboardLayout layout);
  EspBleKeyboardLayout layout() const;

  void onOutputReport(OutputReportCallback callback);
  EspBleClassicHidKeyboardLeds ledState() const;

private:
  friend class EspBleClassic;
  friend class EspBleClassicHidDevice;
  explicit EspBleClassicHidKeyboard(EspBleClassic *owner);
  bool sendHeldNkroState();

  EspBleClassic *owner_;
  bool configured_ = false;
  bool nkroEnabled_ = false;
  EspBleHidKeyboardNkroReport nkroState_;
  EspBleKeyboardLayout layout_ = EspBleKeyboardLayout::EnUs;
  EspBleClassicHidKeyboardLeds ledState_;
  OutputReportCallback outputReportCallback_;
};

class EspBleClassicHidMouse
{
public:
  bool configure(
    const EspBleClassicHidProfileConfig &config =
      EspBleClassicHidProfileConfig());
  bool configured() const;
  bool sendReport(const EspBleHidMouseReport &report);
  bool ready() const;

  bool move(int8_t x, int8_t y, int8_t wheel = 0, uint8_t buttons = 0);
  bool wheel(int8_t amount);
  bool press(uint8_t buttons);
  bool release(uint8_t buttons);
  bool click(uint8_t button, uint32_t holdMs = 10);
  bool releaseAll();
  uint8_t buttons() const;

private:
  friend class EspBleClassic;
  explicit EspBleClassicHidMouse(EspBleClassic *owner);

  EspBleClassic *owner_;
  bool configured_ = false;
  uint8_t buttons_ = 0;
};

class EspBleClassicHidConsumerControl
{
public:
  bool configure(
    const EspBleClassicHidProfileConfig &config =
      EspBleClassicHidProfileConfig());
  bool configured() const;
  bool ready() const;
  bool sendUsage(uint16_t usage);
  bool release();
  bool click(uint16_t usage, uint32_t holdMs = 10);
  uint16_t usage() const;

private:
  friend class EspBleClassic;
  explicit EspBleClassicHidConsumerControl(EspBleClassic *owner);

  EspBleClassic *owner_;
  bool configured_ = false;
  uint16_t usage_ = 0;
};

class EspBleClassicHidSystemControl
{
public:
  bool configure(
    const EspBleClassicHidProfileConfig &config =
      EspBleClassicHidProfileConfig());
  bool configured() const;
  bool ready() const;
  bool sendUsage(uint8_t usage);
  bool release();
  bool click(uint8_t usage, uint32_t holdMs = 10);
  uint8_t usage() const;

private:
  friend class EspBleClassic;
  explicit EspBleClassicHidSystemControl(EspBleClassic *owner);

  EspBleClassic *owner_;
  bool configured_ = false;
  uint8_t usage_ = 0;
};

class EspBleClassicHidGamepad
{
public:
  bool configure(
    const EspBleClassicHidProfileConfig &config =
      EspBleClassicHidProfileConfig());
  bool configured() const;
  bool ready() const;
  bool sendReport(const EspBleHidGamepadReport &report);
  // The axes in the order the report declares them, so a sketch can send a
  // whole stick position without building a report first. Same signature as the
  // BLE side.
  bool send(
    int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry,
    uint8_t hat, uint32_t buttons);
  bool releaseAll();

private:
  friend class EspBleClassic;
  explicit EspBleClassicHidGamepad(EspBleClassic *owner);

  EspBleClassic *owner_;
  bool configured_ = false;
};

// The decoded keyboard state a Classic Host reports, mirroring the BLE side.
struct EspBleClassicHidKeyboardState
{
  static constexpr size_t BitmapSize = 32;

  String peerAddress;
  uint8_t modifiers = 0;
  // One bit per usage, so any number of simultaneous keys can be reported.
  uint8_t bitmap[BitmapSize] = {};

  bool isDown(uint8_t usage) const
  {
    return (bitmap[usage >> 3] & static_cast<uint8_t>(1u << (usage & 7))) != 0;
  }
};

struct EspBleClassicHidKeyboardEvent
{
  String peerAddress;
  uint8_t usage = 0;
  // Unicode code point for the selected layout, 0 when the usage produces no
  // character. `ascii` is its ISO-8859-1 subset.
  uint16_t unicode = 0;
  uint8_t ascii = 0;
  uint8_t modifiers = 0;
  bool pressed = false;
  bool released = false;
  // The report this event was decoded from; several events can share one.
  const uint8_t *rawData = nullptr;
  size_t rawLength = 0;
};

struct EspBleClassicHidMouseEvent
{
  String peerAddress;
  int16_t x = 0;
  int16_t y = 0;
  int16_t wheel = 0;
  uint8_t buttons = 0;
  bool moved = false;
  bool buttonsChanged = false;
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

  // Asking the device for a report instead of waiting for one. The answer
  // arrives through onReportResult(), because the exchange is a round trip.
  bool requestReport(
    EspBleClassicHidReportType type, uint8_t reportId, uint16_t maximumLength);
  // Sends a report the device is expected to store. The first byte is the
  // report ID when the device declares one, as with sendOutputReport().
  bool sendReport(
    EspBleClassicHidReportType type, const uint8_t *data, size_t length);
  // Boot Protocol is what a restricted Host uses; a sketch acting as Host can
  // put a device into it and read back which mode is in effect.
  bool requestProtocolMode();
  bool setProtocolMode(EspBleClassicHidProtocolMode mode);
  // The idle rate is how often the device repeats a report with no change.
  // Zero means "only on change", which is what most devices default to.
  bool requestIdleRate();
  bool setIdleRate(uint8_t idleRate);
  // Makes the device forget this Host, rather than only closing the link.
  bool virtualCableUnplug();

  // The LED report, named and ordered as on the BLE side. The report ID comes
  // from the peer's Report Descriptor, so this needs the descriptor to have
  // arrived; sendOutputReport() stays available for anything else.
  bool setKeyboardLeds(
    bool numLock,
    bool capsLock,
    bool scrollLock,
    bool compose = false,
    bool kana = false);

  using KeyboardStateCallback =
    std::function<void(const EspBleClassicHidKeyboardState &)>;
  using KeyboardCallback =
    std::function<void(const EspBleClassicHidKeyboardEvent &)>;
  using MouseCallback =
    std::function<void(const EspBleClassicHidMouseEvent &)>;

  void onConnected(ConnectionCallback callback);
  void onDisconnected(ConnectionCallback callback);
  void onConnectionFailed(ConnectionFailureCallback callback);
  // Every input report, decoded or not. A device with a descriptor this
  // library cannot classify still reaches the sketch here.
  void onInputReport(ReportCallback callback);

  // Decoded events, named as on the BLE side. They need the peer's Report
  // Descriptor, which arrives with the connection.
  void onKeyboardState(KeyboardStateCallback callback);
  void onKeyboard(KeyboardCallback callback);
  void onMouse(MouseCallback callback);
  void setKeyboardLayout(EspBleKeyboardLayout layout);
  EspBleKeyboardLayout keyboardLayout() const;
  bool reportMapKnown() const;

  // Results of the round-trip requests above. Each carries whether the device
  // answered, because a device may refuse a request it does not support.
  struct ReportResult
  {
    bool success = false;
    // Carries the report ID in front of the payload when the device declares
    // one, the same shape onInputReport() delivers.
    String value;
  };
  struct ProtocolModeResult
  {
    bool success = false;
    EspBleClassicHidProtocolMode mode = EspBleClassicHidProtocolMode::Report;
  };
  struct IdleRateResult
  {
    bool success = false;
    uint8_t idleRate = 0;
  };
  using ReportResultCallback = std::function<void(const ReportResult &)>;
  using ProtocolModeResultCallback =
    std::function<void(const ProtocolModeResult &)>;
  using IdleRateResultCallback = std::function<void(const IdleRateResult &)>;

  void onReportResult(ReportResultCallback callback);
  // Reports whether a sendReport() was accepted; there is no value to return.
  void onReportSent(ReportResultCallback callback);
  void onProtocolMode(ProtocolModeResultCallback callback);
  void onIdleRate(IdleRateResultCallback callback);

  size_t droppedEventCount() const;
  // Reports whose length does not match the descriptor. Counting beats
  // dropping them silently: "discovered but no keys arrive" is unexplainable.
  size_t invalidInputReportCount() const;

private:
  friend class EspBleClassic;
  explicit EspBleClassicHidHost(EspBleClassic *owner);
  void deliverDecoded(const EspBleClassicHidReport &report);
  ~EspBleClassicHidHost();
  void update();

  EspBleClassic *owner_;
  EspBleClassicHidHostImpl *impl_ = nullptr;
  ConnectionCallback connectedCallback_;
  ConnectionCallback disconnectedCallback_;
  ConnectionFailureCallback connectionFailureCallback_;
  ReportCallback inputReportCallback_;
  KeyboardStateCallback keyboardStateCallback_;
  KeyboardCallback keyboardCallback_;
  MouseCallback mouseCallback_;
  ReportResultCallback reportResultCallback_;
  ReportResultCallback reportSentCallback_;
  ProtocolModeResultCallback protocolModeCallback_;
  IdleRateResultCallback idleRateCallback_;
  EspBleKeyboardLayout keyboardLayout_ = EspBleKeyboardLayout::EnUs;
};

// What a peer says it offers, asked for by address rather than found by
// scanning. Inquiry reports that a device exists; this reports what it is for.
struct EspBleClassicRemoteServices
{
  static constexpr size_t MaximumServices = 12;
  String peerAddress;
  bool success = false;
  // Service UUIDs as text, in the form the BLE side of this library uses.
  // Truncated at MaximumServices; the count says how many were reported.
  String uuids[MaximumServices];
  size_t count = 0;
  size_t reportedCount = 0;
};

// A name asked for directly. An inquiry result may carry no name at all, and
// the name can take longer to fetch than the inquiry response itself.
struct EspBleClassicRemoteName
{
  String peerAddress;
  bool success = false;
  String name;
};

class EspBleClassicInquiry
{
public:
  using ResultCallback =
    std::function<void(const EspBleClassicInquiryResult &)>;
  using CompleteCallback =
    std::function<void(const EspBleClassicInquiryComplete &)>;
  using RemoteServicesCallback =
    std::function<void(const EspBleClassicRemoteServices &)>;
  using RemoteNameCallback =
    std::function<void(const EspBleClassicRemoteName &)>;

  // One scan at a time. Results arrive from update() as the controller reports
  // them, and the same peer can be reported more than once during a scan.
  bool start(
    const EspBleClassicInquiryConfig &config = EspBleClassicInquiryConfig());
  bool stop();
  bool running() const;

  // Both are round trips over SDP or a name request, so the answer arrives at
  // the matching callback rather than as a return value. One query of each kind
  // at a time; a second while one is outstanding is refused.
  //
  // Not while a scan runs: an inquiry and a query both need the radio, and a
  // query issued during a scan is accepted but never answered. Wait for
  // onComplete(), or stop() first.
  bool requestServices(const char *address);
  bool requestName(const char *address);

  void onResult(ResultCallback callback);
  void onComplete(CompleteCallback callback);
  void onRemoteServices(RemoteServicesCallback callback);
  void onRemoteName(RemoteNameCallback callback);
  size_t droppedResultCount() const;

private:
  friend class EspBleClassic;
  explicit EspBleClassicInquiry(EspBleClassic *owner);
  ~EspBleClassicInquiry();
  bool begin();
  void end();
  void update();

  EspBleClassic *owner_;
  EspBleClassicInquiryImpl *impl_ = nullptr;
  ResultCallback resultCallback_;
  CompleteCallback completeCallback_;
  RemoteServicesCallback remoteServicesCallback_;
  RemoteNameCallback remoteNameCallback_;
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

  EspBleClassicInquiry &inquiry();
  EspBleClassicSpp &spp();
  EspBleClassicHidKeyboard &hidKeyboard();
  EspBleClassicHidMouse &hidMouse();
  EspBleClassicHidConsumerControl &hidConsumerControl();
  EspBleClassicHidSystemControl &hidSystemControl();
  EspBleClassicHidGamepad &hidGamepad();
  EspBleClassicHidDevice &hidDevice();
  EspBleClassicHidHost &hidHost();
  EspBleClassicA2dpSink &a2dpSink();
  EspBleClassicA2dpSource &a2dpSource();
  EspBleClassicAvrcp &avrcp();
  EspBleClassicHfpClient &hfpClient();
  EspBleClassicHfpAudioGateway &hfpAudioGateway();

  using SecurityChangedCallback =
    std::function<void(const EspBleClassicSecurityChanged &)>;
  using NumericComparisonCallback =
    std::function<void(const EspBleClassicNumericComparison &)>;
  using PasskeyDisplayedCallback =
    std::function<void(const EspBleClassicPasskeyDisplayed &)>;
  using PasskeyRequestedCallback =
    std::function<void(const EspBleClassicPasskeyRequested &)>;

  void onSecurityChanged(SecurityChangedCallback callback);
  // Delivered only with DisplayYesNo. Answer with confirmNumericComparison();
  // a request nobody answers before the timeout is rejected.
  void onNumericComparisonRequested(NumericComparisonCallback callback);
  // Delivered with DisplayOnly: show this passkey to the user.
  void onPasskeyDisplayed(PasskeyDisplayedCallback callback);
  // Delivered with KeyboardOnly. Answer with providePasskey().
  void onPasskeyRequested(PasskeyRequestedCallback callback);

  bool confirmNumericComparison(const char *peerAddress, bool accept);
  bool providePasskey(const char *peerAddress, uint32_t passkey);

  // Bonds live in NVS and outlive a restart, so a sketch needs to be able to
  // list and drop them without erasing the whole partition.
  size_t bondCount() const;
  bool bond(size_t index, EspBleClassicBond &bond) const;
  bool deleteBond(const EspBleClassicBond &bond);
  bool deleteAllBonds();

  // Visibility and Class of Device can both change while running: a device may
  // want to be discoverable only while a user is pairing it, and a composed
  // device may only know which class it is after the sketch decided which
  // profiles to start.
  bool setVisibility(EspBleClassicVisibility visibility);
  EspBleClassicVisibility visibility() const;
  // The backend applies the class on its own task, and starting a profile
  // rewrites it from that profile's service records before this library
  // restores it. So true means the request was accepted, not that the class is
  // already in effect, and classOfDevice() read back immediately still reports
  // the previous value.
  bool setClassOfDevice(const EspBleClassicClassOfDevice &classOfDevice);
  bool classOfDevice(EspBleClassicClassOfDevice &classOfDevice) const;

  EspBleError lastError() const;
  const char *lastErrorName() const;
  const String &lastErrorDetail() const;

private:
  friend class EspBleClassicInquiry;
  friend class EspBleClassicHidKeyboard;
  friend class EspBleClassicHidMouse;
  friend class EspBleClassicHidConsumerControl;
  friend class EspBleClassicHidSystemControl;
  friend class EspBleClassicHidGamepad;
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
  // Records a profile for the composed HID Device descriptor. The device is
  // registered when the stack starts, so every profile must be configured
  // before begin().
  bool configureHidProfile(
    uint8_t profile, const EspBleClassicHidProfileConfig &config);
  void setHidKeyboardNkro(bool enable);
  bool startComposedHidDevice();
  void deliverHidKeyboardLeds(const EspBleClassicHidReport &report);

  uint8_t hidProfileMask_ = 0;
  bool hidKeyboardNkro_ = false;
  uint8_t hidMouseButtonCount_ = 5;
  EspBleClassicHidProfileConfig hidProfileConfig_;
  uint8_t hidProfileDescriptor_[512] = {};
  size_t hidProfileDescriptorLength_ = 0;

  EspBleClassicImpl *impl_ = nullptr;
  SecurityChangedCallback securityChangedCallback_;
  NumericComparisonCallback numericComparisonCallback_;
  PasskeyDisplayedCallback passkeyDisplayedCallback_;
  PasskeyRequestedCallback passkeyRequestedCallback_;
  EspBleClassicInquiry inquiry_;
  EspBleClassicHidKeyboard hidKeyboard_;
  EspBleClassicHidMouse hidMouse_;
  EspBleClassicHidConsumerControl hidConsumerControl_;
  EspBleClassicHidSystemControl hidSystemControl_;
  EspBleClassicHidGamepad hidGamepad_;
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
