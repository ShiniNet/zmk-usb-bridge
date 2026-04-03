# BLE

## Goal

- 無改造の ZMK BLE キーボードをドングルから発見、接続、再接続できるようにする
- 1 ドングル : 1 キーボードの運用を単純な操作で成立させる

## Scope

- Scan 条件
- 接続対象の識別
- Pairing / bonding
- Bond 永続化
- 再接続制御
- 切断時の BLE 側処理

## Responsibilities

- 既知キーボード探索
- 初回ペアリング開始
- 接続先の妥当性確認
- Bond 情報の保存と読み出し
- 再接続リトライ制御
- scan filter と connect attempt 制御

## MVP Baseline

- 参照キーボードは `LaLapadGen2`
- target stack の暫定第一案は `ESP-IDF NimBLE host + ESP controller`
- 初回ペアリング時は、キーボード側で未登録プロファイルを選ぶことで ZMK 標準のペアリングモードへ入ってもらう
- ドングル側は bond 未登録なら自動でペアリング探索を開始する
- ペアリング中は、その場で最初に成立した対象を採用する
- 既知デバイスへの再接続は bond 情報を基準に行う
- unbonded pairing scan では `connectable advertisement` と `HID service の存在` を最低条件にする
- bonded reconnect では bond / identity を主キーにし、広告名は認証に使わない
- scan 自体は止めず、connect attempt にだけ backoff をかける

## External Interfaces

- キーボード側 BLE 広告
- GATT / HID over GATT 入力受信
- 永続化層
- 状態遷移管理
- Recovery UI

## State / Data

### 決定済み

- 無改造キーボード対応が必須
- 1:1 利用を基本とする
- 初回は `LaLapadGen2` を前提に設計する
- bond が無い起動では自動ペアリング探索に入る
- bond 済み個体とは再接続で区別できる
- pairing window 中は最初に成立した相手を採用する運用とする
- unbonded pairing scan の最低 filter は `connectable + HID service`
- bonded reconnect では bond / identity を主キーとし、local name は補助情報に留める
- scan duty を細かく切るより、connect attempt 間隔を backoff する方針を採る

### 未決定

- target stack が directed advertisement や private address をどこまで application 層へ解決済み情報として渡してくれるか
- target stack で accept list / resolving list 主体の known-peer reconnect をどこまでそのまま使えるか
- stack 解決不能時に `再検出待ち` へ留めるだけで実用 reconnect 体感を維持できるか

## Target Stack Baseline

- MVP 実装の第一案は `ESP-IDF NimBLE host + ESP controller` とする
- 必須ロールは `Central` と `Observer` とする
- bond 永続化は NimBLE の NVS 永続化を第一候補にする
- `CONFIG_BT_NIMBLE_HOST_ALLOW_CONNECT_WITH_SCAN` を前提に、scan 継続中の connect attempt を許容する
- `CONFIG_BT_NIMBLE_ENABLE_CONN_REATTEMPT` と `CONFIG_BT_NIMBLE_MAX_CONN_REATTEMPT=3` を、`fast reconnect` の内部再試行補助として使う
- privacy 機能は有効化し、host-based privacy より controller 側解決を優先候補にする
- resolving list / accept list の更新は、active scan / connect 中の動的更新を前提にせず、初期化時または bond 更新直後に反映する
- target stack がこの前提を満たせない場合は、MVP の privacy / reconnect 戦略を再検討する

## Implementation Baseline

### Initialization Sequence

- BLE 関連の初期化は `USB ready` 後に開始する
- 実装の第一案では以下の順で初期化する
1. `nvs_flash_init()`
2. `esp_nimble_hci_and_controller_init()`
3. `nimble_port_init()`
4. `ble_hs_cfg` の callback と security policy 設定
5. bond 永続化初期化
6. アプリ側 metadata 読み出し
7. `nimble_port_freertos_init()`
- scan / connect 開始は `ble_hs_cfg.sync_cb` 後にのみ許可する
- `ble_hs_cfg.reset_cb` を受けた場合は BLE manager を `boot 相当` に戻し、USB 側 safe state を維持したまま host resync を待つ

### Own Address Policy

- own address type は `ble_hs_id_infer_auto(privacy=1)` の結果を第一候補にする
- privacy 無効または address infer 失敗時だけ、public または static random identity にフォールバックする
- own address type は scan / connect / pairing の各 GAP procedure で一貫して使う

### Task / Ownership Model

- NimBLE host callback から直接複雑な状態遷移を完結させず、BLE manager の単一 event queue に正規化して渡す
- scan result、connect result、disconnect、security result、host reset はすべて `BLE internal event` として queue 化する
- `state-machine.md` の状態遷移は、この BLE manager queue を唯一の更新入口とする
- USB への release 要求は BLE callback から直接行わず、状態遷移処理側で発火する

### GAP Event Mapping

- `sync_cb`: bond / metadata / own address type の準備完了後、`pairing_scan` または `scanning_known_device` を開始する
- `reset_cb`: 自動再接続を一時停止し、host 再同期後に scan を再開する
- `BLE_GAP_EVENT_DISC`: scan policy に従って candidate 判定し、known peer を観測したら即 `connecting` または `reconnecting_fast` の attempt を許可する
- `BLE_GAP_EVENT_CONNECT`: 成功時は即 `connected` に入らず `connecting` を維持し、post-connect bring-up を開始する
- `BLE_GAP_EVENT_CONNECT`: 失敗時は `fast reconnect` または `backoff reconnect` の schedule を更新する
- `BLE_GAP_EVENT_DISCONNECT`: USB safe state を要求し、known bond が有効なら reconnect 系状態へ戻す
- `BLE_GAP_EVENT_ENC_CHANGE` または同等の security 完了通知: bond / authentication 成立を確認し、metadata 再生成可否を判断する
- `BLE_GAP_EVENT_REPEAT_PAIRING` または同等の既存 bond 衝突通知: 自動 erase はせず、MVP では `recovery_required` 判定候補として扱う
- `BLE_GAP_EVENT_DISC_COMPLETE`: 意図せぬ scan 停止であれば `scanning_known_device` または `pairing_scan` を再開する

### Post-Connect HID Bring-up

- `BLE_GAP_EVENT_CONNECT` 成功直後に即 `connected` へ遷移せず、`connecting` の中で post-connect bring-up を完了させる
- bring-up の第一案は `security establish -> service discovery -> report discovery -> input report subscribe` の順とする
- HID service、Report Map、Report characteristics、CCCD を見つけられない場合は、MVP では `connected` に入らず connect failure 扱いで切断してよい
- Input Report notify/indicate の購読完了をもって `入力橋渡し開始可能` とみなす
- `connected` は `bond / security 成立済み` かつ `必要な input report subscription 完了済み` のときだけ入る
- Consumer Control や Pointer に必要な report が欠ける場合の扱いは MVP では `LaLapadGen2` 必須前提で fail-fast 寄りにする
- host LED output や feature report は MVP の bring-up 完了条件に含めない
- bring-up 途中の失敗は `connect failure` と同等に扱い、pairing 中なら `pairing_scan` へ、既知 peer なら reconnect 系状態へ戻す

### Startup Readiness Gate

- 起動時の scan 開始条件は `USB ready`、`BLE sync 完了`、`bond 有無の判定完了` の 3 つが揃った時点とする
- `USB ready` 前に BLE 初期化を進めてもよいが、scan / connect 開始は readiness gate 通過後まで許可しない
- `bond 有無の判定完了` は、NimBLE store の既知 bond 状態と app metadata 読み出し結果をまとめて `startup decision` として扱う
- metadata が欠損・破損していても、bond が有効なら readiness gate の結果は `known bond present` とする
- readiness gate 通過後、`known bond present` なら `scanning_known_device`、それ以外なら `pairing_scan` へ入る

### HOG Report Handling Baseline

- keyboard、consumer、pointer の各 input report は `report reference` 情報と characteristic handle の対応で識別する
- report map 全体の厳密解釈より、`必要 input report の発見と subscribe` を優先する
- HOG input notification の payload は `report_id` 付き全体 struct ではなく、該当 report の `body` 部分として扱う
- report notification 受信後の USB bridge 変換は BLE callback で直接行わず、BLE manager queue へ渡して USB 側へ橋渡しする
- unknown report type や MVP 対象外 report はログ対象に留め、bridge の primary path を止めない
- reconnect 後は previous discovery cache を盲信せず、最低限の report availability を再確認する

### LaLapadGen2 Report Baseline

- `LaLapadGen2` の bridge 対象 input report は `Keyboard`、`Consumer Control`、`Mouse / Pointer` の 3 本を必須とする
- report reference の期待値は `Keyboard input = id 1 / type input`、`Consumer input = id 2 / type input`、`Mouse input = id 3 / type input` とする
- `CONFIG_ZMK_POINTING_SMOOTH_SCROLLING=y` のため、`Mouse feature = id 3 / type feature` の存在も想定する
- `Mouse feature report` は reconnect bring-up の必須条件にはしないが、存在すれば read/write 可能性を確認して将来拡張に備える
- `HID indicators output = id 1 / type output` は MVP の bridge 完了条件には含めない
- local config に keyboard / consumer の HID override が無い限り、`Keyboard = HKRO 6-key`、`Consumer = basic usages x 6` を想定する
- 上記は `LaLapadGen2` config が `pointing` を有効化していることと、ZMK 既定の HID defaults に基づく暫定前提である

### Report Discovery Procedure

- HIDS service 発見後は `Report characteristic -> Report Reference descriptor -> CCC descriptor` の順にたどる
- `Report Reference` を読んで `id` と `type` を確定してから subscribe する
- `Keyboard input` と `Consumer input` と `Mouse input` の 3 本が揃った時点を `minimum viable input path ready` とする
- `Mouse feature` や `LED output` は見つかれば記録するが、MVP では missing でも connect failure にしない
- `Report Map` 読み出しは診断用には有用だが、MVP では report routing の主キーにしない
- subscribe 済み handle は connection lifetime にのみ有効とみなし、切断後に再利用しない

### Persistence Boundary in Code

- bond の保存と復元は NimBLE store に委譲する
- アプリ側 metadata は別 namespace / 別 record として管理し、bond ストアと混在させない
- metadata 更新契機は `接続成功` ではなく、`既知 peer として security 成立 + 入力橋渡し開始可能` を確認した時点にする
- metadata 読み出し失敗時は metadata record のみ discard し、bond 読み出し可否を優先して次状態を決める
- bond erase 実行時は NimBLE store と app metadata の両方を消す

### Discovery / Connect Procedure Split

- passive scan は `BLE_HS_FOREVER` 相当の継続 scan を第一案とする
- known peer の接続可否判定は `DISC event` ごとに行う
- connect attempt 実行中も scan 継続許可設定を前提にする
- stack が auto-connect を素直に提供するなら、known-peer reconnect は manual connect よりそちらを優先候補にする
- stack が auto-connect で candidate 判定や state 観測を難しくする場合は、scan callback + explicit connect の方へ寄せる

## Scan Policy

### Unbonded Pairing Scan

- passive scan を継続する
- `connectable advertisement` のみを対象にする
- advertisement または scan response に `HID service` が見えることを最低条件にする
- advertisement の `Appearance` が keyboard 系であることを追加条件の第一候補とする
- local name はあればログや補助判断に使ってよいが、MVP では必須条件にしない
- optional config として `name allowlist` を持てるようにし、設定されている場合だけ `local name` 一致を補助条件に加えてよい
- 初期実装では `CONFIG_ZMK_USB_BRIDGE_PAIRING_NAME_ALLOWLIST_ENABLED` と `CONFIG_ZMK_USB_BRIDGE_PAIRING_NAME_ALLOWLIST` を想定する
- 利用者には対象キーボードだけを pairing mode にしてもらう

### Bonded Reconnect Scan

- passive scan を継続する
- 既知 bond に対応する相手だけを接続候補にする
- local name や RSSI は認証条件にしない
- directed advertisement を見つけた場合も、`BLE stack / controller` が bond / identity と結び付けられる範囲で接続候補にする
- 既知デバイスの広告を新たに観測した時点では、backoff 状態を持ち越さず即 connect attempt 候補に戻す

## Discovery / Identification

### 初回ペアリング

- ドングルに bond が無い場合は自動でペアリング探索を開始する
- 利用者には対象キーボードだけをペアリングモードにしてもらう
- ドングルは MVP では厳密な機種判別よりも、`connectable + HID service + keyboard appearance` と `想定対象だけが pairing mode に入っている` 運用前提を採る
- `name allowlist` が設定されている場合だけ、advertisement / scan response の local name 一致を追加条件にしてよい
- 初期実装では allowlist の形式を `comma-separated exact match` とする
- 最初に接続成立したキーボードを即採用せず、post-connect validation を通過した相手だけを採用する
- post-connect validation では、少なくとも `HIDS service 存在`、`必要 input report 発見`、`必要 report reference の整合` を確認する
- `LaLapadGen2` 前提の MVP では `Keyboard=1`、`Consumer=2`、`Mouse=3` の report reference 条件を採用条件にしてよい
- post-connect validation に失敗した相手は `adopt しない`。pairing により一時 bond が生成されていた場合は、その接続の cleanup と bond erase を行って scan へ戻る

### 既知デバイス再接続

- 既知デバイスの識別は bond 情報を主キーとして扱う
- reconnect 可否の最終判断は `BLE stack が保持する bond` と、それに紐づく identity 解決結果を基準にする
- 同名の別個体は bond 不一致として区別する
- bond が一致しないデバイスには自動再接続しない
- 保存した補助メタデータがあっても、それだけで接続を許可しない

### Identification / Persistence Boundary

- `BLE stack 管理の bond` を識別の正本とする
- アプリ側永続化は `補助メタデータ` のみを持ち、bond の代替主キーは持たない
- MVP の補助メタデータ最小集合は `metadata format version`、`identity snapshot if available`、`last successful peer address snapshot` とする
- `identity snapshot if available` は stack が安定した identity address を参照できる場合だけ保存候補にする
- `peer address snapshot` は診断と次回初回 attempt のヒントに留め、bond 不一致を覆す根拠にはしない
- local name、RSSI、advertisement 上の service 列挙結果は補助メタデータに含めない
- 補助メタデータ欠損や破損だけでは再ペアリングへ倒さず、bond が有効なら bond 主体で再接続を継続する
- local name の allowlist は `初回 pairing の補助条件` にのみ使い、bond 後の既知 peer 識別には使わない

### Privacy / Directed Advertisement Policy

- MVP では `private address` や `directed advertisement` の解決を、可能な限り `BLE stack / controller` の privacy 機能に委ねる
- known-device reconnect の第一候補は、bond に紐づく identity と accept list / resolving list 相当の仕組みを使った接続である
- resolving list や accept list の更新は、target stack によっては `scan / connect 中に変更できない` 制約がありうるため、初期化時または bond 更新直後に反映する前提で設計する
- アプリ側は `last successful peer address snapshot` を stable identity とみなさない
- RPA 変化や directed advertisement を、アプリ独自の address 比較や local name 比較で吸収しようとしない
- scan callback で peer identity が解決できる場合だけ、その情報を candidate 判定に使う
- scan callback で identity 解決できない場合でも、controller 側 auto-connect や bond-aware connect path が使えるなら、それを優先する
- `private address を解決できないこと` 自体では `recovery required` にしない
- `bond/auth mismatch が確定したこと` だけを recovery 導線へ送る条件にする
- stack / controller が directed advertisement や privacy 解決を吸収できない場合、MVP では推測接続を増やさず、`再検出待ち + 継続 scan` を優先する

## Reconnection Strategy

- 接続断や起動時の再接続は自動で開始する
- 基本方針は `継続再接続` とする
- scan 自体は既知デバイス不在時を含めて継続する
- 既知デバイス広告を観測した時点では、まず即 connect attempt を許可する
- 起動直後、切断直後、または `既知デバイス広告の再観測直後` は `fast reconnect` として短い間隔で接続を試みる
- `backoff reconnect` は `相手が見えているのに connect attempt が連続失敗する場合` にだけ使う
- 長時間未接続でも scan は止めず、次に広告が見えた瞬間に即 connect attempt へ戻れる設計を目指す
- MVP の第一案では connect attempt の目安を `即時 + 0.5s + 1s + 2s` とし、その後に失敗が続く場合だけ `5s -> 10s cap` の backoff へ移る
- 暫定仕様では `fast reconnect` を `即時 + 0.5s + 1s + 2s` の計 4 回までとし、最後の fast attempt 失敗後に `reconnecting_backoff` へ移る
- 暫定仕様では `reconnecting_backoff` の attempt 間隔を `5s -> 10s cap` とする
- button 短押し時は backoff 状態をリセットし、`fast reconnect` へ戻してよい
- backoff 定数、ホストや熱への影響は試作で検証する

## Failure Handling

- 接続失敗時はまず `fast reconnect` を行い、連続失敗時のみ再接続バックオフへ移行する
- 誤った対象へ接続するリスクは `対象キーボードのみを pairing mode にする` 運用で最小化する
- 誤採用リスクは `connectable + HID service + keyboard appearance` と post-connect validation でさらに下げる
- 補助メタデータの欠損、破損、version 不一致は `metadata discard + 再生成待ち` として扱う
- local bond が読めない、または存在しない場合は既知デバイス扱いをやめて `pairing_scan` へ戻す
- local bond はあるが認証や暗号化で不整合が確定した場合は `recovery required` とし、ボタン長押しによる bond erase 導線を用意する
- 接続断時は USB 側へ release 処理を要求して stuck input を防ぐ
- 同一名デバイス発見だけでは接続を許可しない
- 補助メタデータ不一致はログや診断には使えても、bond 一致を覆さない
- 既知デバイスが一度見えなくなり、後で再び広告を出した場合は backoff の待ちを持ち越さず `fast reconnect` に戻す

## Constraints

- 初期リリースでポインティング入力を含む
- 設定アプリなしで運用可能であること
- BOM コスト優先
- Windows 優先で評価する

## Open Questions

### Advertisement edge cases

- target stack が directed advertisement や private address をどこまで application 層へ解決済み情報として渡してくれるか
- target stack で accept list / resolving list 主体の known-peer reconnect をどこまでそのまま使えるか
- stack 解決不能時に `再検出待ち` へ留めるだけで実用 reconnect 体感を維持できるか

### Generalization boundary

- `LaLapadGen2` 以外へ広げるときに、`connectable + HID service` 以外の追加条件が必要か

## Resolution Order

1. target stack における privacy / directed advertisement 実装可否を確認する
2. `LaLapadGen2` 以外への一般化条件を後段で見直す

この順にする理由は、reconnect scheduling は暫定仕様を置けたため、次の主要リスクは bond 主体設計を target stack 上でどこまで実装に落とせるかの確認だからである。一般化条件は参照機で成立性を確認した後に詰めても遅くない。

## Validation Needed

- `LaLapadGen2` を安定して広告検出できるか
- `connectable + HID service` filter で pairing scan が十分に成立するか
- `keyboard appearance` を加えても pairing 成立率が落ちすぎないか
- Bond 永続化後の自動再接続が Windows 運用で実用的か
- security 成立後に HOG service / input report subscribe を完了し、初回入力を落とさず USB bridge へ渡せるか
- post-connect validation 失敗時に一時 bond を確実に消去して scan へ戻れるか
- 既知デバイス広告の再観測直後に即 connect attempt へ入れるか
- `fast reconnect` から `backoff reconnect` への移行条件が reconnect 体感を悪化させないか
- 補助メタデータを消した状態でも bond 主体で再接続を継続できるか
- local bond 不整合と metadata-only 破損を区別して扱えるか
- 継続再接続で過剰な電流、発熱、ログノイズ、接続不安定化が起きないか
- ボンド済み個体と同名別個体を区別できるか
- private address や directed advertisement を含んでも既知個体の再接続判断が破綻しないか
- `peer address snapshot` が診断に役立つか、また再接続ヒントとして過剰に依存しなくて済むか
- `name allowlist` を導入する場合、dynamic device name や user rename と衝突しない運用にできるか

## Related ADRs

- `docs/adr/0001-platform-selection.md`
- `docs/adr/0002-ble-identification-strategy.md`
- `docs/adr/0004-ble-stack-baseline.md`
