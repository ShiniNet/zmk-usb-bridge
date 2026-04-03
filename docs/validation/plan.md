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
- private address を含む candidate 判定の成立確認
- unresolved private address を誤って recovery 扱いしないことの確認
- peer address snapshot の診断有効性確認
- bond 不整合を想定した recovery 導線確認
- Zephyr Bluetooth callback と BLE manager queue の責務分離で実装複雑度を抑えられるか
- metadata 更新契機を `security 成立後` に置いても不足がないことの確認
- post-connect bring-up を `connected` 入口条件としても reconnect 体感を損ねないことの確認
- `Keyboard=1`、`Consumer=2`、`Mouse=3` の report reference 前提が `LaLapadGen2` 実機で成立することの確認
- `name allowlist` を設定した場合だけ補助フィルタとして効くことの確認

### USB スパイク

- `Keyboard + Mouse` 複合 HID 列挙
- `single HID interface + report IDs` の成立確認
- 現行 build では `Seeed XIAO nRF52840` 向けに `Keyboard(1) + Consumer(2) + Mouse(3)` descriptor の列挙実装を投入済み
- Keyboard HID 列挙
- Pointing HID 列挙
- `HKRO` keyboard report 動作確認
- Consumer Control `basic` usage 動作確認
- Keyboard / Consumer の順序維持確認
- Mouse movement / wheel 集約確認
- Mouse button press / release 順序確認
- 切断時の全 release
- `all release` 最優先フラグ動作確認
- Windows での安定認識
- ボタン 1-5 の動作確認
- 縦横スクロールの動作確認

### 結合スパイク

- BLE 入力を USB レポートへ橋渡し
- HOG notification body から `report_id` 付き USB report へ正しく再構成できるか
- report body 長不一致や未知 handle を受けても bridge 状態が壊れないか
- 再接続中の USB 側挙動
- エラー復旧と UI の整合
- RGB LED とボタン操作の整合
- bond erase 後の自動 `pairing_scan` 復帰

### Build / Distribution スパイク

- `west build` が日常開発経路として安定するか
- `config/user.conf` の編集だけで allowlist 変更が完結するか
- 利用者 fork 上の GitHub Actions だけで build artifact を取得できるか
- `zephyr.uf2` 主導線だけで利用者が自力で書き込み可能か
- `zephyr.hex` 補助導線が診断に役立つか
- `zub-usb-logging` で Windows の `COM port` 監視だけで十分なログが取れるか
- `config/dev-rtt.conf` を重ねた build で `RTT` runtime log が取れるか
- `USB CDC ACM` を dev profile で許容したとき、bring-up を妨げないか

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
5. Build / Distribution 導線の成立性

## Initial Baseline

- Board: `Seeed XIAO nRF52840`
- Keyboard: `LaLapadGen2`
- Host OS: `Windows`
- UI: `1 external button + 1 RGB USRLED`
