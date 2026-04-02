# ADR 0003: USB HID Minimum Descriptor

## Status

Accepted for MVP

## Context

- 初期リリースでキーボード入力とポインティング入力を扱う
- stuck key / stuck button 防止が必須
- ホスト OS は `Windows` を優先する
- ポインティング入力は `相対移動`、`ボタン 1-5`、`縦横スクロール` を必要とする
- キーボード入力は標準的な ZMK キーボード利用に準拠した橋渡しを目指す
- keyboard report は `HKRO` を前提にする
- Consumer Control は `basic` usage 範囲を前提にする

## Decision

- MVP では `Keyboard + Mouse` の複合 USB HID デバイスとして提示する
- 内部構成の第一案は `single HID interface + report IDs` とする
- `Report ID 1 = Keyboard`、`Report ID 2 = Consumer Control`、`Report ID 3 = Mouse` を第一案とする
- Keyboard report は `HKRO` を採用する
- Keyboard report は既存 ZMK の標準 `HKRO` 構成に可能な限り一致させる
- Consumer Control は `basic` usage 範囲で含める
- Mouse / Pointer 側は `相対移動`、`ボタン 1-5`、`縦横スクロール` を含める
- キーボード側の入力意味は標準的な ZMK キーボード利用に合わせて崩さない
- `Keyboard` と `Consumer Control` は順序重視で送る
- `Mouse movement / wheel` は未送信分を集約してよい
- `Mouse buttons` は press / release 順序を潰さない
- `all release` は専用高優先度キューではなく、最優先フラグ方式で扱う
- `Mouse movement / wheel` は固定タイマーを持たず、次の送信可能タイミングで flush する
- `Mouse movement / wheel` の保留値は飽和加算とし、wraparound させない
- BIOS / UEFI 向け最適化や boot protocol 有効化、ジェスチャ専用表現は MVP の対象にしない
- ただし将来の boot protocol 追加余地は残す
- BLE 切断や bond erase 時は keyboard / pointer の両方を安全状態へ戻す
- `all release` は通常送信より優先して扱う
- USB 状態に応じた専用 UI 表示は MVP では増やさない

## Options Considered

- Keyboard と pointer を単一複合で提示
- Keyboard と pointer を分離して提示
- 初期段階では usage を最小限に絞る

## Evaluation Criteria

- OS 互換性
- 実装難度
- デバッグしやすさ
- 異常時の安全性

## Consequences

- Windows 優先の MVP として必要な入力機能を一通りカバーできる
- 既存 ZMK の HID / HOG 実装パターンに寄せられるため、橋側の変換設計が単純になる
- `HKRO` を採ることで将来の boot protocol 追加余地と実装単純性を両立しやすい
- descriptor は consumer と横スクロール対応を含む分だけやや複雑になる
- Mouse については ZMK 既定の 5 ボタン前提に揃えることで、互換性重視の方針に合う
- `NKRO` を採らないため、6 キー超の通常キー同時押し最適化は MVP では対象外になる
- movement / wheel は最新重視で扱えるため、ポインタ鮮度を維持しやすい
- 一方で送出ポリシーが report 種別ごとに異なるため、内部実装は単純な単一 FIFO より少し複雑になる
- 固定タイマーを持たないため、不要な待ち時間を増やさずに済む
- その代わり movement / wheel の保留値管理と `all release` 競合処理の実装責務が増える
- UI 表示を BLE / recovery 中心に保つことで、MVP の状態表現を増やしすぎずに済む
- 将来 BIOS / UEFI 対応を強める場合は再設計が必要になる可能性がある

## Follow-up

- USB スパイクの結果を反映する
