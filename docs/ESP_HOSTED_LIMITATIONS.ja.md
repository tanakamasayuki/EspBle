# ESP32-P4 / ESP-Hostedの既知制限

この文書はArduino-ESP32 3.3.11、ESP-Hosted Host/Slave 2.12.11、
ESP32-P4 + ESP32-C6（SDIO）で実機確認した制限を記録する。
compile、scan、接続、GATT read/write、notify/indicate、MTU交換は動作する。

## LE Secure Connectionsとbonding

LE Secure Connectionsを有効にしたP4/S3 Peer testは、初回pairing中に必ず
DHKey check failure (`0x0b`)となる。P4 centralのbackend statusは`0x50b`、
S3 peripheralは`0x40b`だった。S3/S3では同じtestが成功するため、EspBleの共通
Security処理やS3 peripheralだけでは再現しない。

C6 Slaveを2.3.2からHostと同じ2.12.11へ更新しても解消しなかった。さらに、
ESP-Hosted-MCU 2.12.11相当commitから2.12.12までのHosted HCI host/slave実装に
この経路の変更はない。

次の回避も採用できないことを実機で確認した。

- P4だけSecure Connectionsを無効にすると、初回のLegacy pairingと暗号化GATTは
  成功する。ただしLegacy pairingは暗号学的に弱いため、ライブラリが暗黙に
  downgradeしてはならない。
- bond再接続時はP4側のSecurity eventが欠落する。接続状態をpollすると暗号化済み
  であることは検出できるが、`bonded=0`、`key_size=0`の不完全な状態しか得られない。
  EspBle側でbond状態や鍵長を推測して成功扱いすることはできない。

したがって、P4/C6 Hosted構成では現時点でEspBleのSecurity、bonding、およびそれを
前提にするHID利用を対応済みと扱わない。Securityを必要とする製品用途では、上流の
ESP-Hosted修正を待つか、内蔵BLE Controllerを持つS3/C3/C6/H2を使用する。

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

これらはEspBleの公開GAP/GATT APIではなく、Hosted HCIまたはtransport lifecycleの
制限である。EspBleは安全で意味を保てる回避だけを実装し、firmware更新、Hosted
transportの修正、Arduino Coreへのcomponent取り込みは各上流projectの責務とする。
EspBle側は再現test、検証version、制約、公式更新手順を維持する。
