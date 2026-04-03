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

## Runtime Module Boundary

- `ble_manager` は BLE 全体の coordinator と event queue 所有者に留める
- Zephyr stack 初期化と callback 登録は `ble_runtime` に置く
- explicit scan と advertisement parse は `ble_scan` に置く
- connect / disconnect / security callback 正規化は `ble_connection` に置く
- fast reconnect / backoff reconnect の schedule は `ble_reconnect` に置く
- HOG bring-up は `hog_client` を維持して分離する
- 詳細な責務分割の正本は `docs/subsystems/ble-runtime.md` とする

## MVP Baseline

- 参照キーボードは `LaLapadGen2`
- target stack の第一案は `zmkfirmware/zephyr v3.5.0+zmk-fixes` 系の Bluetooth host on nRF52840
- 初回ペアリング時は、キーボード側で未登録プロファイルを選ぶことで ZMK 標準のペアリングモードへ入ってもらう
- ドングル側は bond 未登録なら自動でペアリング探索を開始する
- 既知デバイスへの再接続は bond 情報を基準に行う
- unbonded pairing scan では `connectable advertisement` と `HID service の存在` を最低条件にする
- bonded reconnect では bond / identity を主キーにし、広告名は認証に使わない
- scan は `接続試行中だけ停止し、失敗または切断後にすぐ再開する` を基本とする
- connect attempt には backoff をかける

## External Interfaces

- キーボード側 BLE 広告
- GATT / HID over GATT 入力受信
- 永続化層
- 状態遷移管理
- Recovery UI

## State / Data

### 決定済み

- 初回 pairing、既知 peer reconnect、privacy、metadata の原則は本書の各専用節を正本とする
- 特に識別の正本は `bond / identity`、allowlist は `初回 pairing の補助条件`、再接続は `scan restart + connect attempt backoff` を基本とする

### 未決定

- `Zephyr Bluetooth host` が privacy 解決済み情報を candidate 判定へどこまで素直に渡せるか
- filter accept list 主体の known-peer reconnect をどこまでそのまま使えるか
- stack 解決不能時に `再検出待ち` へ留めるだけで実用 reconnect 体感を維持できるか

## Target Stack Baseline

- MVP 実装の第一案は `zmkfirmware/zephyr v3.5.0+zmk-fixes` 系の Bluetooth host とする
- 必須ロールは `Central` と `Observer` とする
- bond 永続化は `CONFIG_BT_SETTINGS` を前提に Zephyr の settings 連携へ委譲する
- privacy 機能は有効化し、BLE stack 側の解決を優先候補にする
- known-peer reconnect では filter accept list または bond-aware reconnect path を優先候補にする
- target stack がこの前提を満たせない場合は、MVP の privacy / reconnect 戦略を再検討する

## Implementation Baseline

### Initialization Sequence

- BLE 関連の初期化は `USB ready` 後に開始してよい
- 実装の第一案では以下の順で初期化する
1. settings backend 準備
2. `bt_enable()`
3. Bluetooth settings 読み出し
4. app metadata 読み出し
5. bond 読み出し
6. BLE manager queue / worker 準備
- scan / connect 開始は Bluetooth stack 初期化完了後にのみ許可する
- host 初期化失敗や再初期化時は BLE manager を `boot 相当` に戻し、USB 側 safe state を維持したまま再開機会を待つ

### Task / Ownership Model

- Bluetooth callback から直接複雑な状態遷移を完結させず、BLE manager の単一 event queue に正規化して渡す
- scan result、connect result、disconnect、security result はすべて `BLE internal event` として queue 化する
- `state-machine.md` の状態遷移は、この BLE manager queue を唯一の更新入口とする
- USB への release 要求は BLE callback から直接行わず、状態遷移処理側で発火する
- Zephyr の explicit central scan を前提に、`bt_conn_le_create()` の直前には scan を停止し、接続失敗または切断後に scan を再開する

### Post-Connect HID Bring-up

- 接続成功直後に即 `connected` へ遷移せず、`connecting` の中で post-connect bring-up を完了させる
- bring-up の第一案は `security establish -> service discovery -> report discovery -> input report subscribe` の順とする
- HID service、Report Map、Report characteristics、CCCD を見つけられない場合は、MVP では `connected` に入らず connect failure 扱いで切断してよい
- Input Report notify/indicate の購読完了をもって `入力橋渡し開始可能` とみなす
- `connected` は `bond / security 成立済み` かつ `keyboard input report subscription 完了済み` のときだけ入る
- `consumer` と `pointer` は MVP では optional capability とし、欠けていても `connected` を妨げない
- optional report が見つかった場合は subscribe と橋渡しを有効化し、見つからない場合は capability absent としてログに留める
- host LED output や feature report は MVP の bring-up 完了条件に含めない

### Startup Readiness Gate

- 起動時の scan 開始条件は `USB ready`、`BLE enabled`、`bond 有無の判定完了` の 3 つが揃った時点とする
- `bond 有無の判定完了` は、BLE stack の既知 bond 状態と app metadata 読み出し結果をまとめて `startup decision` として扱う
- metadata が欠損・破損していても、bond が有効なら readiness gate の結果は `known bond present` とする
- readiness gate 通過後、`known bond present` なら `scanning_known_device`、それ以外なら `pairing_scan` へ入る

### HOG Report Handling Baseline

- keyboard、consumer、pointer の各 input report は、存在する場合に `report reference` 情報と characteristic handle の対応で識別する
- report map 全体の厳密解釈より、`必要 input report の発見と subscribe` を優先する
- HOG input notification の payload は `report_id` 付き全体 struct ではなく、該当 report の `body` 部分として扱う
- report notification 受信後の USB bridge 変換は BLE callback で直接行わず、BLE manager queue へ渡して USB 側へ橋渡しする
- unknown report type や MVP 対象外 report はログ対象に留め、bridge の primary path を止めない
- MVP の bring-up 完了条件に含める必須 role は `keyboard input` のみとする
- reconnect 後は previous discovery cache を盲信せず、最低限の report availability を再確認する

## Scan Policy

### Unbonded Pairing Scan

- 初回 pairing 探索は active scan を使う
- `connectable advertisement` のみを対象にする
- advertisement または scan response に `HID service` が見えることを最低条件にする
- advertisement の `Appearance` が keyboard 系であることを追加条件の第一候補とする
- local name は advertisement または scan response から取得し、あればログや補助判断に使ってよい
- `name allowlist` は optional config とし、設定されている場合は `local name` 一致を補助条件として使う
- `name allowlist` を使わない場合でも、pairing 探索は scan response を拾える active scan のままでよい
- 利用者には対象キーボードだけを pairing mode にしてもらう
- 接続候補を見つけたら `scan stop -> connect attempt` へ進み、失敗したら scan を再開する

### Bonded Reconnect Scan

- bonded reconnect は passive scan を継続する
- 既知 bond に対応する相手だけを接続候補にする
- local name 取得や scan response は不要とし、local name や RSSI は認証条件にしない
- privacy 解決済み identity を candidate 判定に使える場合はそれを最優先する
- `peer invisible` になった後の fresh re-observation では、backoff 状態を持ち越さず即 `scan stop -> connect attempt` 候補に戻す

## Discovery / Identification

### 初回ペアリング

- ドングルに bond が無い場合は自動でペアリング探索を開始する
- 利用者には対象キーボードだけをペアリングモードにしてもらう
- 候補判定条件は `Scan Policy` の `Unbonded Pairing Scan` を正本とする
- 最初に接続成立したキーボードを即採用せず、post-connect validation を通過した相手だけを採用する
- post-connect validation では、少なくとも `HIDS service 存在`、`keyboard input report 発見`、`keyboard report reference の整合` を確認する
- `LaLapadGen2` 前提の MVP では `Keyboard=1` を必須採用条件とし、`Consumer=2` と `Mouse=3` は存在すれば整合確認する optional capability として扱ってよい
- post-connect validation に失敗した相手は `adopt しない`

### 既知デバイス再接続

- 既知デバイスの識別原則は `bond 情報を主キーとして扱う`
- reconnect 可否の最終判断は `BLE stack が保持する bond` と、それに紐づく identity 解決結果を基準にする
- MVP の known-device reconnect では `bond / identity` のみを正本とし、アプリ補助メタデータは照合条件に使わない
- 同名の別個体は bond 不一致として区別する
- bond が一致しないデバイスには自動再接続しない
- 保存した補助メタデータがあっても、それだけで接続を許可しない

### Identification / Persistence Boundary

- `BLE stack 管理の bond` を識別の正本とする
- アプリ側永続化は `補助メタデータ` のみを持ち、bond の代替主キーは持たない
- MVP の補助メタデータ最小集合は `metadata format version`、`identity snapshot if available`、`last successful peer address snapshot` とする
- `identity snapshot if available` は stack が安定した identity address を参照できる場合だけ保存候補にする
- `peer address snapshot` は診断専用に留め、known-device reconnect の接続可否判定には使わない
- local name、RSSI、advertisement 上の service 列挙結果は補助メタデータに含めない
- 補助メタデータ欠損や破損だけでは再ペアリングへ倒さず、bond が有効なら bond 主体で再接続を継続する

### Privacy Policy

- MVP では `private address` の解決を、可能な限り BLE stack の privacy 機能に委ねる
- known-device reconnect の第一候補は、bond に紐づく identity と filter accept list 相当の仕組みを使った接続である
- アプリ側は `last successful peer address snapshot` を stable identity とみなさない
- RPA 変化を、アプリ独自の address 比較や local name 比較で吸収しようとしない
- `private address を解決できないこと` 自体では `recovery required` にしない
- `bond/auth mismatch が確定したこと` だけを recovery 導線へ送る条件にする

## Reconnection Strategy

- 接続断や起動時の再接続は自動で開始する
- 基本方針は `継続再接続` とする
- scan は `接続試行中だけ停止し、それ以外の待機時間では再開する` 方針とする
- `fast reconnect` への入口は `起動直後`、`切断直後`、`ボタン短押し`、または `既知 peer を見失った後の fresh re-observation` とする
- `fast reconnect` の attempt schedule は `即時 + 0.5s + 1s + 2s` の 4 回を基準とする
- connect failure の連続回数は `bt_conn_le_create()` 失敗、`connected` 後の security failure、または `HID ready` 前の bring-up failure を 1 回として数える
- `backoff reconnect` は `同じ known peer が visible のまま 4 回連続失敗した場合` にだけ入る
- `backoff reconnect` の attempt schedule は `5s`、以後 `10s cap` を繰り返す
- known peer が `2s` 以上観測されなかった場合は `peer invisible` とみなし、pending reconnect delay を破棄して `scanning_known_device` 相当の待機へ戻る
- `fresh re-observation` は `peer invisible` になった後に最初の既知広告を観測した瞬間だけを指し、広告 packet ごとには発火させない
- 長時間未接続でも、接続試行中以外は scan を再開し続け、`fresh re-observation` 時に即 connect attempt へ戻れる設計を目指す
- button 短押し時は visible 状態と failure streak をリセットし、`fast reconnect` へ戻してよい
- `bond/auth mismatch` が確定した場合は `backoff reconnect` を継続せず、`recovery_required` へ送る

## Failure Handling

- 接続失敗時はまず `fast reconnect` を行い、連続失敗時のみ再接続バックオフへ移行する
- 接続失敗時は `scan restart` を優先し、次の connect attempt だけを delay 制御する
- known peer が見えていない間は delay 満了だけで connect attempt を起こさず、scan 継続を優先する
- 誤った対象へ接続するリスクは `対象キーボードのみを pairing mode にする` 運用で最小化する
- 誤採用リスクは `connectable + HID service + keyboard appearance` と post-connect validation でさらに下げる
- 補助メタデータの欠損、破損、version 不一致は `metadata discard + 再生成待ち` として扱う
- local bond が読めない、または存在しない場合は既知デバイス扱いをやめて `pairing_scan` へ戻す
- local bond はあるが認証や暗号化で不整合が確定した場合は `recovery_required` とし、ボタン長押しによる bond erase 導線を用意する
- 接続断時は USB 側へ release 処理を要求して stuck input を防ぐ

## Related Documents

- `docs/subsystems/ble-runtime.md`
- `docs/cross-cutting/state-machine.md`
- `docs/cross-cutting/persistence.md`
