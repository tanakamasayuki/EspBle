# 次回リリース前タスクリスト（1.3.0候補）

現在公開済みのversionは1.2.0です。Classic / dual-hostを含む機能追加を次回公開へ含める場合は1.3.0を
候補としますが、versionはrelease開始時にCHANGELOGと公開範囲を確認して確定します。

この文書は「未完了のrelease gate」を追跡する正本です。テストやexampleの総数は記録しません。
増減する集計値ではなく、必要な範囲をどの条件で通したかを結果として残します。

## 現在地

- BLE/GATT/Security/HID/MIDIの公開済み機能はv1.2.0までに提供済み。
- 無印ESP32向け同梱NimBLE hostはCentral / Peripheral両roleで実機回帰済み。
- Classic-only Bluedroid archive、SPP、HID Device/Host、dual-host HCI brokerは実装・実機検証済み。
  Classicは次回releaseの対象で、BLEとClassicの同時利用（dual-host）だけを実験扱いとして残す。
- Classic-onlyを実用範囲へ広げる次の対象はA2DP/AVRCP/HFP。Bluetooth media payloadまでをEspBleの
  責務とし、PCM処理とdevice I/OはPCMFlow等の独立libraryへ委ねる方針を確定した。
- A2DP Sink / Sourceのraw transport APIとESP32同士の実機media転送、AVRCP CT/TGのpassthroughと
  absolute volumeに加え、HFP Client/Audio GatewayのSLC、単一call control、mSBC raw SCO transport、
  process-wide role排他まで完了した。PCMFlowBluetoothのA2DP Sink adapterとSBC decoderは実装済みだが、
  実機probeで判明したdecoder reset問題と正式E2E追加を担当側へ依頼中である。
- 配布形式は次回Classic拡張ではNimBLE source / Classic `.a`のmixed distributionを意図的に維持する。
- dual-hostはcommand/ACL routing、bond/RPA、任意停止順、再attach、FIFO満杯、再登録とheap安定性に加え、BLE GATT接続中のHFP mSBC SCO双方向通信とA2DP/AVRCPまで検証済み。
- 数時間級dual-host soak、観測HCI commandのpolicy分類、不正HID report拒否、peer突然消失後の復旧、接続・pairing失敗後の復旧、lifecycle競合監査は完了。公開範囲は「次回releaseへ含める（保証は掲げず検証状態を書く）」として確定した。
- `CHANGELOG.md`のUnreleasedと利用者向けClassic文書は次回release内容として整理済み。文書整合と
  artifact監査も完了した。残るのはGate Cの最終回帰（機械時間）とGate Dの外部機器相互運用である。
- board / core matrix、release対象board全体でのexample compile、release操作、公開後確認は
  GitHub Actionsが行うため、release gateとしては追跡しない。

## Gate A: Classic / dual-hostの公開範囲

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 完了 | 数時間級dual-host soak | 20 run、1時間41分44秒。command競合／停止・再登録各100サイクル、panic・broker error・heap低下なし |
| 完了 | pairing / policy修正後のsoak再実行 | 10 run、1時間7分9秒（2026-08-14）。初回pairingから接続する条件で、heapは103 sample全て同値（min差12 byte）、`unknown=0 qfull=0 mismatch=0 busy=0`、FIFO投入数＝物理送信数。log: `tests/.soak/dual-host-20260814T071408Z` |
| 完了 | HCI command policy監査 | 接続後cleanupを含むinventoryを分類し、未知／別host opcodeのfail-closed policyとunit・実機回帰を追加 |
| 完了 | 不正HID report / peer消失 | null・上限超過reportを送信前に拒否し接続を維持。peer突然再起動後にbond済みBLEとClassic HIDを復旧 |
| 完了 | 接続・pairing失敗 | 誤passkey後にbondを残さず再pairing。HID非同期接続失敗後も暗号化LEを維持し、正しいClassic peerへ再接続 |
| 完了 | lifecycle競合監査 | SPP/HID callback targetを登録mutex下で取得し、解除後は取得済み参照が0になるまでstateを保持。clean実機lifecycle回帰成功 |
| 完了 | release scope決定 | **次回releaseへ含める。**build flagは設けず`EspBleClassic`を使うかどうかだけで決まる。exampleもBLE側と同じ範囲まで用意する。MIT OSSとして厳密なサポート保証は掲げず、代わりに機能ごとの「実機検証済み / 未検証 / 未実装」を文書で区別する。releaseまで未実装項目を減らす作業を続ける（[決定台帳](DECISIONS.ja.md)のスコープ6） |
| 完了 | 利用者向け文書 | README、Feature Matrix、example、制限が上記scopeと一致し、**未検証と未実装が読み手に区別できる。**「サポート」「保証」ではなく検証状態で書く |

Gate Aの詳細は[引き継ぎ](HANDOFF_ESP32_CLASSIC.ja.md)を正とします。controller-to-host ACL flow controlは
brokerが所有する形で実装・実機検証済みで、判断待ちではありません。

## Gate B: 生成物と互換性

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 完了 | Classic archive生成入口 | IDF/tag/toolchain検査、link check、symbol prefix、必須symbol検査をscript化 |
| 完了 | archive clean再現 | cleanなIDF v5.5.5 worktree・GCC 14.2.0から一時生成し、格納済みarchiveとbyte単位・SHA-256一致 |
| 完了 | Classic Audio archive | external codec A2DP/AVRCP、Voice over HCI / external codec HFPを有効化し、必須API link checkとclean再生成を完了 |
| 完了 | A2DP Sink transport | SBC codec設定、接続・stream状態、callback限定raw view、停止barrierを実装し、ESP32同士で実機転送 |
| 完了 | A2DP Source transport | 固定SBC endpoint、copy送信、MTU検査、`WouldBlock` retryを実装。通常回帰に加え20,000 packet連続転送を欠損なく完走 |
| 一部完了 | AVRCP CT/TG | passthrough、absolute volume、通知をA2DP併用で実機確認。metadata/play-status受信は外部Targetとの相互運用を残す |
| 一部完了 | HFP | Client/Audio GatewayのSLC、発信・着信・応答・終了、選択可能なmSBC/CVSD raw SCO transport、packet statistics、role排他を実機確認。外部機器相互運用を残す |
| 完了 | dual-host Audio基本回帰 | BLE GATT接続を維持してHFP mSBC SCO、A2DP SBC media、AVRCP Play / absolute volumeを実行し、各link中・切断後のGATT readとbroker異常0を確認 |
| 完了 | 他SoC非影響 | Arduino-ESP32 3.3.11のS3/C3/C6/H2/P4代表compileでClassic archive・無印ESP32 patchが非適用 |

archive手順は[Classic host archive再生成](CLASSIC_HOST_BUILD.ja.md)を正とします。
Audioのscopeと段階は[Classic Audio拡張計画](PLAN_ESP32_CLASSIC_AUDIO.ja.md)を正とします。

## Gate C: 自動回帰

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 未完了 | S3標準回帰 | `--clean`全Peer + unitを連続実行し、複数回でflaky failure、heap低下、task残留なし |
| 未完了 | 無印ESP32 NimBLE回帰 | Central / Peripheral両roleの`--clean`掃引が成功 |
| 完了 | Classic専用回帰 | clean buildでSPP、HID profile初期化、HID双方向report・SPP併用・再接続が成功 |
| 完了 | Classic-only build UX | 独自hostを自動選択し、公開Classic exampleから`build_opt.h`を除去。flagなしSPPを実機確認 |
| 完了 | dual-host回帰 | public address、RPA/bond、soak、HFP、A2DP/AVRCPが成功 |
| 未完了 | P4/C6 Hosted代表回帰 | Security非依存のrelease gateが成功。上流既知制限は再確認 |

具体的なcommandは[リリースチェックリスト](RELEASE_CHECKLIST.ja.md)を正とします。
release対象board全体でのexample compileはworkflowが行うため、gateには含めません（後述）。

## Gate D: 相互運用

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 未完了 | BLE HID Device | 外部Host 2種類以上でkeyboard/mouse/consumer、切断、bond再接続 |
| 未完了 | BLE HID Host | 市販BLE keyboardで入力、modifier、LED、切断release、bond再接続 |
| 未完了 | BLE Security / GATT | 外部実装でJust Works、passkey、scan、read/write、notify |
| 未完了 | Classic相互運用 | 次回releaseへClassicを含める場合、外部Classic HID/SPP/A2DP/AVRCP実装で対象profileを確認 |

結果には実施日、機器、OS、Bluetooth stack/versionを記録します。

## Gate E: 文書とmetadata

release操作そのものはこのgateに含めません（後述の「GitHub Actionsで行う作業」）。

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 完了 | CHANGELOG | Unreleasedへdual-host/Classic/RPA/lifecycle、無線設定、SPP Stream、HFP付随command、HID合成上限、制限、破壊的変更の有無を日英で記録 |
| 完了 | 文書整合 | README / STATUS / Feature Matrix / CLASSIC_VS_BLE（日英）、棚卸し、技術検証、試験計画をscopeと突き合わせた。scope未確定を前提にした記述と、dual-hostのbuild flag opt-inという古い記述を削除。試験計画に未記載だった5 suite（`classic_hid_profiles`、`classic_a2dp_sink_profile`、`classic_a2dp_media`、`dual_host_hfp`、`dual_host_a2dp`）を日英へ追加し、`tests/peer`の全suiteが両方の計画に載っている状態にした |
| 完了 | metadata | `library.properties`と`keywords.txt`をClassic分を含めて確認し、`EspBleClassicSppStream`とHFPの追加型・追加methodをkeywordsへ登録。version生成文書（BOARDS / COMPATIBILITY）はworkflowが作るためここでは扱わない |
| 完了 | artifact監査 | 追跡binaryは`src/esp32/libespble_bluedroid_classic.a`のみ。output / build / cache / logは未追跡。`git diff --check`は無警告。archiveのSHA-256が[archive再生成手順](CLASSIC_HOST_BUILD.ja.md)の記録と一致（`d64d3a40…9421`） |

## GitHub Actionsで行う作業（gateではない）

次はworkflowの実行結果であり、手元で先に通しても意味が変わりません。したがってrelease gateとして
追跡しません。release時またはrelease後に実行し、結果だけを記録します。

| 項目 | workflow | 備考 |
|---|---|---|
| board matrix | `board-matrix.yml` | 対象boardを再生成し、`docs/BOARDS.<version>.md`を確定 |
| core matrix | `core-matrix.yml` | Arduino-ESP32対応versionを再検証し、`docs/COMPATIBILITY.<version>.md`を確定 |
| 全example compile | `compile-examples.yml` | release対象board全体。手元ではesp32 / esp32s3のcompileをGate Cで確認する |
| release操作 | `release.yml` | bump preview、version、CHANGELOG、release branch、tag、GitHub release |
| 公開後確認 | — | Arduino Library Managerから取得し、最小exampleをcompile |

## Gate F: Classic exampleと入門ガイド

exampleに無い機能は「無い機能」として扱われる。BLEにあってClassicに無いexampleは、
実装済みのClassic機能を利用者から見えなくしている。したがってHIDはBLEと同じ一覧を揃える。

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 完了 | `Classic/HidGamepad` | **最優先。**BR/EDR HIDしか持たない旧世代ゲーム機が実際の接続先で、BLEでは代替できない。hat switchと39 fieldの作り方を示す |
| 完了 | `Classic/HidComposite` | keyboard + mouse + consumerを1台で合成し、profileごとにreport IDが分かれることを示す（BLE `Hid/CompositeKeyboardMouse`と対）。合成数はSDP recordの214 byte（descriptor + 文字列）で決まるため、gamepadを加えた構成は登録できない。上限とその理由をsketchとREADMEに書き、`begin()`が超過を拒否する |
| 完了 | `Classic/HidMouse` | mouse単独。`wheel()`、`press()`の加算、`buttons()`（BLE `Hid/Mouse`と対） |
| 完了 | `Classic/HidConsumerControl` | 音量・再生操作。car audioやTVが接続先（BLE `Hid/ConsumerControl`と対） |
| 完了 | `Classic/HidKeyboardNkro` | NKRO。6KRO制限が無いこと（BLE `Hid/KeyboardNkro`と対） |
| 見送り | `Classic/HidCustomDevice` | 既存の`Classic/HidVendorDevice`が任意Report Descriptorを示しており、内容が重複する。BLEの`Hid/CustomDevice`に対応するものとしてvendor版を案内すれば足りる |
| 完了 | `Hid/Gamepad`（BLE側） | `hidGamepad()`はどのexampleでも呼ばれていない。Classic版と同じ内容をBLEでも示す |
| 完了 | `Classic/SppClient` | `spp().connect()`。相手のservice recordからchannelを解決する流れはserver exampleから分からない |
| 完了 | `Classic/A2dpSource` | Sourceは実装済みだがexampleはSinkだけ |
| 完了 | `Classic/AvrcpController` | `sendKey()` / `requestMetadata()` / `requestPlayStatus()` / `setAbsoluteVolume()` / `registerVolumeNotifications()`。既存exampleはTarget側のみ |
| 完了 | `Hosted/WifiCoexistence` | P4/C6でWi-FiとBLEが同一transportを共有し、`EspBle::end()`がWi-Fiを残すこと。peer testはあるがexampleが無い |
| 完了 | `Classic/RadioSettings` | 送信電力・page timeout・暗号鍵の最小長。無線設定を公開したので、受理と反映が別であることを含めて示す |
| 完了 | `Classic/SppStream` | SPPをArduino `Stream`として使う。`Serial`向けcodeの移行先で、write 1回が1 packet・送信queueが有限という差を示す |
| 完了 | Classic example全数のREADME | READMEが無かった9本（Inquiry、SppServer、SppPairing、HidVendor×2、A2dpSink×2、Hfp×2）へ両言語のREADMEを追加。「どちらの無線がどの相手に届くか」を各READMEに書いた |
| 完了 | `Gap/MultiConnection` | 複数同時接続（上限3）。`AutoReconnectClient`は1接続の再接続のみ |
| 完了 | `docs/GUIDE_CLASSIC_BASICS.ja.md` / `.md` | Classicの概念とAPI境界の入門。`GUIDE_BLE_BASICS`と対にする。日英とも作成した（BLEと別の通信モデル、起動と可視性、無線設定、inquiry、SPP、security、HID、A2DP/AVRCP、HFP、BLEとの同時利用） |
| 完了 | BLEとClassicの選び方 | [CLASSIC_VS_BLE.ja.md](CLASSIC_VS_BLE.ja.md)。どちらを選ぶか、Classicの用途が実質SPP・音声・旧世代HIDの3つであること、HIDとSPPの同時利用が可能であること、両方にある機能（HID / Security / 探索 / データ転送）の差を記述。docs索引とREADME.ja.mdから参照 |
| 完了 | BLEとClassicの違いの周知 | 上記文書と`examples/README`両言語、Feature Matrix両言語、各Classic exampleのREADMEへ反映し、英語版`CLASSIC_VS_BLE.md`を作成して`README.md`（英語）とdocs索引から参照した |

姉妹libraryのガイドをそのまま持ち込まない。次の点はEspBleと異なる。

- `classic()` namespaceは無い。`EspBleClassic`が独立classで、profileは`spp()` / `hidKeyboard()` /
  `a2dpSink()` / `avrcp()` / `hfpClient()`のように直下にある。
- capability照会API（`profileSupport()`）は無い。
- Classic HIDは有効。姉妹libraryは「Core buildで無効」と書いているが、EspBleは独自archiveで有効化している。
- A2DP/HFPはexternal codecで、EspBleが扱うのはencode済みpayloadとraw SCOである。復号PCMは扱わない。
- AVRCPはController/Targetを1つの`avrcp()`が持ち、別objectではない。
- SPPのStream adapterは`EspBleClassicSppStream`として実装済み。ただし姉妹libraryの
  `EspBluedroidSppSerial`とは違い`esp_spp_vfs_register()`を使わず、sessionを借りるだけである。

## 未実装項目の削減（release前）

release前に未実装を減らす方針（[決定台帳](DECISIONS.ja.md)のスコープ6）で、次を実装・実機検証した。
判断の記録は[Classic機能の棚卸し](CLASSIC_FEATURE_INVENTORY.ja.md)の優先度欄が正本である。

| 項目 | 状態 | 備考 |
|---|---|---|
| 無線・link設定（送信電力・page timeout・暗号鍵最小長） | 完了 | `classic_radio_settings`。page timeoutは所要時間で反映を確認 |
| SPPのArduino `Stream` adapter | 完了 | `classic_spp_stream`。VFSは採用しない |
| HFP Clientのoperator名・subscriber番号・memory dial・NREC・Apple拡張 | 完了 | `classic_hfp_client`へ追加 |
| HFP AGのin-band ring tone | 完了 | 同上。鳴らす側の取り違えは実害があるため対応した |
| HID合成の上限検査 | 完了 | SDP record 214 byteを`begin()`が検査。`classic_hid_gamepad`を新設 |
| 見送り: RSSI（接続後）・QoS・AFH・ACL packet type・EIR・VFS・CHLD/BTRH | 判断済み | 実用場面が無い、値の意味が薄い、または検証相手が無い。理由は棚卸しに記載 |
| 残: HID Hostの複数device同時接続 | release後 | 公開signatureがdevice単位idを取る形へ変わるため |
| 残: A2DP Sourceの追加endpoint・SBC以外のcodec | release後 | EspBleはencode/decodeを持たない方針 |
| 残: AVRCP TGのmetadata / play status送信 | 不可 | v5.5.5の公開TG APIに送信手段が無い |

## 残作業の規模（概算）

行数は作業量ではないため、性質ごとに分けて見積もる。実装と実機検証は概ね終わっており、
残りは**最終回帰、文書整合、外部機器との相互運用**である。

| 区分 | 残り | 性質 | 目安 |
|---|---|---|---|
| 実装・実機検証（Gate A・B の機能行） | 完了 | — | Classic / dual-hostの機能検証とscope決定はいずれも完了 |
| Gate F: example | 完了（15本） | 執筆。既存exampleの構成（sketch + README両言語 + `sketch.yaml`）に沿う | 12本＋`Classic/RadioSettings`＋`Classic/SppStream`。あわせてREADMEが無かったClassic example 9本へ両言語のREADMEを追加し、Classic exampleは全数がREADME付きになった。全数がesp32 / esp32s3でcompile通過 |
| Gate F: Classic入門ガイド | 完了（ja / en） | 執筆 | — |
| Gate F: 違いの周知 | 完了 | 既存文書への反映と英語版 | — |
| Gate E: CHANGELOGとmetadata | 完了 | 執筆と点検 | — |
| Gate E: 文書整合とartifact監査 | 2 | 執筆と点検 | 1作業日 |
| Gate C: 最終回帰 | 4 | ほぼ機械時間。code freeze後にやり直しが必要 | S3全Peer cleanを複数回で数時間×回数、無印ESP32掃引、Classic、P4/C6 |
| Gate D＋Gate Bの一部完了2件 | 6 | **外部機器が必要でblockしている。**市販BLE keyboard、外部Host 2種以上、外部Classic HID/SPP/A2DP/AVRCP機器、外部HFP機器 | 機材の準備待ち。準備後は各1作業単位 |

したがってblockしているのは**外部機器**（Gate D）だけである。手元の2台構成では代替できない。
board / core matrix、release対象board全体でのexample compile、release操作、公開後確認は
workflowの仕事であり、手元で前倒ししても結果が変わらないためgateから外した。

## 推奨実行順

1. AVRCP metadata/play-statusの外部Target相互運用と、HFPの外部機器相互運用を行う（機材待ち）。
2. [PCMFlowBluetooth修正依頼](REQUEST_PCMFLOWBLUETOOTH_A2DP_VALIDATION.ja.md)のreset修正と正式E2E結果を受け取り、
   PCMFlowDevice等を接続した実出力と長時間負荷へ進む。
3. コードfreeze後にGate Cのclean全回帰を複数回行う。
4. Gate Eの文書整合とartifact監査を締める。
5. Gate Dを並行実施する。
6. release時にworkflowを回し、生成文書と公開後の取得を確認する。

コード変更後に全回帰を先行してもfreeze後に再実行が必要です。長時間作業の結果は、合否だけでなく
条件とlog保存先を引き継ぎ文書・技術検証へ記録します。
