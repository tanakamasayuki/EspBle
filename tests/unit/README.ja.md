# Unit Tests

host上で実行する純粋C++/データ変換のテストです。実機とシリアルポートは不要で、g++でビルドして実行します。

```sh
uv run --env-file .env pytest unit/
```

## 追加済み

- `keymap`: `src/EspBleKeymap.h`のHID usage→8-bit文字変換を、各layoutの一次ソース(Windows layoutデータ)由来の期待値で検証する。dead key→0、ISO-8859-1、AltGr非解釈・位置ベースCapsLock(EspUsbHost互換仕様)の挙動も固定する。
