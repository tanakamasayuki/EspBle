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

## SDIO pinの選択と上書き

ESP-HostedのSDIO pinはArduino Coreが管理する。EspBleは`begin()`内で
`hostedInitBLE()`を呼ぶが、pin番号を独自に保持したり変更したりしない。

### 推奨: 実機に合うboardを選ぶ

Coreは選択されたboardの`variants/<board>/pins_arduino.h`に
`BOARD_HAS_SDIO_ESP_HOSTED`があれば、次のmacroを初期値として使用する。

```cpp
BOARD_SDIO_ESP_HOSTED_CLK
BOARD_SDIO_ESP_HOSTED_CMD
BOARD_SDIO_ESP_HOSTED_D0
BOARD_SDIO_ESP_HOSTED_D1
BOARD_SDIO_ESP_HOSTED_D2
BOARD_SDIO_ESP_HOSTED_D3
BOARD_SDIO_ESP_HOSTED_RESET
```

代表的な定義は次のとおり。Tab5をgeneric ESP32-P4としてbuildするとpinが一致しないため、
Arduino IDEでは`M5Tab5`、CLIでは`esp32:esp32:m5stack_tab5`を選ぶ。

| Signal | generic ESP32-P4 | M5Stack Tab5 |
| --- | ---: | ---: |
| CLK | 18 | 12 |
| CMD | 19 | 13 |
| D0 | 14 | 11 |
| D1 | 15 | 10 |
| D2 | 16 | 9 |
| D3 | 17 | 8 |
| RESET | 54 | 15 |

### 回避策: 初期化前に実行時上書きする

独自基板や、installed Coreにboard variantがまだ無い場合は、Arduino Coreの
`hostedSetPins()`で7本をまとめて上書きできる。必ず`ble.begin()`より前に呼ぶ。

```cpp
#include <EspBle.h>

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#include "esp32-hal-hosted.h"
#endif

EspBle ble;

void setup()
{
  Serial.begin(115200);

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  // M5Stack Tab5: CLK, CMD, D0, D1, D2, D3, RESET
  if (!hostedSetPins(12, 13, 11, 10, 9, 8, 15))
  {
    Serial.println("ESP-Hosted SDIO pin configuration failed");
    return;
  }
#endif

  if (!ble.begin())
  {
    Serial.println(ble.lastErrorDetail());
  }
}
```

Wi-Fiを先に開始する場合は`WiFi.STA.begin()`より前に設定する。Wi-FiとBLEは同じ
Hosted transportとpin設定を共有する。Coreは次を拒否する。

- Hosted初期化後の変更
- 負数を含むpin
- D0だけのような一部の指定（CLK/CMD/D0〜D3/RESETの全指定が必要）

上書きはRAM上だけに保持され、再起動するとboard variantの値へ戻る。現在値は
`hostedGetPins()`で取得できる。動作するsketchは
[Hosted/CustomPins example](../examples/Hosted/CustomPins/)を参照する。

pinはWi-Fi/BLE共有SDIO transportの設定なので、EspBle固有APIとして重複させず、
Arduino CoreのHosted HALを利用する。

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
firmware更新用の認証情報を設定してuploadした後、その一時directoryを破棄する。

Wi-Fi/BLE共存Peer testだけは、git管理外の`tests/.env`に次を設定する。

```dotenv
TEST_SERIAL_PORT_P4_PEER_HOST=/dev/ttyUSB2
TEST_WIFI_SSID=test-ap-ssid
TEST_WIFI_PASSWORD=test-ap-password
```

値は`build_config.toml`からtest sketchのcompile-time defineへ渡される。実行時に
SerialへSSID/passwordは出力しないが、Arduino CLIのverboseなcompile commandには
define値が表示され得るため、公開されてもよいテスト専用AP情報を使用する。
