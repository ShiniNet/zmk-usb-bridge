# ADR 0007: Artifact and Debug Console Policy

## Status

Accepted for prototyping

## Context

- 利用者向けには簡潔な書き込み手順を用意したい
- 一方で、開発者向けには診断しやすい build 出力も残したい
- `ESP32-S3` では USB HID 本番経路と debug console の干渉に注意が必要

## Decision

- GitHub Actions artifact には `merged.bin` と分割 firmware 一式の両方を含める
- `merged.bin` は利用者向け主導線とする
- 分割 firmware 一式には少なくとも `bootloader.bin`、`partition-table.bin`、application `.bin`、`flash_args` を含める
- debug console は当面 `開発専用 USB CDC` を許容する
- USB HID 安定化フェーズでは `UART` を第一候補として再評価する

## Consequences

- 利用者向け手順は短く保ちやすい
- offset や partition 構成を含む診断性も維持できる
- `開発専用 USB CDC` は立ち上がりを速くするが、安定化後に `UART` へ寄せる判断余地を残す

## Follow-up

- `docs/foundation/build-and-distribution.md` に詳細を集約する
- validation で artifact 構成と debug console 方針を確認する
