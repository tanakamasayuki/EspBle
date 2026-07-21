# Unit Tests

> English: [README.md](README.md)

host上で実行する純粋C++/データ変換のテストです。実機とシリアルポートは不要で、g++でビルドして実行します。

```sh
uv run --env-file .env pytest unit/
```

## 追加済み

- `keymap`: `src/EspBleKeymap.h`のHID usage→文字変換(`espBleUsageToUnicode`/`espBleUsageToAscii`)を、各layoutの一次ソース(Windows layoutデータ)由来の期待値で検証する。AltGr層の選択とfallback、文字ペア判定CapsLock、dead key→0、非Latin-1文字の`ascii`=0(EspUsbHost互換仕様)の挙動を固定する。
- `report_map`: `src/EspBleHidReportMap.h`のHID Report Map parserを検証する。項目順序が異なるkeyboard、Report IDなしのboot keyboard、Consumer Control併載機、mouse-only descriptorや途中で切れたdescriptorを扱う。
- `midi`: `src/EspBleMidi.h`のBLE MIDI packet codecを検証する。timestamp header/lowバイトのデコード、running status（timestampバイト有無の両方）、System Real-Time割り込み、単一/2パケットのSystem Exclusive、異常系パケット、packet builder（単一/複数メッセージ、timestamp window拒否、overflow、round trip）、複数パケットSysEx encoder（単一パケット出力、分割encode→parse round trip、framing検証）を扱う。
- `medical_float`: `src/EspBleMedicalFloat.h`のIEEE-11073 medical float codecを検証する。32-bit FLOATと16-bit SFLOATのencode/decode round trip、正確なlittle-endianバイト配置、負のmantissa、Health Thermometer/Blood Pressure/Glucoseで使う予約値（NaN / NRes / ±INFINITY）を扱う。
