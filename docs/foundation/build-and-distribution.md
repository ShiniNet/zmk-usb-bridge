# Build and Distribution

## Goal

- ローカル試作と GitHub Actions ベース self-build の両立方針を定義する
- build 入力、利用者設定、artifact の正本を明確にする

## MVP Baseline

- `zmk-usb-bridge` は独立した `ESP-IDF` アプリとして扱う
- 試作ボードは `Seeed XIAO ESP32-S3`
- `ESP-IDF` は `v5.5.3` に pin する
- 開発の主経路は `WSL2` build とする
- `flash` と `monitor` は必要に応じて `Windows` 側実行を許容する
- 利用者配布は `fork -> GitHub Actions -> artifact download -> local flashing` を標準導線にする
- 利用者設定は repository 追跡下の設定フラグメントで扱う
- 中央 repository から恒久 firmware release を配る前提は置かない

## Local Workflow

- source tree は `WSL2` の Linux filesystem 上に置く
- build は `WSL2` 上の `ESP-IDF` 環境で行う
- `Windows` 側で `flash` や `monitor` を行う場合でも、build をやり直す前提は置かない
- `Windows` 側は `WSL2` 生成物を参照して書き込む補助経路として扱う

## Version Policy

- ローカルと GitHub Actions は同じ `ESP-IDF v5.5.3` を使う
- `release/v5.5` のような追従ブランチではなく `v5.5.3` タグ固定を使う
- bugfix 取り込みは `v5.5.x -> v5.5.y` の明示更新で扱う
- version 更新時は、文書、ローカル手順、CI 設定を同時に更新する

## Build Profiles

- `release profile` は利用者向け artifact の正本とする
- `release profile` では development-only debug transport を前提にしない
- `dev profile` は bring-up と調査のための補助構成とする
- 当面は `開発専用 USB CDC` の採用候補を `dev profile` に寄せる
- USB HID 安定化フェーズでは `UART` を第一候補として再評価する

## Configuration Model

### Fragment Roles

- `sdkconfig.defaults`: project 共通既定値
- `sdkconfig.defaults.xiao_esp32s3`: board 固有既定値
- `config/user.defaults`: upstream が最小デフォルトを保持し、利用者が fork 上で編集する設定
- `sdkconfig.defaults.ci`: CI 実行都合の差分だけを持つ設定

### Composition Rules

- build 入力の正本は `sdkconfig` 生成物ではなく `sdkconfig.defaults` 系ファイルとする
- 標準順序は `sdkconfig.defaults -> sdkconfig.defaults.xiao_esp32s3 -> config/user.defaults -> sdkconfig.defaults.ci` を第一候補にする
- 利用者意味論を変える設定は `sdkconfig.defaults.ci` に置かない
- CI でもローカルと同じ設定入力を使い、workflow 側で別の正本を増やさない

## User Configuration

- 利用者が直接編集するのは `config/user.defaults` のような単一ファイルを第一候補にする
- `menuconfig` や workflow input を利用者設定の正本にしない
- 初回 pairing 用 `local name allowlist` は user config から設定可能にする
- user config は commit され、build 入力として履歴追跡できる状態を保つ

### Initial User-Facing Settings

- `CONFIG_ZMK_USB_BRIDGE_PAIRING_NAME_ALLOWLIST_ENABLED`
- `CONFIG_ZMK_USB_BRIDGE_PAIRING_NAME_ALLOWLIST`

### Current Constraints

- allowlist は `comma-separated exact match`
- 名前ごとの長さと件数には実装上限がある
- allowlist は `初回 pairing の補助条件` にのみ使い、bond 後の識別正本には使わない

## CI / Artifact Model

- build は upstream repository または利用者 fork 上の GitHub Actions で実行する
- 配布単位の第一候補は `workflow artifact`
- 標準 artifact は `release profile` の build 結果を格納する
- artifact には `merged.bin` と分割 firmware 一式の両方を含める
- `merged.bin` は利用者向け主導線とする
- 分割 firmware 一式には少なくとも `application bin`、`bootloader`、`partition table`、`flash arguments` を含める
- 分割 firmware 一式は診断と保守向けの補助導線とする
- public repository 運用を第一候補にし、private fork 前提は標準導線にしない

## Open Questions

- artifact に利用者向け簡易書き込み手順書を同梱するか
- `UART` への移行判断をどの検証結果で行うか
- `release profile` と `dev profile` の差分をどこまで持つか

## Validation Needed

- `WSL2 build + Windows flash/monitor` の組み合わせが日常運用として成立するか
- `config/user.defaults` 編集だけで allowlist 変更が完結するか
- fork 上 GitHub Actions artifact だけで利用者が self-build できるか
- `merged.bin` 主導線と分割 firmware 補助導線の二本立てが有効か
- `開発専用 USB CDC` が bring-up 導線として十分成立するか
- `release profile` と `dev profile` の責務分離が運用上破綻しないか

## Related Documents

- `docs/foundation/requirements.md`
- `docs/foundation/architecture.md`
- `docs/adr/0005-build-and-distribution-workflow.md`
- `docs/adr/0006-esp-idf-version-pin.md`
- `docs/adr/0007-artifact-and-debug-console-policy.md`
