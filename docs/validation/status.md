# Validation Status

## Current Savepoint

- Date: `2026-04-05`
- Savepoint run: `20260405_003814`
- Target keyboard: `LaLapadGen2`
- Board: `Seeed XIAO nRF52840`
- Host OS: `Windows`
- Result: bonded reconnect は比較的安定し、`conn_fail_to_estab` 後も scan 復帰と proactive retry で `connected` へ戻れることを複数回確認した
- Note: session metadata 上の build profile は `dev-usb-logging` だが、`git_head=N/A` のため commit provenance は run artifact だけでは確定しない

## Confirmed Working Path

1. bond なし起動で `pairing_scan` に入る
2. active scan で allowlist 名 `LalapadGen2` を持つ候補を検出する
3. `scan stop -> connect attempt -> security -> HOG discovery -> subscribe` を通って `connected` に入る
4. 切断後は `reconnecting_fast` へ入り、known-device passive scan を再開する
5. fresh re-observation 後に short stabilization delay を置く
6. retry timer 満了時は、次の広告 packet 待ちではなく BLE 内部から proactive に reconnect attempt を起こす
7. bonded reconnect では explicit `bt_conn_set_security(BT_SECURITY_L2)` を先行させず、HOG discovery を開始する
8. 必要な security upgrade は Zephyr の ATT retry path に委ねる
9. `connected` 復帰後、キーボード入力が再びホストへ届く

## Important Implementation Notes

- bonded reconnect での明示 security 要求は、`smp_pairing_complete status 0x8` と `encrypt_change hci status 0x1f` を誘発しやすかった
- 現行実装では、bonded peer へ再接続できたら HOG discovery を先に開始する
- stale bond 判定は 1 回の security failure だけで確定させず、連続 failure で確定する
- fresh re-observation の直後は short stabilization delay を挟む
- known peer の直近広告アドレスを保持し、retry timer 満了時に proactive reconnect attempt を起こす
- known-device reconnect では connection context が busy な間は `scan stop` へ進まず、その場で attempt を見送る
- build 確認は build 設計どおり `ShiniNet/` から `../scripts/build_zmk_usb_bridge.sh` を使う

## Remaining Risks

- reconnect の長期反復で同じ成功率を維持できるかは未検証
- bond mismatch recovery 導線は実装済みだが、今回の savepoint では成功経路を優先している
- reconnect 失敗時の timeout 理由と peer-visible 判定の組み合わせは、追加 run でまだ詰められる
- `Run ID: 20260404_225835` では、キーボード電源OFF/ON復帰直後の再接続で bridge が黄LED緩点滅のまま停滞した
- `Run ID: 20260404_234529` では最後の失敗が `hci_name=conn_fail_to_estab` で、scan 復帰後の retry 起動条件が主な弱点と判断した
- 上記に対して、pending `bt_conn_le_create()` watchdog、reconnect mode 反映、proactive retry を追加した
- `Run ID: 20260405_000314` では `attempt trigger dispatched from reconnect timer` を確認でき、bonded reconnect 改善の裏付けになった
- `Run ID: 20260405_000314` では unbonded pairing failure 後に `bt_le_scan_start failed err=-5 pairing=1` も出ており、pairing path の scan restart 手順に整理余地が見えた
- ただし `Run ID: 20260405_003814` では同種の `err=-5 pairing=1`、`command=1 failed before state=2`、`event dispatch failed type=13` は再現しなかった
- `Run ID: 20260405_003814` では `conn_fail_to_estab` 後に `scan started pairing=0` と `attempt trigger dispatched from reconnect timer` を経て再び `state=5` へ戻る列を複数回確認できた

## Next Recommended Validation

- 同一セッションでの複数回 reconnect 連続試験
- 電源投入タイミングを変えた reconnect 試験
- 長時間放置後の reconnect 試験
- bond erase からの再 pairing と再 reconnect の回帰試験
- 特に「電源OFF直後にすぐON」「長時間OFF後にON」の両パターンで、`attempt trigger dispatched from reconnect timer` と再接続成功が続くか確認する
- `attempt armed mode=` の直後に `attempt trigger dispatched from reconnect timer` と `connect create started` が続くかを確認する
- stale bond / pairing failure 時の pairing path は、`err=-5 pairing=1` が再発しないかを継続監視する
