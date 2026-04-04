# Recovery UI

## Goal

- 設定アプリなしでペアリングと復旧操作が完結する
- 最小限の物理 UI で次の操作を判断できる

## Scope

- 物理ボタン操作
- RGB LED 表示
- Pairing 開始
- Bond 初期化
- エラー時の利用者導線

## Responsibilities

- 利用者に現在状態を伝える
- Pairing と復旧の入口を提供する
- 誤操作を減らす

## External Interfaces

- 状態遷移管理
- BLE 管理
- 永続化層
- ハードウェア GPIO

## State / Data

### 決定済み

- 設定アプリなしでの基本運用が必須
- 外部ボタンは 1 個
- 状態表示には RGB の `USRLED` を使う
- `Seeed XIAO BLE` では board DTS が持つ `led0(red) / led1(blue) / led2(green)` を `USRLED` として使う
- ボンド削除はボタン長押しで行う
- 現行 board overlay では外部 recovery button を `XIAO D0` の active-low / pull-up 入力として扱う
- 長押し時間の現行デフォルトは `3000 ms`
- USB 状態に応じた専用 LED パターンは MVP では増やさず、BLE / recovery 状態中心で表現する

### 未決定

- 接続中を常時点灯にするか低輝度にするか

## Proposed Controls

### Button

- `短押し`: 未接続時は即時の再探索トリガ、bond 未登録時は pairing 探索の再開始
- `短押し`: ただし `recovery required` では bond 不整合を解消できないため、状態解除には使わない
- `長押し`: bond erase を実行し、そのまま自動ペアリング探索へ入る

### RGB LED

- `青の緩い点滅`: bond 未登録で pairing 探索中
- `黄の緩い点滅`: bond 登録済みで自動再接続試行中
- `緑の点灯`: 接続中
- `赤の遅い点滅`: `recovery required`
- `赤の速い点滅`: bond erase 実行中
- `赤の点灯`: 致命的エラー

### 暫定 UI Mapping

- `pairing_scan`: `青の緩い点滅`
- `scanning_known_device`: `黄の緩い点滅`
- `reconnecting_fast`: `黄の緩い点滅`
- `reconnecting_backoff`: `黄の緩い点滅`
- `connecting`: `黄の緩い点滅`
- `connected`: `緑の点灯`
- `recovery_required`: `赤の遅い点滅`
- `bond_erasing`: `赤の速い点滅`
- `fatal_error`: `赤の点灯`
- `boot` と `usb_ready`: 消灯

## Failure Handling

- Pairing 失敗時の再試行導線
- Bond 破損時の初期化導線
- `metadata-only` 破損と `bond 不整合` を利用者導線として混同しない
- USB 接続済みだが BLE 未接続の状態表示
- 長期再接続中でも利用者が `未接続だが自動復帰待ち` と理解できる表示

## Constraints

- 物理部品の追加は最小限
- 利用者が覚えやすい操作体系を優先
- ボタン 1 個でも誤って bond erase しにくいこと

## Open Questions

- 接続中を常時点灯にするか、低輝度や消灯を許容するか

## Validation Needed

- 説明なしでも操作を再現できるか
- ペアリングと復旧の導線が短いか
- 状態表示だけで迷わず次の行動を選べるか
- 長押し誤爆による bond erase を防げるか
- `recovery required` と通常の再接続中を見分けられるか

## Related ADRs

- `docs/adr/0001-platform-selection.md`
