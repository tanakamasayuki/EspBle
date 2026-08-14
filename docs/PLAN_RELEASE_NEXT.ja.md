# 次回リリース前タスクリスト（1.3.0候補）

現在公開済みのversionは1.2.0です。Classic / dual-hostを含む機能追加を次回公開へ含める場合は1.3.0を
候補としますが、versionはrelease開始時にCHANGELOGと公開範囲を確認して確定します。

この文書は「未完了のrelease gate」を追跡する正本です。テストやexampleの総数は記録しません。
増減する集計値ではなく、必要な範囲をどの条件で通したかを結果として残します。

## 現在地

- BLE/GATT/Security/HID/MIDIの公開済み機能はv1.2.0までに提供済み。
- 無印ESP32向け同梱NimBLE hostはCentral / Peripheral両roleで実機回帰済み。
- Classic-only Bluedroid archive、SPP、HID Device/Host、dual-host HCI brokerは実装・実機検証済みだが実験扱い。
- Classic-onlyを実用範囲へ広げる次の対象はA2DP/AVRCP/HFP。Bluetooth media payloadまでをEspBleの
  責務とし、PCM処理とdevice I/OはPCMFlow等の独立libraryへ委ねる方針を確定した。
- A2DP Sink / Sourceのraw transport APIとESP32同士の実機media転送、AVRCP CT/TGのpassthroughと
  absolute volumeに加え、HFP Client/Audio GatewayのSLC、単一call control、mSBC raw SCO transport、
  process-wide role排他まで完了した。PCMFlowBluetoothのA2DP Sink adapterとSBC decoderは実装済みだが、
  実機probeで判明したdecoder reset問題と正式E2E追加を担当側へ依頼中である。
- 配布形式は次回Classic拡張ではNimBLE source / Classic `.a`のmixed distributionを意図的に維持する。
- dual-hostはcommand/ACL routing、bond/RPA、任意停止順、再attach、FIFO満杯、再登録とheap安定性に加え、BLE GATT接続中のHFP mSBC SCO双方向通信とA2DP/AVRCPまで検証済み。
- 数時間級dual-host soak、観測HCI commandのpolicy分類、不正HID report拒否、peer突然消失後の復旧、接続・pairing失敗後の復旧、lifecycle競合監査は完了。公開サポート範囲の確定が未完了。
- `CHANGELOG.md`のUnreleasedと利用者向けClassic文書は、次回release内容として未整理。

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
| 未完了 | 利用者向け文書 | README、Feature Matrix、example、制限が上記scopeと一致し、**未検証と未実装が読み手に区別できる。**「サポート」「保証」ではなく検証状態で書く |

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
| 未完了 | board matrix | workflowで対象boardを再生成し、次回versionのBOARDS文書を確定 |
| 未完了 | core matrix | Arduino-ESP32対応versionを再検証し、次回versionのCOMPATIBILITY文書を確定 |
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
| 未完了 | 全example compile | release対象boardでworkflow成功、Classic exampleは無印ESP32条件を確認 |

具体的なcommandは[リリースチェックリスト](RELEASE_CHECKLIST.ja.md)を正とします。

## Gate D: 相互運用

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 未完了 | BLE HID Device | 外部Host 2種類以上でkeyboard/mouse/consumer、切断、bond再接続 |
| 未完了 | BLE HID Host | 市販BLE keyboardで入力、modifier、LED、切断release、bond再接続 |
| 未完了 | BLE Security / GATT | 外部実装でJust Works、passkey、scan、read/write、notify |
| 未完了 | Classic相互運用 | 次回releaseへClassicを含める場合、外部Classic HID/SPP/A2DP/AVRCP実装で対象profileを確認 |

結果には実施日、機器、OS、Bluetooth stack/versionを記録します。

## Gate E: 文書・metadata・公開

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 未完了 | CHANGELOG | Unreleasedへdual-host/Classic/RPA/lifecycle、制限、破壊的変更の有無を日英で記録 |
| 未完了 | 文書整合 | README、STATUS、Feature Matrix、API/spec、example、テスト計画がrelease scopeと一致 |
| 未完了 | metadata | `library.properties`、`keywords.txt`、version生成文書を最終確認 |
| 未完了 | artifact監査 | build/cache/local profile、意図しないbinary、dirty submoduleなし。archive provenance確認 |
| 未完了 | release | bump preview、release workflow、branch/tag/GitHub release作成 |
| 未完了 | 公開後確認 | Arduino Library Managerから取得し、最小exampleをcompile |

## Gate F: Classic exampleと入門ガイド

exampleに無い機能は「無い機能」として扱われる。BLEにあってClassicに無いexampleは、
実装済みのClassic機能を利用者から見えなくしている。したがってHIDはBLEと同じ一覧を揃える。

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 未完了 | `Classic/HidGamepad` | **最優先。**BR/EDR HIDしか持たない旧世代ゲーム機が実際の接続先で、BLEでは代替できない。hat switchと39 fieldの作り方を示す |
| 未完了 | `Classic/HidComposite` | keyboard + mouse + consumer + system + gamepadを1台で合成し、profileごとにreport IDが分かれることを示す（BLE `Hid/CompositeKeyboardMouse`と対） |
| 未完了 | `Classic/HidMouse` | mouse単独。`wheel()`、`press()`の加算、`buttons()`（BLE `Hid/Mouse`と対） |
| 未完了 | `Classic/HidConsumerControl` | 音量・再生操作。car audioやTVが接続先（BLE `Hid/ConsumerControl`と対） |
| 未完了 | `Classic/HidKeyboardNkro` | NKRO。6KRO制限が無いこと（BLE `Hid/KeyboardNkro`と対） |
| 未完了 | `Classic/HidCustomDevice` | 任意Report Descriptor。既存`HidVendorDevice`と重複するため、統合するかBLEと同名へ揃えるかを着手時に決める |
| 未完了 | `Hid/Gamepad`（BLE側） | `hidGamepad()`はどのexampleでも呼ばれていない。Classic版と同じ内容をBLEでも示す |
| 未完了 | `Classic/SppClient` | `spp().connect()`。相手のservice recordからchannelを解決する流れはserver exampleから分からない |
| 未完了 | `Classic/A2dpSource` | Sourceは実装済みだがexampleはSinkだけ |
| 未完了 | `Classic/AvrcpController` | `sendKey()` / `requestMetadata()` / `requestPlayStatus()` / `setAbsoluteVolume()` / `registerVolumeNotifications()`。既存exampleはTarget側のみ |
| 未完了 | `Hosted/WifiCoexistence` | P4/C6でWi-FiとBLEが同一transportを共有し、`EspBle::end()`がWi-Fiを残すこと。peer testはあるがexampleが無い |
| 未完了 | `Gap/MultiConnection` | 複数同時接続（上限3）。`AutoReconnectClient`は1接続の再接続のみ |
| 未完了 | `docs/GUIDE_CLASSIC_BASICS.ja.md` / `.md` | Classicの概念とAPI境界の入門。`GUIDE_BLE_BASICS`と対にする。姉妹library`../EspBleBluedroid/docs/GUIDE_CLASSIC_BASICS.ja.md`の構成（BLEと別の通信モデル、Inquiry、SPP、Security、A2DP/AVRCP、HFP、同時利用）を土台に、EspBleのAPI名と範囲へ書き換える |
| 完了 | BLEとClassicの選び方 | [CLASSIC_VS_BLE.ja.md](CLASSIC_VS_BLE.ja.md)。どちらを選ぶか、Classicの用途が実質SPP・音声・旧世代HIDの3つであること、HIDとSPPの同時利用が可能であること、両方にある機能（HID / Security / 探索 / データ転送）の差を記述。docs索引とREADME.ja.mdから参照 |
| 一部完了 | BLEとClassicの違いの周知 | 上記文書は作成済み。残りは英語版`CLASSIC_VS_BLE.md`、`examples/README`両言語、`README.md`（英語）、Feature Matrix、各Classic exampleの冒頭コメントへの反映 |

姉妹libraryのガイドをそのまま持ち込まない。次の点はEspBleと異なる。

- `classic()` namespaceは無い。`EspBleClassic`が独立classで、profileは`spp()` / `hidKeyboard()` /
  `a2dpSink()` / `avrcp()` / `hfpClient()`のように直下にある。
- capability照会API（`profileSupport()`）は無い。
- Classic HIDは有効。姉妹libraryは「Core buildで無効」と書いているが、EspBleは独自archiveで有効化している。
- A2DP/HFPはexternal codecで、EspBleが扱うのはencode済みpayloadとraw SCOである。復号PCMは扱わない。
- AVRCPはController/Targetを1つの`avrcp()`が持ち、別objectではない。
- SPPのStream / Serial adapter（姉妹libraryの`EspBluedroidSppSerial`相当）は未実装。
  ガイドに書く前に[棚卸し](CLASSIC_FEATURE_INVENTORY.ja.md)のVFS行を実装するか、書かないかを決める。

## 残作業の規模（概算）

行数は作業量ではないため、性質ごとに分けて見積もる。実装と実機検証は概ね終わっており、
残りは**exampleと文書、最終回帰、外部機器との相互運用、release操作**である。

| 区分 | 残り | 性質 | 目安 |
|---|---|---|---|
| 実装・実機検証（Gate A・B の機能行） | ほぼ完了 | — | Classic / dual-hostの機能検証は完了。残るのはscope決定という判断 |
| Gate F: example 12本 | 12 | 執筆。既存exampleの構成（sketch + README両言語 + `sketch.yaml`）に沿う | 1本あたり1作業単位。合計で数日規模 |
| Gate F: Classic入門ガイド | 2（ja / en） | 執筆。姉妹libraryの構成を土台にAPI名と範囲を書き換える | 1〜2作業日 |
| Gate F: 違いの周知（残り） | 1 | 既存文書への反映と英語版 | 1作業単位 |
| Gate A・E: 利用者向け文書とCHANGELOG整合 | 4 | scope決定の後でなければ確定しない | 1作業日 |
| Gate C: 最終回帰 | 4 | ほぼ機械時間。code freeze後にやり直しが必要 | S3全Peer cleanを複数回で数時間×回数、無印ESP32掃引、P4/C6、example compile workflow |
| Gate B: board / core matrix | 2 | CI（`workflow_dispatch`）実行と生成物確認 | 1作業単位＋CI時間 |
| Gate D＋Gate Bの一部完了2件 | 6 | **外部機器が必要でblockしている。**市販BLE keyboard、外部Host 2種以上、外部Classic HID/SPP/A2DP/AVRCP機器、外部HFP機器 | 機材の準備待ち。準備後は各1作業単位 |
| Gate E: release操作 | 2 | metadata確認、release workflow、公開後確認 | 1作業単位 |

したがってblockしているのは次の2つだけである。

1. **Classicのrelease scope決定**（判断）。これが決まらないとGate Aの利用者向け文書、
   Gate EのCHANGELOG整合、Gate Fのexampleの取捨が確定しない。ただしHIDのように
   実装済みでexampleが無いものは、scopeと独立に埋めてよい。
2. **外部機器**（Gate D）。手元の2台構成では代替できない。

## 推奨実行順

1. AVRCP metadata/play-statusの外部Target相互運用と、HFPの外部機器相互運用を行う。
2. [PCMFlowBluetooth修正依頼](REQUEST_PCMFLOWBLUETOOTH_A2DP_VALIDATION.ja.md)のreset修正と正式E2E結果を受け取り、
   PCMFlowDevice等を接続した実出力と長時間負荷へ進む。
3. A2DP/AVRCP/HFPの到達範囲を踏まえてClassicのrelease scopeを決める。
4. Gate FのexampleとClassic入門ガイドを揃える。scope決定を待つのはexampleの取捨だけで、
   実装済み機能にexampleが無い状態（とくにHID）はscopeと独立に解消する。
5. scope確定後にCHANGELOGと利用者向け文書を更新する。
6. Gate Bのboard/core matrixと他SoC非影響を確定する。
7. コードfreeze後にGate Cのclean全回帰を複数回行う。
8. Gate Dを並行実施し、最後にGate Eのmetadata・release操作へ進む。

コード変更後に全回帰を先行してもfreeze後に再実行が必要です。長時間作業の結果は、合否だけでなく
条件とlog保存先を引き継ぎ文書・技術検証へ記録します。
