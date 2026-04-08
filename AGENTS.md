# AGENTS.md

## Purpose
This subtree contains the `zmk-usb-bridge` concept and design docs.
Keep `README.md` end-user facing. Put design detail in `docs/`.

## Scope
- Project: `zmk-usb-bridge`
- Goal: BLE-to-USB bridge for unmodified ZMK BLE keyboards
- Not the default ZMK split-central dongle approach

## Source of Truth
- User-facing overview: `README.md`
- Design doc index: `docs/README.md`
- Requirements and scope: `docs/foundation/requirements.md`
- Architecture and technical issues: `docs/foundation/architecture.md`
- Developer workflow: `docs/development/workspace-setup.md`
- AI workflow: `docs/development/ai-workflow.md`

## Important Assumptions
- Unmodified-keyboard support is required.
- Pointing support is in scope for the initial release.
- Treat `1 dongle : 1 keyboard` as the default assumption.
- BIOS / UEFI support is not required for the initial release.
- BOM cost is prioritized over development effort.

## Working Rules
- Preserve the unmodified-keyboard requirement unless the user explicitly changes it.
- Prefer clearer information architecture over longer documents.
- When adding docs, choose filenames that make lookup obvious for both humans and Codex.
- Keep `README.md` focused on end users; do not move design discussion back into it.
- Put design details under the appropriate `docs/` category directory instead of adding more files to `docs/` root.
- Do not quietly steer the project toward ZMK split-central dongle mode.
- Do not expand scope to multiple keyboards or desktop configuration apps unless the user changes direction.
- Treat workspace-root scripts as the primary local entrypoints for build, ZTEST, and log capture.
- When implementation behavior changes, update the related validation or workflow docs in the same pass.
- Prefer small, testable logic seams that can be covered by ZTEST before leaning on manual hardware-only verification.

## Change Priorities
1. Requirements alignment
2. Terminology clarity
3. Separation between user-facing and design-facing docs
4. Fast lookup for future Codex runs
5. Reproducible build / test / logging workflow

## Still Unsettled
- Quantitative targets
- USB HID descriptor details
- Device identification strategy on the BLE side
- Recovery UI details
- Exact Zephyr USB / BLE implementation details
