# ADR 0005: Build and Distribution Workflow

## Status

Accepted for prototyping

## Context

- 試作速度を優先しつつ、利用者ごとの self-build 導線も必要
- 開発ホストは `WSL2`、実機は `Seeed XIAO ESP32-S3`
- 利用者ごとに `pairing name allowlist` を編集できるようにしたい
- 中央 repository から恒久 firmware release を配る前提にはしたくない

## Decision

- build の主経路は `WSL2` 上の `ESP-IDF`
- `flash` と `monitor` は必要なら `Windows` 側実行を許容する
- 配布の主経路は `fork した各自の repository で GitHub Actions を実行し、workflow artifact を取得して自分で書き込む` 方式とする
- 利用者設定の正本は repository 追跡下の `user config fragment` とする
- build は `sdkconfig.defaults` 多段構成を前提にする
- allowlist は user config から設定可能にし、`初回 pairing の補助条件` にのみ使う

## Consequences

- `WSL2` build と `Windows` 側 `flash/monitor` を分けることで試作を止めにくい
- fork 上 self-build を標準導線にすることで、利用者ごとの allowlist 差分を中央配布なしで吸収しやすい
- workflow input や `sdkconfig` 生成物を正本にしないため、再現性と履歴性を保ちやすい
- 利用者には `fork` と Actions artifact の最低限の説明が必要になる

## Follow-up

- `docs/foundation/build-and-distribution.md` に設計詳細を集約する
- user config と artifact 構成を実装へ落とす
