# Persistence

## Goal

- 再接続に必要な情報を安全に保持し、破損時も復旧可能にする

## Scope

- Bond 情報
- 既知キーボード識別情報
- バージョニング
- 初期化と消去

## Responsibilities

- 保存対象の定義
- 読み出しタイミングの定義
- 更新契機の定義
- 破損時のフェイルセーフ

## MVP Baseline

- 1:1 利用を前提に、単一キーボード分の永続データだけを扱う
- `BLE stack 管理の bond` と `アプリ管理の補助メタデータ` を分けて扱う
- bond 済み個体の再接続に必要な情報を保持する
- bond erase 実行時は、再接続に使う BLE 関連データを消去し、そのまま `pairing_scan` に戻す
- bond を識別の主キーとし、補助メタデータは診断や接続ヒントに留める
- BLE stack 管理の bond は `CONFIG_BT_SETTINGS` を前提に Zephyr settings 連携へ委譲する

## Data Model

### MVP で保持したい内容

- `BLE stack` が保持する既知キーボード 1 台分の bond 関連データ
- 補助メタデータの永続化フォーマットバージョン
- stack が安定した identity を参照できる場合だけ、その `addr_type + addr[6]` snapshot
- 最後に正常接続できた peer の `addr_type + addr[6]` snapshot

### 補助メタデータの役割

- bond が無い状態から既知デバイスを証明する役割は持たない
- ログと診断に使う
- known-device reconnect の接続可否判定には使わない
- 欠損しても次回正常接続後に再生成できることを優先する
- private address の追跡や自前解決には使わない

### 保存方式

- app metadata は Zephyr settings の専用 subtree に保持する
- settings backend は非 filesystem 系を前提にする
- backend 具体名は platform と version policy に従って `NVS` または後継候補を使う

## Lifecycle

- 初回ペアリング直後ではなく、`security 成立 + required HOG profile 検証通過` 後に採用情報を保存する
- 起動時に読み出し
- 補助メタデータが欠損していても bond が有効なら既知デバイス再接続へ進む
- bond erase 操作時に消去
- フォーマット変更時に移行または破棄
- 補助メタデータを破棄した場合は、次回正常接続成功時に再生成する

## Failure Handling

- 読み出し失敗時は安全側に倒す
- 補助メタデータの破損検知時は、まず補助メタデータだけを破棄して bond 主体で継続を試みる
- bond ストア自体が読めない、存在しない、または初期化済みなら既知デバイス扱いをやめる
- 認証や暗号化で既存 bond の不整合が確定した場合は recovery 導線へ移す
- `peer address snapshot` が現在広告アドレスと一致しないことだけでは fault にしない
- `peer address snapshot` が現在広告アドレスと不一致でも、それだけで reconnect 候補から外す根拠にはしない
- bond erase 完了後に古い情報を参照しない

## Constraints

- 1:1 利用が基本
- 高価な追加部品に頼らない
- bond erase はユーザー操作 1 回で確実に完了すること

## Open Questions

- peer address snapshot をどの形式で保持するか
- バージョン不一致時の扱いをどうするか
- 更新頻度と書き込み寿命のバランスをどう取るか
- identity snapshot を取得できない stack 実装でも十分診断性を確保できるか

## Validation Needed

- 電源断を挟んでも再接続できるか
- 補助メタデータだけを壊しても bond 主体で再接続を継続できるか
- 意図的な bond 不整合で復旧導線が機能するか
- Bond erase 操作後に確実に初期状態へ戻るか
- app metadata subtree と BLE bond subtree が混線しないか

## Related Documents

- `docs/cross-cutting/state-machine.md`
- `docs/subsystems/ble.md`
