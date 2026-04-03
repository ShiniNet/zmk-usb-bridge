# ADR 0004: BLE Stack Baseline

## Status

Accepted

## Context

- MVP の試作基盤は `nRF52840`、具体的には `Seeed XIAO nRF52840` を前提にしている
- BLE central / observer と USB device の両立が必要
- bond 主体の known-device reconnect、privacy、explicit scan ベースの reconnect 制御が必要
- `private address` の解決は、可能な限り stack に委ねたい

## Decision

- MVP の BLE stack 第一案は `ZMK v0.3.x` と整合する Zephyr 系 Bluetooth host とする
- 必須機能の前提は以下とする
- `Central` と `Observer` role を有効にする
- bond は `CONFIG_BT_SETTINGS` を前提に settings 連携で保持する
- privacy 機能を有効にする
- known-peer reconnect では filter accept list または bond-aware reconnect path を優先候補にする
- explicit scan 方式では `known peer advertisement observed -> scan stop -> connect attempt -> fail/disconnect で scan restart` を基本シーケンスとする
- app metadata は stack の bond と分離して保持する

## Options Considered

- `ZMK v0.3.x` と整合する Zephyr 系 Bluetooth host
- `ESP-IDF NimBLE host + ESP controller`

## Evaluation Criteria

- BLE central 実装の成立性
- privacy / bond-aware reconnect の扱いやすさ
- ZMK / Zephyr との親和性
- `nRF52840` 上での試作容易性

## Consequences

- `bond 主体 reconnect` と privacy の設計を Zephyr API と settings へ落としやすくなる
- ZMK と近い概念で設計できるため、bridge 条件や HOG client の前提を揃えやすい
- 一方で、Zephyr USB 実装との接続や HOG client 実装は別途スパイクで詰める必要がある

## Follow-up

- BLE スパイクで、privacy、filter accept list、reconnect 実装可否を確認する
