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
EspBle <version>
```

`<version>`は`espble_version.h`で定義されるライブラリのバージョンです。

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[CompileSmoke.ino](CompileSmoke.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。この例に入力コマンドはありません。

## この例の構成

CompileSmoke - ライブラリが対象ボードでビルド・リンクできることだけを確認する 最小sketch。BLEスタックは初期化せず、ヘッダのincludeとバージョン表示のみ行う。

## 確認と変更のポイント

ライブラリをincludeしてバージョンを表示するだけの例です。コンパイル成功やバージョン表示では、スタック初期化・無線・ペアリング・相互運用は検証できません。

[サンプル一覧](../README.ja.md) · [サンプル実行ガイド](../../docs/GUIDE_EXAMPLES.ja.md)
