#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  ci_build_bundle.sh --workspace-dir DIR --project-dir DIR --artifact-dir DIR
                     [--profile release|dev-usb-logging]
                     [--board BOARD]
                     [--pristine auto|always|never]
                     [--build-dir DIR]
                     [--run-id ID]

Build zmk-usb-bridge in a west workspace, then collect the resulting files into
an artifact directory suitable for GitHub Actions downloads.
EOF
}

json_escape() {
    local value="${1:-}"
    value=${value//\\/\\\\}
    value=${value//\"/\\\"}
    value=${value//$'\n'/\\n}
    value=${value//$'\r'/\\r}
    value=${value//$'\t'/\\t}
    printf '%s' "$value"
}

render_command() {
    local rendered=()
    local arg
    for arg in "$@"; do
        rendered+=("$(printf '%q' "$arg")")
    done
    printf '%s\n' "${rendered[*]}"
}

copy_if_exists() {
    local src="$1"
    local dest="$2"
    local label="$3"

    if [[ -f "$src" ]]; then
        cp -f "$src" "$dest"
        printf '%s\t%s\n' "$label" "$dest" >>"$OUTPUTS_FILE"
    fi
}

write_artifact_readme() {
    local readme_file="$1"
    local flash_target="zephyr.uf2"

    cat >"$readme_file" <<EOF
zmk-usb-bridge firmware artifact
================================

Profile: $PROFILE
Board:   $BOARD
Status:  $BUILD_STATUS

Main files
----------
- $flash_target : main firmware for Seeed XIAO nRF52840 drag-and-drop flashing
- zephyr.hex    : fallback image for probe/debug workflows
- build_meta.json / command.txt / build.log : reproducibility and troubleshooting data

Flash steps
-----------
1. Put the board into UF2 bootloader mode.
2. Connect it to your computer over USB.
3. Copy $flash_target onto the mounted drive.
4. Wait for the board to reboot.

Notes
-----
- The default end-user artifact is the release profile.
- The dev-usb-logging profile keeps USB logging enabled for bring-up and debugging.
EOF

    if [[ -n "${GITHUB_SERVER_URL:-}" ]] && [[ -n "${GITHUB_REPOSITORY:-}" ]] && [[ -n "${GITHUB_RUN_ID:-}" ]]; then
        cat >>"$readme_file" <<EOF
- Source run: ${GITHUB_SERVER_URL}/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}
EOF
    fi
}

github_run_url() {
    if [[ -n "${GITHUB_SERVER_URL:-}" ]] && [[ -n "${GITHUB_REPOSITORY:-}" ]] && [[ -n "${GITHUB_RUN_ID:-}" ]]; then
        printf '%s/%s/actions/runs/%s' "$GITHUB_SERVER_URL" "$GITHUB_REPOSITORY" "$GITHUB_RUN_ID"
    else
        printf ''
    fi
}

PROFILE="release"
BOARD="seeeduino_xiao_ble"
PRISTINE="always"
WORKSPACE_DIR=""
PROJECT_DIR=""
BUILD_DIR=""
ARTIFACT_DIR=""
RUN_ID=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --workspace-dir)
            WORKSPACE_DIR="${2:?missing value for --workspace-dir}"
            shift 2
            ;;
        --project-dir)
            PROJECT_DIR="${2:?missing value for --project-dir}"
            shift 2
            ;;
        --artifact-dir)
            ARTIFACT_DIR="${2:?missing value for --artifact-dir}"
            shift 2
            ;;
        --profile)
            PROFILE="${2:?missing value for --profile}"
            shift 2
            ;;
        --board)
            BOARD="${2:?missing value for --board}"
            shift 2
            ;;
        --pristine)
            PRISTINE="${2:?missing value for --pristine}"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="${2:?missing value for --build-dir}"
            shift 2
            ;;
        --run-id)
            RUN_ID="${2:?missing value for --run-id}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown argument: %s\n\n' "$1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

case "$PROFILE" in
    release)
        SNIPPETS=()
        ;;
    dev-usb-logging)
        SNIPPETS=(-S zub-usb-logging)
        ;;
    *)
        printf 'Unsupported profile: %s\n' "$PROFILE" >&2
        exit 1
        ;;
esac

case "$PRISTINE" in
    auto|always|never)
        ;;
    *)
        printf 'Unsupported pristine mode: %s\n' "$PRISTINE" >&2
        exit 1
        ;;
esac

if [[ -z "$WORKSPACE_DIR" ]] || [[ -z "$PROJECT_DIR" ]] || [[ -z "$ARTIFACT_DIR" ]]; then
    printf 'workspace-dir, project-dir, and artifact-dir are required.\n' >&2
    exit 1
fi

if [[ -z "$BUILD_DIR" ]]; then
    BUILD_DIR="$WORKSPACE_DIR/build/$BOARD/$PROFILE"
fi

if [[ -z "$RUN_ID" ]]; then
    RUN_ID="$(date '+%Y%m%d_%H%M%S')"
fi

if [[ ! -d "$WORKSPACE_DIR/.west" ]]; then
    printf 'west workspace not found: %s\n' "$WORKSPACE_DIR" >&2
    exit 1
fi

if [[ ! -d "$PROJECT_DIR" ]]; then
    printf 'project directory not found: %s\n' "$PROJECT_DIR" >&2
    exit 1
fi

mkdir -p "$ARTIFACT_DIR"

BUILD_LOG="$ARTIFACT_DIR/build.log"
COMMAND_FILE="$ARTIFACT_DIR/command.txt"
META_FILE="$ARTIFACT_DIR/build_meta.json"
OUTPUTS_FILE="$ARTIFACT_DIR/outputs.txt"
README_FILE="$ARTIFACT_DIR/README.txt"

GIT_HEAD="$(git -C "$PROJECT_DIR" rev-parse HEAD 2>/dev/null || printf 'unknown')"
if [[ -n "$(git -C "$PROJECT_DIR" status --short 2>/dev/null || true)" ]]; then
    GIT_DIRTY="true"
else
    GIT_DIRTY="false"
fi

SOURCE_DIR_REL="$(realpath --relative-to="$WORKSPACE_DIR" "$PROJECT_DIR")"

WEST_CMD=(
    west
    build
    -d "$BUILD_DIR"
    -b "$BOARD"
    "$SOURCE_DIR_REL"
    "--pristine=$PRISTINE"
)
WEST_CMD+=("${SNIPPETS[@]}")

render_command "${WEST_CMD[@]}" >"$COMMAND_FILE"
: >"$OUTPUTS_FILE"

printf 'Building profile=%s board=%s\n' "$PROFILE" "$BOARD"
printf 'Workspace: %s\n' "$WORKSPACE_DIR"
printf 'Project:   %s\n' "$PROJECT_DIR"
printf 'Build dir: %s\n' "$BUILD_DIR"
printf 'Artifacts: %s\n' "$ARTIFACT_DIR"

BUILD_STATUS="failed"
if (
    cd "$WORKSPACE_DIR"
    "${WEST_CMD[@]}"
) 2>&1 | tee "$BUILD_LOG"; then
    BUILD_STATUS="success"
fi

copy_if_exists "$BUILD_DIR/zephyr/zephyr.uf2" "$ARTIFACT_DIR/zephyr.uf2" "zephyr.uf2"
copy_if_exists "$BUILD_DIR/zephyr/zephyr.hex" "$ARTIFACT_DIR/zephyr.hex" "zephyr.hex"
copy_if_exists "$BUILD_DIR/zephyr/zephyr.bin" "$ARTIFACT_DIR/zephyr.bin" "zephyr.bin"
copy_if_exists "$BUILD_DIR/zephyr/zephyr.elf" "$ARTIFACT_DIR/zephyr.elf" "zephyr.elf"
copy_if_exists "$BUILD_DIR/zephyr/zephyr.map" "$ARTIFACT_DIR/zephyr.map" "zephyr.map"
copy_if_exists "$BUILD_DIR/zephyr/.config" "$ARTIFACT_DIR/zephyr.config" "zephyr.config"
copy_if_exists "$BUILD_DIR/zephyr/zephyr.dts" "$ARTIFACT_DIR/zephyr.dts" "zephyr.dts"
copy_if_exists "$BUILD_DIR/compile_commands.json" "$ARTIFACT_DIR/compile_commands.json" "compile_commands.json"

write_artifact_readme "$README_FILE"
printf 'README.txt\t%s\n' "$README_FILE" >>"$OUTPUTS_FILE"

cat >"$META_FILE" <<EOF
{
  "run_id": "$(json_escape "$RUN_ID")",
  "status": "$(json_escape "$BUILD_STATUS")",
  "profile": "$(json_escape "$PROFILE")",
  "board": "$(json_escape "$BOARD")",
  "pristine": "$(json_escape "$PRISTINE")",
  "snippet": "$(json_escape "${SNIPPETS[*]:-}")",
  "workspace_dir": "$(json_escape "$WORKSPACE_DIR")",
  "project_dir": "$(json_escape "$PROJECT_DIR")",
  "build_dir": "$(json_escape "$BUILD_DIR")",
  "artifact_dir": "$(json_escape "$ARTIFACT_DIR")",
  "git_head": "$(json_escape "$GIT_HEAD")",
  "git_dirty": $GIT_DIRTY,
  "github_repository": "$(json_escape "${GITHUB_REPOSITORY:-}")",
  "github_ref": "$(json_escape "${GITHUB_REF:-}")",
  "github_sha": "$(json_escape "${GITHUB_SHA:-}")",
  "github_run_id": "$(json_escape "${GITHUB_RUN_ID:-}")",
  "github_run_attempt": "$(json_escape "${GITHUB_RUN_ATTEMPT:-}")",
  "github_run_url": "$(json_escape "$(github_run_url)")"
}
EOF

printf 'build_meta.json\t%s\n' "$META_FILE" >>"$OUTPUTS_FILE"
printf 'build.log\t%s\n' "$BUILD_LOG" >>"$OUTPUTS_FILE"
printf 'command.txt\t%s\n' "$COMMAND_FILE" >>"$OUTPUTS_FILE"

printf 'Build %s.\n' "$BUILD_STATUS"
printf 'Artifact directory: %s\n' "$ARTIFACT_DIR"

if [[ "$BUILD_STATUS" != "success" ]]; then
    exit 1
fi
