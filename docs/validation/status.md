# Validation Status

## Current Savepoint

- Date: `2026-04-04`
- Savepoint run: `20260404_162314`
- Target keyboard: `LaLapadGen2`
- Board: `Seeed XIAO nRF52840`
- Host OS: `Windows`
- Result: 初回 pairing 成功、切断後再接続成功、入力反映成功

## Confirmed Working Path

1. bond なし起動で `pairing_scan` に入る
2. active scan で allowlist 名 `LalapadGen2` を持つ候補を検出する
3. `scan stop -> connect attempt -> security -> HOG discovery -> subscribe` を通って `connected` に入る
4. 切断後は `reconnecting_fast` へ入り、known-device passive scan を再開する
5. fresh re-observation 後に short stabilization delay を置いて reconnect attempt を行う
6. bonded reconnect では explicit `bt_conn_set_security(BT_SECURITY_L2)` を先行させず、HOG discovery を開始する
7. 必要な security upgrade は Zephyr の ATT retry path に委ねる
8. `connected` 復帰後、キーボード入力が再びホストへ届く

## Important Implementation Notes

- bonded reconnect での明示 security 要求は、`smp_pairing_complete status 0x8` と `encrypt_change hci status 0x1f` を誘発しやすかった
- 現行実装では、bonded peer へ再接続できたら HOG discovery を先に開始する
- stale bond 判定は 1 回の security failure だけで確定させず、連続 failure で確定する
- fresh re-observation の直後は short stabilization delay を挟む

## Remaining Risks

- reconnect の長期反復で同じ成功率を維持できるかは未検証
- bond mismatch recovery 導線は実装済みだが、今回の savepoint では成功経路を優先している
- reconnect 失敗時の timeout 理由と peer-visible 判定の組み合わせは、追加 run でまだ詰められる

## Next Recommended Validation

- 同一セッションでの複数回 reconnect 連続試験
- 電源投入タイミングを変えた reconnect 試験
- 長時間放置後の reconnect 試験
- bond erase からの再 pairing と再 reconnect の回帰試験
