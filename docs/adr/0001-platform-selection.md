# ADR 0001: Platform Selection

## Status

Accepted for prototyping

## Context

- BLE central と USB device の両方が必要
- BOM コストを優先する
- 初期候補は ESP32-S3
- 手元の試作基板として `Seeed XIAO ESP32-S3` を利用予定
- ホスト OS は `Windows` を優先する

## Decision

- MVP の試作基盤として `ESP32-S3`、具体的には `Seeed XIAO ESP32-S3` を採用する
- 本番採用の最終判断は、BLE 再接続性、USB HID 安定性、発熱、実装余裕の検証後に確定する

## Options Considered

- ESP32-S3
- 代替 MCU / SoC

## Evaluation Criteria

- BOM コスト
- 入手性
- BLE central 実装難度
- USB HID 実装難度
- 試作容易性

## Consequences

- 早期試作をすぐ始められる
- 初期の設計判断は ESP32-S3 の制約とライブラリ事情に寄る
- 本番ハード確定前に、量産観点の代替 SoC 比較は別途残る

## Follow-up

- 候補比較結果をここに追記する
