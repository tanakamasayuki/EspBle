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
  process-wide role排他まで完了した。PCMFlowBluetoothの初期仕様はsibling repositoryへ配置済みである。
- 配布形式は次回Classic拡張ではNimBLE source / Classic `.a`のmixed distributionを意図的に維持する。
- dual-hostはcommand/ACL routing、bond/RPA、任意停止順、再attach、FIFO満杯、再登録とheap安定性まで検証済み。
- 数時間級dual-host soak、観測HCI commandのpolicy分類、不正HID report拒否、peer突然消失後の復旧、接続・pairing失敗後の復旧、lifecycle競合監査は完了。公開サポート範囲の確定が未完了。
- `CHANGELOG.md`のUnreleasedと利用者向けClassic文書は、次回release内容として未整理。

## Gate A: Classic / dual-hostの公開範囲

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 完了 | 数時間級dual-host soak | 20 run、1時間41分44秒。command競合／停止・再登録各100サイクル、panic・broker error・heap低下なし |
| 完了 | HCI command policy監査 | 接続後cleanupを含むinventoryを分類し、未知／別host opcodeのfail-closed policyとunit・実機回帰を追加 |
| 完了 | 不正HID report / peer消失 | null・上限超過reportを送信前に拒否し接続を維持。peer突然再起動後にbond済みBLEとClassic HIDを復旧 |
| 完了 | 接続・pairing失敗 | 誤passkey後にbondを残さず再pairing。HID非同期接続失敗後も暗号化LEを維持し、正しいClassic peerへ再接続 |
| 完了 | lifecycle競合監査 | SPP/HID callback targetを登録mutex下で取得し、解除後は取得済み参照が0になるまでstateを保持。clean実機lifecycle回帰成功 |
| 未完了 | release scope決定 | experimental flagのまま同梱するか、正式APIに昇格するか、次回releaseから外すかを決定 |
| 未完了 | 利用者向け文書 | README、Feature Matrix、example、制限、build flagが決定したscopeと一致 |

Gate Aの詳細は[引き継ぎ](HANDOFF_ESP32_CLASSIC.ja.md)を正とします。正式昇格にはACL credit一元管理を
含めるかを判断します。現状の「物理controller-to-host flow control無効化」を許容する場合も、理由と
制限を利用者向けに明記します。

## Gate B: 生成物と互換性

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 完了 | Classic archive生成入口 | IDF/tag/toolchain検査、link check、symbol prefix、必須symbol検査をscript化 |
| 完了 | archive clean再現 | cleanなIDF v5.5.5 worktree・GCC 14.2.0から一時生成し、格納済みarchiveとbyte単位・SHA-256一致 |
| 完了 | Classic Audio archive | external codec A2DP/AVRCP、Voice over HCI / external codec HFPを有効化し、必須API link checkとclean再生成を完了 |
| 完了 | A2DP Sink transport | SBC codec設定、接続・stream状態、callback限定raw view、停止barrierを実装し、ESP32同士で実機転送 |
| 完了 | A2DP Source transport | 固定SBC endpoint、copy送信、MTU検査、`WouldBlock` retryを実装し、100 packetを欠損なく実機転送 |
| 一部完了 | AVRCP CT/TG | passthrough、absolute volume、通知をA2DP併用で実機確認。metadata/play-status受信は外部Targetとの相互運用を残す |
| 一部完了 | HFP | Client/Audio GatewayのSLC、発信・着信・応答・終了、mSBC raw SCO transport、packet statistics、role排他を実機確認。外部機器相互運用を残す |
| 未完了 | board matrix | workflowで対象boardを再生成し、次回versionのBOARDS文書を確定 |
| 未完了 | core matrix | Arduino-ESP32対応versionを再検証し、次回versionのCOMPATIBILITY文書を確定 |
| 未完了 | 他SoC非影響 | S3/C3/C6/H2/P4の代表compileでClassic archive・無印ESP32 patchが非適用 |

archive手順は[Classic host archive再生成](CLASSIC_HOST_BUILD.ja.md)を正とします。
Audioのscopeと段階は[Classic Audio拡張計画](PLAN_ESP32_CLASSIC_AUDIO.ja.md)を正とします。

## Gate C: 自動回帰

| 状態 | 項目 | 完了条件 |
|---|---|---|
| 未完了 | S3標準回帰 | `--clean`全Peer + unitを連続実行し、複数回でflaky failure、heap低下、task残留なし |
| 未完了 | 無印ESP32 NimBLE回帰 | Central / Peripheral両roleの`--clean`掃引が成功 |
| 未完了 | Classic専用回帰 | SPP、HID profile/reportの排他構成が成功 |
| 未完了 | dual-host回帰 | public address、RPA/bond、soakが成功 |
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

## 推奨実行順

1. AVRCP metadata/play-statusの外部Target相互運用と、HFPの外部機器相互運用を行う。
2. PCMFlowBluetoothのA2DP Sink decoder adapterを並行実装し、実SBC連続再生で最大payloadと負荷を測る。
3. A2DP/AVRCP/HFPの到達範囲を踏まえてClassicのrelease scopeを決める。
4. scope確定後にCHANGELOGと利用者向け文書を更新する。
5. Gate Bのboard/core matrixと他SoC非影響を確定する。
6. コードfreeze後にGate Cのclean全回帰を複数回行う。
7. Gate Dを並行実施し、最後にGate Eのmetadata・release操作へ進む。

コード変更後に全回帰を先行してもfreeze後に再実行が必要です。長時間作業の結果は、合否だけでなく
条件とlog保存先を引き継ぎ文書・技術検証へ記録します。
