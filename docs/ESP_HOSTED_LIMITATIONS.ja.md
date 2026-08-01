# ESP32-P4 / ESP-Hostedの既知制限

この文書はArduino-ESP32 3.3.11、ESP-Hosted Host/Slave 2.12.11、
ESP32-P4 + ESP32-C6（SDIO）で実機確認した制限を記録する。
compile、scan、接続、GATT read/write、notify/indicate、MTU交換に加え、Wi-Fi/BLEの
同時利用と共有transportの最終所有者までの維持は動作する。

## Wi-Fi/BLE共存で確認済みの範囲

P4でWi-Fiを先に開始してDHCPでIP addressを取得し、その後BLEを開始する順序を
`wifi_ble_coexistence` Peer testで確認した。Wi-Fi接続中にS3 Peerとのscan、接続、
GATT read/write、subscribe、notificationが成功する。接続中に`EspBle::end()`を
呼んでもWi-FiとHosted transportは有効なままであり、最後に`WiFi.STA.end()`を
呼ぶとHosted transportが解放された。

したがってEspBleの責務は`hostedInitBLE()` / `hostedDeinitBLE()`によるBLE所有分の
lifecycle管理までとし、Wi-Fi接続・認証情報・Wi-Fi所有分の終了はアプリとArduino Coreの
Wi-Fi APIの責務とする。この結果は1回の開始・終了についてのもので、後述する完全な
deinit/initの反復制限を解消するものではない。

## LE Secure Connectionsとbonding

LE Secure Connectionsを有効にしたP4/S3 Peer testは、初回pairing中に必ず
DHKey check failure (`0x0b`)となる。P4 centralのbackend statusは`0x50b`、
S3 peripheralは`0x40b`だった。S3/S3では同じtestが成功するため、EspBleの共通
Security処理やS3 peripheralだけでは再現しない。

C6 Slaveを2.3.2からHostと同じ2.12.11へ更新しても解消しなかった。SMP境界を
instrumentした結果、pairing request/response、双方の64 byte公開鍵、Random、address、
IO capability、DHKey Checkは送信前と受信後で一致した。一方、同じ鍵pairから計算した
P-256 ECDH共有鍵だけがP4とS3で異なった。採取したprivate/public keyを独立計算すると、
公開鍵生成は両側とも正しく、S3の共有鍵だけが期待値と一致した。したがってHostedの
ACL/HCI転送ではなく、Arduino-ESP32 3.3.11同梱ESP-IDF 5.5.5のP4 TinyCrypt/ECC
hardware経路が原因である。

ESP-IDF `release/v5.5`ではcommit
[`9fd7cb7`](https://github.com/espressif/esp-idf/commit/9fd7cb7e606a06111e1b14be7f4e00d77d9cf3dd)
（2026-07-16）で修正済みである。修正はECC hardwareへcanonical scalarを渡し、
software ladder用に正規化された1 bit長いscalarをhardwareへ渡さないようにするほか、
TinyCrypt native wordとECC peripheralのlittle-endian byte表現を変換する。
Core 3.3.11のprebuilt libraryはそれ以前のIDF commit `129cd0d2`から作られている。

診断用にP4のcanonical private scalarとaligned little-endian pointを直接
`esp_tinycrypt_calc_ecc_mult()`へ渡すと、P4/S3の共有鍵、f5 MacKey、双方のf6 Checkが
一致し、SC pairing、bond、暗号化GATT read/writeが成功した。これは原因と上流修正の
有効性を確認するためのinstrumentationであり、内部symbolへの依存と暗号実装の重複を
持ち込むためEspBleの製品workaroundにはしない。

次の回避も採用できないことを実機で確認した。

- P4だけSecure Connectionsを無効にすると、初回のLegacy pairingと暗号化GATTは
  成功する。ただしLegacy pairingは暗号学的に弱いため、ライブラリが暗黙に
  downgradeしてはならない。
- bond再接続時はP4側のSecurity eventが欠落する。接続状態をpollすると暗号化済み
  であることは検出できるが、`bonded=0`、`key_size=0`の不完全な状態しか得られない。
  EspBle側でbond状態や鍵長を推測して成功扱いすることはできない。

したがって、Core 3.3.11のP4/C6 Hosted構成ではEspBleのSecurity、bonding、および
それを前提にするHID利用を対応済みと扱わない。Securityを必要とする製品用途では、
Arduino Coreが上記ESP-IDF修正を取り込むのを待つか、内蔵BLE Controllerを持つ
S3/C3/C6/H2を使用する。Arduino Core向けの手動投稿用issue案は
[ESP_HOSTED_SC_ISSUE.md](ESP_HOSTED_SC_ISSUE.md)に保存している。

## `end()`後の再`begin()`

Arduino-ESP32 3.3.11同梱のESP-Hosted 2.12.11では、完全な
deinit/initを繰り返すとSDIO mempoolの確保に失敗し、次のassertで再起動することがある。

```text
HS_MP: mempool create failed: no mem
assert failed: sdio_mempool_create sdio_drv.c:255 (buf_mp_g)
```

250 msの待機を追加してもlifecycle suiteは`7 passed / 1 failed`で同じassertを再現した。
一方、Hosted transportとC6 Controllerを`end()`後も保持する切り分けではsuiteが
`8 passed`となり、問題が完全なHosted deinit/init経路にあることを確認した。

保持方式はC6、SDIO、heap、電力resourceを解放せず、`EspBle::end()`の契約と
Wi-Fi共有時の所有権を変える。そのためEspBleへ暗黙の回避として実装しない。
Core 3.3.11では次の制約で運用する。

- P4/C6 Hosted構成では、通常は起動後に`begin()`を1回だけ実行する。
- `end()`後にBLEを再開する必要がある場合は、P4を再起動する。
- Wi-FiがHosted transportを保持している場合も、BLE Controllerだけの再初期化を
  未検証のため、繰り返し動作を保証しない。

ESP-Hosted-MCUでは2.12.11の後にcommit `d0f4646`で、各init/deinit cycleに
shared channel mempoolが漏れる問題が修正され、2.12.12としてreleaseされている。
今回のassertと整合する修正だが、Arduino-ESP32 3.3.11は2.12.11を同梱するため
実機では未確認である。Arduino Coreが2.12.12以降へ更新された時点で同じ
`lifecycle_stress`を再実行し、この制約を再評価する。

## 責務

これらはEspBleの公開GAP/GATT APIではなく、Arduino Core同梱のTinyCrypt/ECC実装または
Hosted transport lifecycleの制限である。EspBleは安全で意味を保てる回避だけを実装し、
暗号実装、firmware更新、Hosted transportの修正、Arduino Coreへのcomponent取り込みは
各上流projectの責務とする。
EspBle側は再現test、検証version、制約、公式更新手順を維持する。
