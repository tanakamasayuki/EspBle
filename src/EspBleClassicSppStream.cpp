#include "EspBleClassic.h"

// A Stream over an SPP session. There is no backend call here: everything goes
// through EspBleClassicSpp's session API, so this file builds on every target
// and the adapter cannot get out of step with the session it borrows.

EspBleClassicSppStream::EspBleClassicSppStream(
  EspBleClassicSpp &spp, EspBleClassicSppSessionId sessionId)
{
  attach(spp, sessionId);
}

void EspBleClassicSppStream::attach(
  EspBleClassicSpp &spp, EspBleClassicSppSessionId sessionId)
{
  spp_ = &spp;
  sessionId_ = sessionId;
}

void EspBleClassicSppStream::detach()
{
  spp_ = nullptr;
  sessionId_ = 0;
}

bool EspBleClassicSppStream::attached() const
{
  return spp_ != nullptr && sessionId_ != 0;
}

bool EspBleClassicSppStream::connected() const
{
  if (!attached()) return false;
  EspBleClassicSppSession session;
  return spp_->session(sessionId_, session);
}

EspBleClassicSppSessionId EspBleClassicSppStream::session() const
{
  return sessionId_;
}

void EspBleClassicSppStream::setWriteTimeout(uint32_t milliseconds)
{
  writeTimeoutMs_ = milliseconds;
}

uint32_t EspBleClassicSppStream::writeTimeout() const
{
  return writeTimeoutMs_;
}

int EspBleClassicSppStream::available()
{
  if (!attached()) return 0;
  return static_cast<int>(spp_->available(sessionId_));
}

int EspBleClassicSppStream::read()
{
  if (!attached()) return -1;
  return spp_->read(sessionId_);
}

int EspBleClassicSppStream::peek()
{
  if (!attached()) return -1;
  return spp_->peek(sessionId_);
}

size_t EspBleClassicSppStream::write(uint8_t value)
{
  return write(&value, 1);
}

size_t EspBleClassicSppStream::write(const uint8_t *buffer, size_t size)
{
  if (!attached() || buffer == nullptr || size == 0) return 0;
  size_t written = 0;
  while (written < size)
  {
    // One queued write carries at most MaximumWriteSize bytes, so a larger
    // buffer becomes several packets rather than being refused.
    const size_t chunk =
      min(size - written, EspBleClassicSpp::MaximumWriteSize);
    // The queue drains on the backend's task as each write completes, so waiting
    // here does not need update() to run and cannot re-enter a callback.
    const uint32_t deadline = millis() + writeTimeoutMs_;
    while (!spp_->write(sessionId_, buffer + written, chunk))
    {
      if (writeTimeoutMs_ == 0 ||
          static_cast<int32_t>(millis() - deadline) >= 0)
        return written;
      // Give up rather than spin forever once the session is gone: a closed
      // session never makes room.
      if (!connected()) return written;
      delay(1);
    }
    written += chunk;
  }
  return written;
}

int EspBleClassicSppStream::availableForWrite()
{
  if (!attached()) return 0;
  const size_t pending = spp_->pendingWriteCount(sessionId_);
  if (pending >= EspBleClassicSpp::WriteQueueCapacity) return 0;
  // What one more write() call can take, not the whole queue: a single call is
  // capped at MaximumWriteSize either way.
  return static_cast<int>(EspBleClassicSpp::MaximumWriteSize);
}

void EspBleClassicSppStream::flush()
{
  if (!attached()) return;
  const uint32_t deadline = millis() + writeTimeoutMs_;
  while (spp_->pendingWriteCount(sessionId_) != 0)
  {
    if (writeTimeoutMs_ == 0 ||
        static_cast<int32_t>(millis() - deadline) >= 0)
      return;
    if (!connected()) return;
    delay(1);
  }
}
