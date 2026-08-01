# ESP32-P4 / ESP-Hosted セットアップ

実機で確認済みのSecurityと再初期化の制限は
[ESP32-P4 / ESP-Hostedの既知制限](ESP_HOSTED_LIMITATIONS.ja.md)を参照する。

## 責務の分担

ESP-Hosted co-processor firmwareの配布、version判定、書き込み、activateは
Arduino-ESP32 Coreの責務とする。EspBleは次を担当する。

- NimBLE HostとESP-Hostedの正しい起動・終了順序
- Hostedで利用できないController APIの明示的なerror
- 検証済みArduino Core / ESP-Hosted firmwareの記録
- firmware不一致時の診断方法と公式Updaterへの案内

EspBleの`begin()`からfirmwareを自動更新しない。更新にはWi-Fi接続が必要で、永続的な
書き換えと再起動を伴い、通常のBLE初期化より大きな副作用があるためである。

## 対応version

初期検証環境は次の組み合わせとする。

| Component | Version |
| --- | --- |
| Arduino-ESP32 Core | 3.3.11 |
| P4側 ESP-Hosted Host | 2.12.11 |
| C6側 ESP-Hosted Slave | 2.12.11を推奨 |

HostとSlaveはArduino Coreが提供する同じversionへ揃える。実機に入っていたSlave
2.3.2では、基本的なGATT通信は成功したがLE Secure ConnectionsのDHKey checkに
失敗した。2.12.11へ更新後もDHKey check failureは再現したため、versionを揃えることは
必要な初期条件だが、現時点ではSecurity対応の解決策ではない。

## 公式Updater

Arduino-ESP32 3.3.11には`ESP_HostedOTA`ライブラリと
`ESP_HostedOTA` exampleが同梱されている。Arduino IDEでは次から開く。

```text
File > Examples > ESP_HostedOTA > ESP_HostedOTA
```

exampleは次の処理を行う。

1. ESP-Hosted経由でWi-Fiへ接続する。
2. `hostedHasUpdate()`でHost/Slave versionを比較する。
3. Coreが指定する公式URLから対象C6 firmwareを取得する。
4. `hostedBeginUpdate()`、`hostedWriteUpdate()`、`hostedEndUpdate()`で書き込む。
5. `hostedActivateUpdate()`後にP4を再起動する。

Core 3.3.11 / ESP32-C6の場合の配布先は次である。

```text
https://espressif.github.io/arduino-esp32/hosted/esp32c6-v2.12.11.bin
```

## 更新手順

1. P4とC6へ安定した電源を供給する。
2. 公式exampleの`ssid`と`password`を更新用Wi-Fiへ合わせる。
3. boardをESP32-P4、Coreを3.3.11としてP4へuploadする。
4. Serial Monitorを115200 baudで開く。
5. `SUCCESS: esp-hosted co-processor updated!`と再起動を確認する。
6. EspBleのtest sketchをP4へ戻す。
7. 起動logでHost/Slaveがともに2.12.11であることを確認する。
8. `security_bond`、`hid_keyboard_host`、`lifecycle_stress`を再実行する。

更新中はP4/C6の電源を切らない。更新前後でbond情報が残っている場合は、双方のbondを
消去してからSecurity testを再実行する。

## 今回の実機更新記録

検証機ではSlave 2.3.2から公式2.12.11への更新とactivateに成功し、更新後の
EspBle test logでHost/Slaveがともに2.12.11であることを確認した。

古い2.3.2では対象2.4 GHz APをscanできたものの、公式exampleのWi-Fi接続が
`WL_DISCONNECTED`のまま進まなかった。このため今回に限り、公式
`hostedBeginUpdate()` / `hostedWriteUpdate()` / `hostedEndUpdate()` /
`hostedActivateUpdate()`を使い、PCで取得した公式binをUSB Serial経由でP4へ渡した。
この復旧用transportはArduino Coreの標準手順ではないためEspBleには収録しない。

## CLI環境での注意

repositoryへWi-Fi認証情報をcommitしない。公式exampleを一時directoryへcopyし、
認証情報を設定してuploadした後、その一時directoryを破棄する。EspBle本体やtest用の
`.env`へWi-Fi passwordを追加する必要はない。
