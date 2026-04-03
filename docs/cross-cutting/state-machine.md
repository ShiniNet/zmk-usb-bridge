# State Machine

## Goal

- BLE、USB、Recovery UI をまたぐ状態遷移を一貫して定義する
- 例外系でも利用者と実装の両方が迷わない設計にする

## Scope

- 起動
- 既知デバイス探索
- 初回ペアリング
- 接続中
- 再接続
- Bond 初期化

## States

### MVP の主要状態

- `boot`
- `usb_ready`
- `pairing_scan`
- `scanning_known_device`
- `connecting`
- `connected`
- `reconnecting_fast`
- `reconnecting_backoff`
- `recovery_required`
- `bond_erasing`
- `fatal_error`

## Transition Rules

### 決定済み

- 電源投入後はまず USB を初期化し、その後 BLE 側の処理へ入る
- `state_machine` は BLE stack API を直接呼ばず、BLE ドメインへは高水準 command だけを出す
- 保存済み bond があれば既知キーボードへの自動再接続を試みる
- 保存済み bond が無ければ自動で `pairing_scan` に入る
- 接続断時は USB 側を安全状態に戻した上で自動再接続へ入る
- 再接続は停止せず、接続試行中だけ scan を止め、失敗または切断後は scan を再開した上で connect attempt 間隔だけを短周期フェーズから長周期フェーズへ移す
- 補助メタデータ破損だけでは `recovery_required` に入らず、metadata discard 後に通常の既知デバイス再接続へ戻す
- 既存 bond の不整合が確定した場合のみ `recovery_required` に入る
- unresolved private address の再検出失敗だけでは `recovery_required` に入らない
- bond erase 後は `pairing_scan` に遷移する
- BLE command を伴う遷移では `command 起動成功` を確認してから state を更新する
- `peer invisible` や `fresh re-observation` は BLE 内部ローカル状態として扱い、state machine の public event には上げない

## Events

- USB 給電開始
- USB enumeration 完了
- startup persist 判定完了 (`bond あり` / `bond なし`)
- BLE 初期化完了
- 既知デバイス広告検出
- Pairing 要求
- Pairing 成功 / 失敗
- HID bring-up 完了 / 失敗
- BLE 切断
- Bond/auth mismatch 検出
- USB 側エラー
- ボタン短押し
- Bond erase 操作

### Event Payload Policy

- event type は高水準で保ち、細かな BLE ローカル事情は payload の `reason` と `capability flags` に載せる
- `CONNECT_FAILURE`、`HID_FAILURE`、`DISCONNECTED` は payload の `reason` と `status_code` で具体原因を表す
- `HID_READY` は payload の `capability flags` で `keyboard / consumer / pointer` の有無を運べるようにする
- `peer invisible`、visibility timeout、reconnect timer 満了は `ble_scan` / `ble_reconnect` に閉じ込める

## BLE Commands

- `START_PAIRING_SCAN`
- `START_KNOWN_DEVICE_SCAN`
- `ENTER_RECONNECT_FAST`
- `ENTER_RECONNECT_BACKOFF`
- `STOP_SCAN`
- `ERASE_BONDS`

## State Outline

### `boot`

- 内部初期化を行う
- 永続化データの整合性を確認する

### `usb_ready`

- USB HID device として待機可能な状態
- `BLE 初期化完了` と `startup persist 判定完了` を待つ合流点として扱う
- これらの readiness 入力は `startup` coordinator から event として流し込む
- readiness gate 通過後、`bond あり` なら `scanning_known_device`、`bond なし` なら `pairing_scan` へ分岐する

### `pairing_scan`

- bond 未登録時の自動ペアリング探索
- `connectable + HID service + keyboard appearance` を最低条件にした scan を継続する
- 候補発見時は `scan stop -> connect attempt` へ進み、失敗時は scan を再開する
- 接続成立だけでは採用せず、`connecting` 内の adopt 前検証を通過した対象だけを採用する

### `scanning_known_device`

- bond 済みデバイスの広告を探索する
- 接続試行の許可は bond / identity 条件で絞る
- scan は待機中に継続し、connect attempt の直前だけ停止する
- identity 解決できない private address を見つけても、即 `recovery_required` へは進まず scan 継続を優先する
- `peer invisible` からの fresh re-observation を受けたら、backoff 状態を持ち越さず `connecting` へ進める

### `connecting`

- 発見済み対象へ接続し、入力受信の確立を待つ
- BLE link up 後の security、service discovery、input report subscribe 完了までを含む
- 初回 pairing 中は `adopt 前検証` もこの状態に含める

### `connected`

- BLE 入力を USB へ橋渡しする通常状態
- post-connect bring-up が完了するまでは入らない

### `reconnecting_fast`

- 切断直後の短周期再接続フェーズ
- scan は待機中に再開し、connect attempt を短い間隔で許可する
- `fast reconnect` の attempt schedule は `即時 + 0.5s + 1s + 2s` の 4 回を基準とする
- 同じ known peer が visible のまま 4 回連続失敗したら `reconnecting_backoff` へ移る
- `peer invisible` になった場合は pending attempt を捨てて `scanning_known_device` へ戻す
- fresh re-observation を受けた直後も、このフェーズへ戻してよい
- ボタン短押しでこの状態へ戻し、attempt schedule を初期化してよい

### `reconnecting_backoff`

- 相手が見えているのに connect attempt が連続失敗した場合の低負荷再接続フェーズ
- scan は待機中に再開し、connect attempt を長めの間隔で許可する
- attempt 間隔は `5s`、以後 `10s cap` とする
- `peer invisible` になった場合は pending attempt を捨てて `scanning_known_device` へ戻す
- fresh re-observation を受けたら `reconnecting_fast` へ戻し、attempt schedule を初期化する

### `recovery_required`

- 既存 bond の不整合が確定し、自動再接続を継続しても改善しないと判断した状態
- 利用者には long press による bond erase と再ペアリングを促す
- 短押しだけではこの状態を解除しない

### `bond_erasing`

- 永続化データを消去し、初期状態へ戻す
- `BOND_ERASE_COMPLETE` は `BLE bond erase` と `app metadata erase` の両方成功時だけ発火させる
- どちらか一方でも失敗した場合は完了扱いせず、この状態に留まる

## Failure Handling

- 切断時に USB 全 release を確実に行う
- Bond 不整合を検出したら bond erase 導線を明示する
- 再接続は継続するが、同一候補への高頻度 connect attempt を永続しないようにバックオフする
- 補助メタデータ破損だけでは `recovery_required` に入れず、再生成可能な fault として扱う
- `connecting` 中の bring-up 失敗は、pairing と reconnect で戻り先を分けて扱う
- USB 側致命エラー時のみ `fatal_error` を許容する
- `bond/auth mismatch` は reconnect schedule を継続せず `recovery_required` を優先する

## Validation Needed

- 主要イベント列を時系列でトレースできるか
- stuck key を起こす遷移が残っていないか
- 想定外の順序でも復旧可能か
- 継続再接続がホストや SoC に悪影響を与えないか
- `scan restart + attempt backoff` の組み合わせで reconnect 体感が悪化しないか
- metadata-only 破損で `recovery_required` へ誤遷移しないか
- `peer invisible -> fresh re-observation -> 即 reconnect` の列が過剰再試行なく成立するか

## Related Documents

- `docs/subsystems/ble.md`
- `docs/subsystems/usb.md`
- `docs/subsystems/recovery-ui.md`
- `docs/cross-cutting/persistence.md`
