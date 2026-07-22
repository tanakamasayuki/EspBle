# CompileSmoke

> English: [README.md](README.md)

最小のビルド確認用sketchです。`EspBle.h`をincludeしてライブラリのバージョンを表示するだけで、BLEスタックは初期化しません。機能系のexampleを試す前に、対象ボードでライブラリがコンパイル・リンクできることを確認する用途に使います。

## 必要なもの

- 1 × ESP32-S3（またはArduino-ESP32のNimBLE backendを持つ任意のボード）。peerは不要

## 動作

- 起動時にEspBleのライブラリバージョンをSerialへ1回表示します
- それ以外は何もしません（loopはidle）

## 主なAPI

- `ESPBLE_VERSION_STR` — `espble_version.h`で定義されるバージョン文字列

## 期待されるSerial出力

```
EspBle 0.1.0
```
