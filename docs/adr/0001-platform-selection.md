# ADR 0001: Platform Selection

## Status

Accepted

## Context

- BLE central と USB device の両方が必要
- BOM コストを優先する
- 無改造 ZMK キーボードとの相性を最優先したい
- 本プロジェクトの主戦場は `BLE central + bond + privacy + reconnect` である
- 認証済みモジュール前提で少量生産可能な構成にしたい
- `Zephyr upstream` を正本にしたい

## Decision

- MVP の実装基盤として `nRF52840` を採用する
- 初期試作基板は `Seeed XIAO nRF52840` を第一候補とする
- ソフトウェア基盤は `Zephyr upstream` を採用する
- 本番ハードでは `nRF52840` 認証済みモジュール搭載 custom board へ展開可能な構成を前提にする

## Options Considered

- `nRF52840 + Zephyr upstream`
- `ESP32-S3 + ESP-IDF`

## Evaluation Criteria

- ZMK / Zephyr との親和性
- BLE central 実装難度
- bond / privacy / reconnect の扱いやすさ
- USB HID 実装難度
- 認証済みモジュール前提の量産移行しやすさ
- BOM コスト

## Consequences

- BLE 側の主要求に対して、設計と言語を Zephyr 系へ揃えやすくなる
- `Seeed XIAO nRF52840` を試作ドングルとしてそのまま使いやすい
- ZMK ユーザーが手元に持っている `XIAO nRF52840` を実験用に流用しやすい
- 単価面では `ESP32-S3` より不利な場合がある
- 既存の `ESP-IDF` 前提スケルトンや文書は抜本的に更新が必要になる

## Follow-up

- BLE stack baseline を `Zephyr upstream Bluetooth host` 前提で更新する
- build / distribution / artifact 方針を `west + UF2` 前提で更新する
