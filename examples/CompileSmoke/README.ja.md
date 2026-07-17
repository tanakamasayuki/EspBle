# CompileSmoke

> English: [README.md](README.md)

最小のビルド確認用sketchです。`EspBle.h`をincludeしてライブラリのバージョンを表示するだけで、BLEスタックは初期化しません。機能系のexampleを試す前に、対象ボードでライブラリがコンパイル・リンクできることを確認する用途に使います。

## ハードウェア

- Arduino-ESP32がNimBLE backendを提供するESP32ボード（初期ターゲットはESP32-S3）

## 動作内容

- 起動時にEspBleのライブラリバージョンをSerialへ1回表示します

## 主なAPI

- `ESPBLE_VERSION_STR` — `espble_version.h`で定義されるバージョン文字列

## 期待されるSerial出力

```
EspBle 0.1.0
```
