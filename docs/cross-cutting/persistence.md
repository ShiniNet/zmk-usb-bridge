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

## Data Model

### 保存候補

- Bond 関連データ
- 既知デバイス識別キー
- 最終接続成功メタデータ
- 設定バージョン

### MVP で保持したい内容

- `BLE stack` が保持する既知キーボード 1 台分の bond 関連データ
- 補助メタデータの永続化フォーマットバージョン
- stack が安定した identity を参照できる場合だけ、その snapshot
- 最後に正常接続できた peer address の snapshot

### 補助メタデータの役割

- bond が無い状態から既知デバイスを証明する役割は持たない
- ログ、診断、初回 attempt のヒントとして使う
- 欠損しても次回正常接続後に再生成できることを優先する
- private address の追跡や directed advertisement の自前解決には使わない

### 未決定

- 単一レコード構成か複数項目か
- 破損検出方式
- Factory reset 時の消去範囲
- identity snapshot を `常に保存` ではなく `取得できるときのみ保存` にする実装詳細

### MVP で保存しないもの

- local name を認証の主キーとしては保存しない
- advertisement 上の一時的な RSSI や service 列挙結果は永続化しない
- 接続可否を単独で決めるための独自デバイス ID は保存しない

## Lifecycle

- 初回ペアリング成功時に保存
- 起動時に読み出し
- 補助メタデータが欠損していても bond が有効なら既知デバイス再接続へ進む
- Bond erase 操作時に消去
- フォーマット変更時に移行または破棄
- 読み出し成功かつ bond が有効なら既知デバイス再接続へ進む
- 読み出し結果が空または消去済みなら自動で `pairing_scan` へ進む
- 補助メタデータを破棄した場合は、次回正常接続成功時に再生成する

## Failure Handling

- 読み出し失敗時は安全側に倒す
- 補助メタデータの破損検知時は、まず補助メタデータだけを破棄して bond 主体で継続を試みる
- bond ストア自体が読めない、存在しない、または初期化済みなら既知デバイス扱いをやめる
- 認証や暗号化で既存 bond の不整合が確定した場合は recovery 導線へ移す
- `peer address snapshot` が現在広告アドレスと一致しないことだけでは fault にしない
- 中途半端な更新を避ける
- bond erase 完了後に古い情報を参照しない
- 補助メタデータ不一致だけでは bond 一致を否定しない

## Constraints

- 1:1 利用が基本
- 高価な追加部品に頼らない
- bond erase はユーザー操作 1 回で確実に完了すること

## Open Questions

- peer address snapshot をどの形式で保持するか
- バージョン不一致時の扱いをどうするか
- 更新頻度と書き込み寿命のバランスをどう取るか
- peer address snapshot を診断専用に留めるか、再接続ヒントとしても参照するか
- identity snapshot を取得できない stack 実装でも十分診断性を確保できるか

## Validation Needed

- 電源断を挟んでも再接続できるか
- 補助メタデータだけを壊しても bond 主体で再接続を継続できるか
- 意図的な bond 不整合で復旧導線が機能するか
- Bond erase 操作後に確実に初期状態へ戻るか
- Bond erase 後に自動で `pairing_scan` へ戻れるか
- 同名別個体に誤って再接続しないか
- peer address snapshot が診断に役立つか

## Related Documents

- `docs/cross-cutting/state-machine.md`
- `docs/subsystems/ble.md`
