# Validation Plan

## Goal

- 未確定事項を試作で潰し、設計判断へ戻せる形で記録する

## Validation Tracks

### BLE スパイク

- `LaLapadGen2` を対象とした pairing
- 既知キーボード探索
- Pairing / bonding
- Bond 永続化
- 自動再接続
- 継続再接続時のバックオフ挙動
- `connectable + HID service` filter の成立確認
- `keyboard appearance` 追加後も候補検出が成立することの確認
- scan 継続 + connect attempt backoff の成立確認
- 既知広告の再観測で即 connect attempt へ戻ることの確認
- `fast reconnect` から `backoff reconnect` へ移る閾値の妥当性確認
- metadata-only 破損からの自動回復確認
- private address / directed advertisement を含む candidate 判定の成立確認
- unresolved private address を誤って recovery 扱いしないことの確認
- peer address snapshot の診断有効性確認
- bond 不整合を想定した recovery 導線確認
- NimBLE `sync_cb` / `reset_cb` / GAP event から状態遷移へ正しく正規化できることの確認
- metadata 更新契機を `security 成立後` に置いても不足がないことの確認
- post-connect bring-up を `connected` 入口条件としても reconnect 体感を損ねないことの確認
- `Keyboard=1`、`Consumer=2`、`Mouse=3` の report reference 前提が `LaLapadGen2` 実機で成立することの確認
- `Mouse feature = id 3 / type feature` の有無と扱いが `pointing_smooth_scrolling` 設定と一致することの確認
- pairing 候補が post-connect validation 失敗時に採用されず cleanup できることの確認
- `name allowlist` を設定した場合だけ補助フィルタとして効くことの確認

### USB スパイク

- `Keyboard + Mouse` 複合 HID 列挙
- Keyboard HID 列挙
- Pointing HID 列挙
- `HKRO` keyboard report 動作確認
- `HKRO` descriptor が既存 ZMK 想定と矛盾しないことの確認
- Consumer Control `basic` usage 動作確認
- Keyboard / Consumer の順序維持確認
- Mouse movement / wheel 集約確認
- Mouse movement / wheel の `next send opportunity` flush 確認
- Mouse movement / wheel の飽和加算確認
- Mouse button press / release 順序確認
- 切断時の全 release
- `all release` 最優先フラグ動作確認
- Windows での安定認識
- ボタン 1-5 の動作確認
- 縦横スクロールの動作確認
- Consumer release all 動作確認

### 結合スパイク

- BLE 入力を USB レポートへ橋渡し
- HOG notification body から `report_id` 付き USB report へ正しく再構成できるか
- report body 長不一致や未知 handle を受けても bridge 状態が壊れないか
- 再接続中の USB 側挙動
- エラー復旧と UI の整合
- RGB LED とボタン操作の整合
- bond erase 後の自動 `pairing_scan` 復帰

## Per-Validation Template

### Hypothesis

- 何を確認したいか

### Setup

- 使用 MCU / board
- 使用キーボード
- 使用ホスト OS
- 電源条件

### Procedure

- 手順

### Pass Criteria

- 合格条件

### Result

- 実測結果

### Decision Impact

- どの設計判断に影響するか

## Initial Priority

1. BLE の識別と再接続成立性
2. USB 最小 descriptor の成立性
3. 切断時の stuck key / stuck button 防止
4. 最小 UI での復旧性

## BLE Spike Decision Outputs

- reconnect scheduling の妥当性確認
- backoff を `不在時間` ではなく `連続 connect failure` に限定することの妥当性
- bond 主体の識別を補助する永続化メタデータの最小集合
- metadata-only 破損を自動回復扱いにしてよいか
- private address / directed advertisement を含む candidate 判定の許容範囲
- target stack が privacy 解決をどこまで吸収できるか
- bond 不整合を recovery へ送る条件
- NimBLE callback と BLE manager queue の責務分離で実装複雑度を抑えられるか
- `LaLapadGen2` の HOG report discovery 前提が bridge 実装と矛盾しないか

## Initial Baseline

- Board: `Seeed XIAO ESP32-S3`
- Keyboard: `LaLapadGen2`
- Host OS: `Windows`
- UI: `1 external button + 1 RGB USRLED`
