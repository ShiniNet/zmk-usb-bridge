# ADR 0002: BLE Identification Strategy

## Status

Accepted for MVP

## Context

- 無改造の ZMK BLE キーボード対応が必須
- 誤接続を防ぎつつ、既知デバイスへ自動再接続したい
- 初期ターゲットは `LaLapadGen2`
- キーボード側では ZMK 標準の pairing mode 操作を利用できる

## Decision

- 初回ペアリングでは、利用者に対象キーボードだけを pairing mode にしてもらう運用を採る
- ドングルは bond 未登録なら自動で pairing 探索を開始する
- unbonded pairing scan では `connectable advertisement` と `HID service` と `keyboard appearance` を最低条件にする
- optional config として `local name allowlist` を持てるようにし、設定されている場合だけ補助条件として使う
- 初期実装では `CONFIG_ZMK_USB_BRIDGE_PAIRING_NAME_ALLOWLIST_ENABLED` と `CONFIG_ZMK_USB_BRIDGE_PAIRING_NAME_ALLOWLIST` を用いる
- 初回 pairing 探索は active scan とし、advertisement と scan response の両方から local name を拾える前提で候補判定する
- pairing 探索中は、その場で最初に接続成立した相手を即採用せず、post-connect validation を通過した相手を採用する
- 既知デバイス再接続では bond 情報を主キーにして同名別個体を区別する
- MVP の known-device reconnect では `bond / identity` のみを正本とし、補助メタデータは照合条件に使わない
- local name は補助情報に留め、認証の主条件にしない
- known-device reconnect は passive scan とし、local name や scan response に依存しない
- scan は `接続試行中だけ停止し、失敗または切断後にすぐ再開する` 方針とし、connect attempt に backoff をかける
- 厳密な ZMK 機種判別は MVP では必須にしない
- `LaLapadGen2` 前提の MVP では、post-connect validation の一部として期待 report reference を確認してよい

## Options Considered

- 広告情報ベースで識別
- Bond 情報主体で識別
- GATT 接続後の確認を含める
- 複数条件の組み合わせ

## Evaluation Criteria

- 誤接続リスク
- 再接続速度
- 実装難度
- 無改造要件との整合

## Consequences

- 初回 pairing は運用前提に依存するため、複数機器が同時に pairing mode に入る環境には弱い
- `connectable + HID service + keyboard appearance` により、無関係な BLE 機器への誤接続リスクをさらに下げられる
- bond 後の再接続では誤接続リスクを大きく下げられる
- 補助メタデータに引きずられず、ペアリング済み個体への再接続判定を単純に保ちやすい
- MVP の対象を `LaLapadGen2` に絞ることで、一般化より先に実利用に近い検証を進められる
- local name を主キーにしないため、名前変更や同名機器に引きずられにくい
- allowlist を補助条件に限定することで、user control を残しつつ bond 後の識別正本を汚さずに済む
- active scan を pairing 時にだけ使うことで、候補絞り込みに必要な local name 取得余地を増やしつつ、bond 後の再接続 path は passive scan で単純に保てる
- 接続試行のたびに scan stop / restart が入るため Zephyr 実装には落とし込みやすいが、attempt backoff 定数は試作で妥当性確認が必要になる
- 将来の汎用化では advertisement filter や接続後確認条件を追加する余地が残る

## Follow-up

- BLE スパイクの結果を反映する
