# Unit Tests

host上で実行する純粋C++/データ変換のテストです。実機とシリアルポートは不要で、g++でビルドして実行します。

```sh
uv run --env-file .env pytest unit/
```

## 追加済み

- `keymap`: `src/EspBleKeymap.h`のHID usage→文字変換(`espBleUsageToUnicode`/`espBleUsageToAscii`)を、各layoutの一次ソース(Windows layoutデータ)由来の期待値で検証する。AltGr層の選択とfallback、文字ペア判定CapsLock、dead key→0、非Latin-1文字の`ascii`=0(EspUsbHost互換仕様)の挙動を固定する。
- `report_map`: `src/EspBleHidReportMap.h`のHID Report Map parserを検証する。項目順序が異なるkeyboard、Report IDなしのboot keyboard、Consumer Control併載機、mouse-only descriptorや途中で切れたdescriptorを扱う。
