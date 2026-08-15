# HfpAudioGatewayRaw (Classic)

> 日本語版: [README.ja.md](README.ja.md)

The phone side of HFP. A headset connects to this board, presses its buttons, and
this sketch answers as if it were a phone — which is what makes it useful for
testing a headset without a phone. **Classic works on the original ESP32 only**,
and it reaches Classic HFP headsets and hands-free units. **The Client and the
Audio Gateway roles are mutually exclusive within one process**, so this cannot
also be [HfpClientRaw](../HfpClientRaw/).

## Hardware

- 1 × original ESP32 running this sketch
- 1 × HFP headset, or a second original ESP32 running
  [HfpClientRaw](../HfpClientRaw/)

## What it does

- Answers the service-level connection with its operator name and subscriber
  number
- Handles dial, answer and hang-up, driving the call state the headset sees
- Prints each SCO frame's codec, length and bad-frame flag

## Key APIs

- `EspBleClassicHfpAudioGatewayConfig` — `operatorName`, `subscriberNumber` and
  `preferredAudioCodec`; the first two are what `AT+COPS` and `AT+CNUM` return
- `onCommand()` — what the headset asked for. `Dial`, `Answer`, `Hangup`, `Dtmf`,
  `VoiceRecognition`, `NoiseReduction`, `DialMemory` and `UnknownAt`
- `respondToCommand(accepted)` — accept or refuse; a command left unanswered
  leaves the headset waiting
- `reportOutgoingCall()` / `reportCallActive()` / `reportCallEnded()` /
  `reportIncomingCall()` — the call state the headset displays
- `respondToUnknownAt(text)` — answer an AT command the backend does not decode,
  such as the Apple extensions; the OK that ends the exchange is sent for you

## Notes

`DialMemory` carries a position in the phone's memory, not a number. Dialling the
position as if it were digits would call the wrong party, which is why it arrives
as its own command type.

**Copy the SCO payload before returning from `onAudio()`.** Decoding CVSD or mSBC,
buffering and device I/O belong outside EspBle. On hardware a 57-byte mSBC
transmission arrives as a padded 58 or 60-byte view, and a bad frame arrives as 60
bytes too — pass the length and the flag through so a decoder can conceal the loss.

This Audio Gateway has a single-call model on purpose: call waiting and three-way
calling are not implemented.

## Related guides

- [Classic guide §8 HFP](../../../docs/GUIDE_CLASSIC_BASICS.md#8-hfp) — the service-level connection, call control and raw SCO
