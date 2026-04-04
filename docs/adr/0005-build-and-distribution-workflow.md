# ADR 0005: Build and Distribution Workflow

## Status

Accepted

## Context

- 試作速度を優先しつつ、利用者ごとの self-build 導線も必要
- 開発ホストは Zephyr application 前提に切り替える
- 実機は `Seeed XIAO nRF52840`
- 利用者ごとに `pairing name allowlist` を編集できるようにしたい
- 中央 repository から恒久 firmware release を配る前提にはしたくない

## Decision

- build の主経路は workspace の build wrapper script から `west build` を呼ぶ運用
- 配布の主経路は `fork した各自の repository で GitHub Actions を実行し、workflow artifact を取得して自分で書き込む` 方式とする
- 利用者設定の正本は repository 追跡下の `user config fragment` とする
- build は `prj.conf` と board/user/ci の差分ファイルで構成する
- `Seeed XIAO nRF52840` 向けの主 artifact は `zephyr.uf2` とする
- allowlist は user config から設定可能にし、`初回 pairing の補助条件` にのみ使う

## Consequences

- `west build` と Zephyr 標準の設定モデルに寄せつつ、workspace build wrapper へ入口を一本化することで、再現性を保ちやすい
- fork 上 self-build を標準導線にすることで、利用者ごとの allowlist 差分を中央配布なしで吸収しやすい
- `UF2` 主導線により、`XIAO nRF52840` の書き込み手順を短く保ちやすい
- 利用者には `fork` と Actions artifact の最低限の説明が必要になる

## Follow-up

- `docs/foundation/build-and-distribution.md` に設計詳細を集約する
- user config と artifact 構成を実装へ落とす
