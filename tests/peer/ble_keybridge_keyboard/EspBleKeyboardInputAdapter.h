#pragma once

#include <ESP32KeyBridge.h>
#include <EspBle.h>
#include <cstring>
#include <functional>
#include <utility>

// Prototype integration adapter. It lives in this test until the EspBle API
// is stable enough for the production adapter to move to ESP32KeyBridge.
class EspBleKeyboardInputAdapter : public esp32keybridge::InputAdapter
{
public:
  static constexpr size_t MaxKeyboards = 4;
  using DiscoveryCallback =
    std::function<void(const EspBleHidKeyboardHostDiscovery &result)>;

  explicit EspBleKeyboardInputAdapter(EspBle &ble) : ble_(ble) {}

  ~EspBleKeyboardInputAdapter() override
  {
    if (discoveryListenerId_ != EspBleInvalidListenerId)
    {
      ble_.hidKeyboardHost().removeListener(discoveryListenerId_);
    }
    if (stateListenerId_ != EspBleInvalidListenerId)
    {
      ble_.hidKeyboardHost().removeListener(stateListenerId_);
    }
  }

  bool discover(EspBleConnectionId connectionId)
  {
    subscribe();
    return ble_.hidKeyboardHost().discover(connectionId);
  }

  void onDiscovered(DiscoveryCallback callback)
  {
    discoveryCallback_ = std::move(callback);
  }

  void update() override
  {
    subscribe();
    ble_.update();

    for (size_t i = keyboardCount_; i > 0; --i)
    {
      if (!ble_.hidKeyboardHost().ready(keyboards_[i - 1].connectionId))
      {
        keyboards_[i - 1] = keyboards_[keyboardCount_ - 1];
        --keyboardCount_;
      }
    }

    keys_.clear();
    for (size_t i = 0; i < keyboardCount_; ++i)
    {
      for (size_t byte = 0; byte < EspBleHidKeyboardState::BitmapSize; ++byte)
      {
        uint8_t bits = keyboards_[i].keys[byte];
        while (bits != 0)
        {
          const int bit = __builtin_ctz(bits);
          bits = static_cast<uint8_t>(bits & (bits - 1));
          const uint16_t usage = static_cast<uint16_t>(byte * 8 + bit);
          if (usage >= 0x04)
          {
            keys_.press(esp32keybridge::keyboardKey(usage));
          }
        }
      }
    }
  }

  const esp32keybridge::KeySet &keys() const override { return keys_; }
  bool connected() const override { return keyboardCount_ > 0; }

  void setLockState(const esp32keybridge::LockState &state) override
  {
    for (size_t i = 0; i < keyboardCount_; ++i)
    {
      ble_.hidKeyboardHost().setKeyboardLeds(
        keyboards_[i].connectionId,
        state.numLock,
        state.capsLock,
        state.scrollLock,
        false,
        state.kana);
    }
  }

private:
  struct Keyboard
  {
    EspBleConnectionId connectionId = 0;
    uint8_t keys[EspBleHidKeyboardState::BitmapSize] = {};
  };

  void subscribe()
  {
    if (subscribed_)
    {
      return;
    }
    subscribed_ = true;
    discoveryListenerId_ = ble_.hidKeyboardHost().addDiscoveredListener(
      [this](const EspBleHidKeyboardHostDiscovery &result) {
        if (result.success)
        {
          Keyboard *keyboard = find(result.connectionId);
          if (keyboard == nullptr && keyboardCount_ < MaxKeyboards)
          {
            keyboard = &keyboards_[keyboardCount_++];
            keyboard->connectionId = result.connectionId;
          }
        }
        if (discoveryCallback_)
        {
          discoveryCallback_(result);
        }
      });
    stateListenerId_ = ble_.hidKeyboardHost().addKeyboardStateListener(
      [this](const EspBleHidKeyboardState &state) {
        Keyboard *keyboard = find(state.connectionId);
        if (keyboard != nullptr)
        {
          memcpy(keyboard->keys, state.keys, sizeof(keyboard->keys));
        }
      });
  }

  Keyboard *find(EspBleConnectionId connectionId)
  {
    for (size_t i = 0; i < keyboardCount_; ++i)
    {
      if (keyboards_[i].connectionId == connectionId)
      {
        return &keyboards_[i];
      }
    }
    return nullptr;
  }

  EspBle &ble_;
  bool subscribed_ = false;
  EspBleListenerId discoveryListenerId_ = EspBleInvalidListenerId;
  EspBleListenerId stateListenerId_ = EspBleInvalidListenerId;
  Keyboard keyboards_[MaxKeyboards];
  size_t keyboardCount_ = 0;
  esp32keybridge::KeySet keys_;
  DiscoveryCallback discoveryCallback_;
};
