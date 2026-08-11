# 無印ESP32 NimBLE / Classic共存 引き継ぎ

2026-08-11時点の実装、検証済み範囲、未完了事項、作業再開手順をまとめます。新しく作業する人は
この文書、[Classic設計・検証記録](PLAN_ESP32_CLASSIC.ja.md)、
[技術検証](TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md)の順に読んでください。

## 現在の結論

無印ESP32では、独自buildしたClassic-only Bluedroid hostとEspBle同梱NimBLE hostを、単一BTDM
controllerへ同時接続できます。Classic hostはcore内蔵archiveではなく、SPP、HID Device、HID Host、
SMPを有効にした名前空間化済み`libespble_bluedroid_classic.a`を使います。

dual-hostは`ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL`によるopt-inです。通常buildはsingle-hostのままで、
無印ESP32以外はcore同梱NimBLE経路を変更しません。現段階は技術検証済みの実験機能であり、一般対応へ
昇格していません。

Classic-only A2DP Sink / Sourceのraw transportとAVRCP CT/TGの基本制御は公開APIと実機転送まで完了しました。
次はHFP Client / AGへ広げます。EspBleはBluetooth profile、codec negotiation、encode済みmedia/SCO payloadの受け渡し
までを担当し、PCM処理やdevice I/Oは担当しません。詳細は
[Classic Audio拡張計画](PLAN_ESP32_CLASSIC_AUDIO.ja.md)を正本とします。

## 完了済み

| 領域 | 状態 |
|---|---|
| Classic host独自build | IDF v5.5.5 / GCC 14.2.0、controller/BLE無効、SPP/HID Device/HID Host/SMP、A2DP/AVRCP、HFP Client/AG external codec有効 |
| archive clean再現 | cleanなv5.5.5 worktreeから一時生成し、格納済み`.a`とbyte単位・SHA-256一致 |
| symbol分離 | archiveのglobal defined symbolを`espble_bd_`へ名前空間化。core Bluedroidと衝突なし |
| Classic排他モード | SPP、HID Device/Host、双方向report、切断、再初期化、再接続を実機確認 |
| A2DP Sink transport | 接続、SBC設定、stream状態、raw media view、buffer解放、callback停止を公開API化。ESP32同士でexternal-codec mediaを実機受信 |
| A2DP Source transport | 固定SBC endpoint、接続、start/suspend、copy送信、MTU検査、`WouldBlock` retryを公開API化。100 packetを欠損なく実機送信 |
| AVRCP CT/TG | 接続、remote feature、passthrough送受信、metadata/play-status要求と応答event、absolute volumeとone-shot通知を公開API化。A2DP併用でPlayと音量変更を実機確認 |
| HFP Client | SLC、発信/応答/終了等のcall control、call/volume/AT event、CVSD/mSBC raw SCO送受信、bad-frame、packet statisticsを公開API化。ESP32 AG probeとmSBC双方向転送を確認 |
| HFP Audio Gateway | CIND/COPS/CNUM/CLCC自動応答、単一call model、application command event、CVSD/mSBC raw SCO送受信を公開API化。公開Clientとの発信・mSBC往復とClient/AG process-wide排他を確認 |
| dual-host HCI routing | Command Complete/Status、LE/BR-EDR handle、ACL、切断、Completed Packetsをrouting |
| command scheduler | broker所有16 packet FIFO、controller credit、opcode照合、1 response command in-flight |
| controller-wide policy | General/Page 2/LE event mask union、再attach時Resetとflow-control設定の仮想完了 |
| lifecycle | controller停止責任をbrokerへ委譲。任意停止順、再attach、両destructor順、再起動に加え、SPP/HID callback解除の参照寿命barrierを確認 |
| Security / privacy | Classic接続中のBLE pairing、bond復元、暗号化GATT、host-based RPA rotationと再起動復元 |
| 負荷 | GATT/Classic ACL反復、command同時発行、FIFO満杯・超過拒否、数時間級再登録soak、heap非減少を確認 |
| 異常report / peer消失 | null・1025 byte HID reportを接続維持のまま拒否。peer突然再起動後にbond済みBLEとClassic HIDを再接続 |
| 接続・pairing失敗 | 誤passkey後にbondを残さず再pairing。HID Hostの非同期接続失敗通知後もBLEを維持し、正しいpeerへ再接続 |
| 他SoC分離 | ESP32-S3 buildでClassic archive非リンク、無印ESP32専用privacy patch非適用を確認 |

詳細な数値と失敗から得た知見は[技術検証](TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md)にあります。

## 重要な設計境界

- ClassicがBTDM controllerを先に起動し、NimBLEはcontrollerを再初期化せずhostだけattachする。
- 最後のlogical hostが外れたときだけbrokerがcontrollerを停止する。
- HCI commandをhostから物理VHCIへ直接流さず、broker FIFOと単一transaction ownershipを通す。
- event種別だけで配送せず、command owner、connection handle、LE Meta / BR-EDR event semanticsを使う。
- Classicが要求するcontroller-to-host ACL flow controlは現在物理controller上で無効化している。
- test-only backpressure holdは`ESPBLE_HCI_BACKPRESSURE_TEST`時だけ存在し、通常buildには入らない。
- `.a`のglobal defined symbolだけをprefixし、platform依存のundefined symbolはArduino coreから解決する。

## 未完了・優先順位

### P0: Classic-only Audio

1. **完了:** A2DP/AVRCP/HFPをexternal codec / Voice over HCI構成でarchiveへ追加し、生成scriptのlink checkを拡張した。
2. **完了:** A2DP Sinkの接続、SBC設定、stream状態、encoded media callback、所有権と停止barrierを公開API化した。
3. **完了:** Classic-only ESP32同士で接続、codec設定、5 packet受信、suspend、切断を実機確認した。
4. **完了:** A2DP Sourceのencoded media copy送信と`WouldBlock` backpressureを公開API化し、100 packetを実機確認した。
5. **基本操作完了:** AVRCP CT/TGのpassthroughとabsolute volumeをA2DP接続上で実機確認した。公開TG APIに
   metadata / play-status応答送信がないため、Controller側応答eventは外部Targetとの相互運用確認を残す。
6. **完了:** HFP Clientのcontrol / Voice over HCI transportを実装し、mSBCで実機確認した。
7. **基本完了:** HFP AGを公開API化し、Client/AGのruntime排他、発信、着信、応答、終了、call active、
   mSBC往復を実機確認した。外部HFP機器との相互運用は残す。
8. codec/PCM/device処理はEspBleへ入れず、`../PCMFlowBluetooth/SPEC.ja.md`を契約として
   独立libraryを並行実装する。PCMFlow coreの既存`PCMSource`/`PCMSink`を再利用する。

HFPの公開境界は[Classic Audio拡張計画](PLAN_ESP32_CLASSIC_AUDIO.ja.md#hfp実装前調査)まで調査済み。
受信bufferはcallback復帰後にEspBleが解放、送信bufferはAPI成功時にBluedroidが消費する。HFP送信には
A2DPのような同期的`WouldBlock`がないため、受理結果と後続packet discard statisticsを混同しないこと。
実機ではmSBC 57-byte送信が受信側で58/60 byteのpadding付きviewになり、bad-frameも60 byteで届いた。
PCMFlowBluetoothへは`badFrame`とraw lengthを失わず渡し、decoder側で57 byte frameを切り出す。

### 完了: dual-host実験機能の信頼性

完了。失敗復旧、異常入力、peer消失、callback解除時の参照寿命を実装・監査し、clean実機回帰まで通した。

### P1: 一般対応・upstream品質

1. brokerがincoming ACL処理完了を一元管理し、controller-to-host flow controlを無効化せず両hostへcreditを返せる形を設計する。
2. HCI parser、transaction、handle table、credit分配へfuzz / fault injectionを追加する。
3. Arduino依存を外したdirection-aware router componentの境界を定め、ESP-IDF upstreamへ出せる差分へ縮小する。
4. Classic dual-hostの利用者向けAPI、build flag、対応profile、制限、exampleを正式サポート範囲として確定する。

### P2: 配布・保守

1. NimBLE source / Classic `.a`のmixed distributionを維持し、理由と生成手順を利用者・保守者文書へ明記する。
2. Arduino-ESP32更新時のIDF/toolchain ABI matrixとarchive再生成gateをCIまたはrelease手順へ組み込む。
3. 外部Classic HID Host/Deviceとの相互運用を追加する。

## 既知の落とし穴

- `Write Local Name`をsoak刺激として反復するとcontrollerのNVDSが`nvds.c:400`でassertする。永続設定を負荷生成に使わず、scan mode切替を使う。
- logical hostごとの`can_send()` slot予約はBluedroidの先読みと両立せず、NimBLEをstarveさせるため採用しない。
- Classicのcontroller-to-host flow controlをそのまま有効化すると、NimBLEへroutingしたLE ACLのcreditをClassicが返せず、共有bufferが枯渇する。
- NimBLE停止を別taskから直接行うとNPL event queueと競合する。停止開始はNimBLE host task自身へ要求する。
- RPA更新はadvertising / scanをpreemptする。元の設定と有限deadlineを保持して再開しないと、処理が停止または期限延長する。
- Classic再attach時の物理HCI Resetは既存LE接続を破壊するため、Classicだけへ仮想Command Completeを返す。
- Bluedroid公開HID Host APIにはpage中の接続試行を取り消す手段がない。未接続peerへの任意timeoutを
  `esp_bt_hid_host_disconnect()`で実装すると内部のconnecting状態が残り、次の接続を拒否するため採用しない。
  APIはbackendの最終`OPEN`失敗を`onConnectionFailed()`で非同期通知する。

## 作業環境と再開方法

- Arduino-ESP32: 3.3.11
- Classic archive: ESP-IDF v5.5.5、xtensa-esp32 GCC 14.2.0
- 実機: 無印ESP32 2台、pytest profile `esp32_peer_host` / `esp32_peer_device`
- Python環境: `tests/`で`uv run --env-file .env ...`

代表回帰:

```sh
cd tests
uv run --env-file .env pytest -s peer/dual_host_smoke/ \
  --profile esp32_peer_host --peer-profile device:esp32_peer_device

uv run --env-file .env pytest -s peer/dual_host_rpa/ \
  --profile esp32_peer_host --peer-profile device:esp32_peer_device

uv run --env-file .env pytest -s peer/classic_a2dp_media/ \
  --profile esp32_peer_host --peer-profile device:esp32_peer_device

uv run pytest -q unit
```

長時間soakはrepository rootから次で開始します。

```sh
ESPBLE_DUAL_SOAK_RUNS=20 tools/run_dual_host_soak.sh
```

既定では各runでcommand競合100サイクル、restart 100サイクルを実行し、最初のrunだけclean buildします。
ログは`tests/.soak/`へ保存し、1 runでも失敗すればその時点で停止します。実行前に他のprocessが対象boardを
使っていないことと、`tests/.env`のprofile/port設定を確認します。

archive再生成は[Classic host archive再生成](CLASSIC_HOST_BUILD.ja.md)を参照してください。

2026-08-11に`v5.5.5` tagの独立cloneへ全submoduleをcheckoutし、GCC 14.2.0でAudio設定を含む一時出力へ
clean buildした。生成物は格納済みarchiveと`cmp`で一致し、SHA-256は双方
`d64d3a40a3f598e206c5aaf798e9d8fda5c867b632224ed72a616b1221089421`だった。
固定Kconfig、必須prefixed symbol、unprefixed global defined symbolがないことも確認した。

## 完了判定と記録

soak完了時は、run数、開始/終了時刻、総経過時間、各runのpytest結果、heap初回/最終値、brokerの
enqueue/send/qmax/qfull/mismatch/busy/unknown、panic/backtraceの有無を
[技術検証](TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md)へ追記します。失敗時は試験を緩めず、最初の失敗logを
保存して再現条件を最小化します。作業終了時は[次回リリース計画](PLAN_RELEASE_NEXT.ja.md)とSTATUSも
同じ結論へ更新します。

## 2026-08-09 長時間soak結果

`ESPBLE_DUAL_SOAK_RUNS=20 tools/run_dual_host_soak.sh`を実行し、全runが成功しました。期間は
2026-08-09 14:29:26〜16:11:10 JST、総経過時間は1時間41分44秒です。各runはcommand競合100サイクルと
停止・再登録100サイクルを行い、暗号化GATT、Classic HID双方向、FIFO backpressure、再attach、任意順停止、
destructor後の再起動を含みます。

- 全runの最終diagnosticsでcommand enqueue数と物理send数が一致
- `qfull=0 mismatch=0 busy=0 unknown=0`、最大通常queue深度5
- 各run内のfree heapとlargest blockに減少なし。1 runで開始後にfree heapが8 byte増え、その後は一定
- panic、watchdog、backtrace、再接続失敗なし

logは`tests/.soak/dual-host-20260809T052926Z/`に保存しています。`.soak/`はgit管理外なので、長期保存が
必要な場合はrelease artifactまたは外部保存先へ退避してください。
