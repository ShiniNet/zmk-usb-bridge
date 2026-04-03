# BLE Runtime Module Split

## Goal

- `ble_manager` が Zephyr BLE 実装の巨大な受け皿になるのを防ぐ
- scan、connect、security、backoff、HOG bring-up を責務ごとに分割する

## Why Split Early

- このプロジェクトの BLE 側は `stack init + settings + bond 判定 + explicit scan + connect + security + HOG discovery + reconnect scheduling` を含む
- これらを 1 ファイルへ寄せると、Zephyr callback、timer、状態遷移、永続化判断が強く絡みやすい
- 実装前に境界を切っておくことで、`ble_manager` を coordinator に留めやすくする

## Module Boundary

### `ble_manager`

- BLE ドメイン全体の coordinator
- 単一 event queue の所有
- BLE 内部イベントを state machine へ渡す唯一の入口
- app 層からの高水準トリガを BLE 下位モジュールへ振り分ける
- high-level event type と payload 正規化の責務を持つ

### `ble_runtime`

- `bt_enable()`
- Bluetooth settings 読み出し
- bond inventory の初期判定
- Zephyr callback 登録の起点

### `ble_scan`

- explicit scan の start / stop
- advertisement / scan response の parse
- pairing 時の active scan と bonded reconnect 時の passive scan の切り替え
- `pairing_filter` と known-peer 条件を使った candidate 正規化

### `ble_connection`

- `bt_conn_le_create()` と切断要求
- `connected` / `disconnected` / `security_changed` の正規化
- active connection context の管理

### `ble_reconnect`

- fast reconnect と backoff reconnect の schedule
- button short press による fast reconnect reset
- advertisement 再観測時の attempt 再開条件
- `peer visible / invisible` 判定と visibility timeout 管理
- reconnect timer 満了は BLE 内部ローカルで扱い、state machine の public event へは出さない

### 既存の分割を維持するもの

- `hog_client`: post-connect bring-up と report subscription
- `pairing_filter`: unbonded pairing 候補条件
- `persist`: app metadata のみを扱う
- `state_machine`: USB / BLE / recovery をまたぐ状態遷移

## State Machine Command Surface

`state_machine` から BLE ドメインへは、Zephyr API ではなく以下の高水準 command だけを出す。

- `START_PAIRING_SCAN`
- `START_KNOWN_DEVICE_SCAN`
- `ENTER_RECONNECT_FAST`
- `ENTER_RECONNECT_BACKOFF`
- `STOP_SCAN`
- `ERASE_BONDS`

## Startup Readiness Responsibility Split

- `ble_runtime` は Bluetooth stack の enable、Bluetooth settings 読み出し、bond inventory 判定だけを担当する
- `persist` は app metadata の読み出し、破損判定、discard / store を担当する
- `startup` coordinator は `ble_runtime` と `persist` の結果をまとめ、state machine へ `BLE_SYNCED` と `PERSIST_READY_*` を流す
- `app_main` は個別初期化の手順詳細を持たず、`startup` coordinator を起動するだけに留める

## Command Semantics

### `START_PAIRING_SCAN`

- unbonded pairing 用の active scan policy で待機へ入る

### `START_KNOWN_DEVICE_SCAN`

- bond / identity 主体の passive known-device scan へ入る

### `ENTER_RECONNECT_FAST`

- fast reconnect schedule を初期化する
- known-device scan 再開までを BLE 側で面倒を見る
- failure streak を 0 に戻し、`即時 + 0.5s + 1s + 2s` の attempt 計画を作る

### `ENTER_RECONNECT_BACKOFF`

- backoff reconnect schedule を有効化する
- 次の attempt 許可タイミングは BLE 側で管理する
- MVP では `5s`、以後 `10s cap` の attempt 計画を使う

### `STOP_SCAN`

- host reset や recovery 待機など、BLE 側を待機状態へ寄せたいときに使う

### `ERASE_BONDS`

- BLE stack 側の bond erase と app metadata erase の両方を起動する
- state machine は erase 手順の詳細を知らない
- 完了通知は `BLE bond erase` と `app metadata erase` の両方成功時だけ返す

## Coordination Rules

- Zephyr callback から state machine を直接更新しない
- BLE 下位モジュールは正規化済みイベントを `ble_manager` へ返す
- `ble_manager` だけが queue を通して state machine へ渡す
- `state_machine` は BLE stack API を直接呼ばず、高水準 command に留める
- `known peer が visible かどうか` の局所判定は `ble_scan` / `ble_reconnect` 側に閉じ込め、state machine へは `CONNECT_FAILURE` や `DISCONNECTED` などの高水準イベントだけを渡す
- event payload の `reason` は正規化済みカテゴリ、`status_code` は Zephyr / HCI 生コードの保持先として使う
- event payload の `capability flags` は `hog_client` の発見結果を state machine とログへ伝えるために使う

## File Weight Guideline

- `ble_manager.c` は queue と orchestration を主責務とし、scan parse や connection callback 本体を持たない
- 1 モジュールが `scan + connection + reconnect timer + HOG discovery` の複数責務を同時に持ち始めたら再分割を検討する
- Zephyr callback の種類が増えたときは、まず `ble_connection` か `ble_scan` 側へ追加し、`ble_manager` へ戻さない

## Related Documents

- `docs/subsystems/ble.md`
- `docs/cross-cutting/state-machine.md`
- `docs/cross-cutting/persistence.md`
