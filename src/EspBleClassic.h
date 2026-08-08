#ifndef ESP_BLE_CLASSIC_H
#define ESP_BLE_CLASSIC_H

#include <Arduino.h>
#include <functional>
#include <stdint.h>

#include "EspBleTypes.h"

using EspBleClassicSppSessionId = uint32_t;

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

struct EspBleClassicImpl;
struct EspBleClassicSppImpl;
class EspBleClassic;

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

  EspBleError lastError() const;
  const char *lastErrorName() const;
  const String &lastErrorDetail() const;

private:
  friend class EspBleClassicSpp;

  void clearError();
  void setError(EspBleError error, const char *detail);

  EspBleClassicImpl *impl_ = nullptr;
  EspBleClassicSpp spp_;
  EspBleError lastError_ = EspBleError::None;
  String lastErrorDetail_;
};

#endif
