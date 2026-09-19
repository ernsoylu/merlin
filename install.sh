#!/bin/sh
# Merlin toolchain installer. Idempotent: a second run changes nothing.
#
#   ./install.sh [--dry-run]
#
# Not run as root. sudo is used only for distribution packages, and only if
# something is actually missing. Everything else lands under $HOME.
set -eu

ROOT=$(cd "$(dirname "$0")" && pwd)
DRY=0
[ "${1:-}" = "--dry-run" ] && DRY=1

# ESP-IDF creates its own Python environment. A pre-existing virtualenv (for
# example PlatformIO's) makes that installer fail, so use a system interpreter
# for the bootstrap and put its directory first while ESP-IDF is installing.
PYTHON_BIN=$(command -v python3 2>/dev/null || true)
if [ -z "$PYTHON_BIN" ] || ! "$PYTHON_BIN" -c 'import sys; raise SystemExit(sys.prefix != sys.base_prefix)' 2>/dev/null; then
    for candidate in /usr/bin/python3 /usr/local/bin/python3; do
        if [ -x "$candidate" ] && "$candidate" -c 'import sys; raise SystemExit(sys.prefix != sys.base_prefix)' 2>/dev/null; then
            PYTHON_BIN=$candidate
            break
        fi
    done
fi
[ -n "$PYTHON_BIN" ] || { echo "install.sh: system python3 not found" >&2; exit 2; }
PATH="$(dirname "$PYTHON_BIN"):$PATH"
export PATH

# Pins come from toolchain.env and nowhere else -- check-env reads the same file.
. "$ROOT/toolchain.env"

IDF_DIR=${IDF_PATH:-$HOME/esp/esp-idf}
VENV="$ROOT/.venv"

say()  { printf '==> %s\n' "$*"; }
skip() { printf '    (already) %s\n' "$*"; }
run()  {
    if [ "$DRY" -eq 1 ]; then
        printf '    would run: %s\n' "$*"
    else
        "$@"
    fi
}

[ "$(id -u)" -eq 0 ] && { echo "install.sh: do not run as root" >&2; exit 2; }

# --- distribution packages ---------------------------------------------------
# ESP-IDF's own prerequisites. Checked first so an already-provisioned machine
# never prompts for a password.
PKGS="git wget flex bison gperf python3 python3-venv python3-pip cmake ninja-build
      ccache libffi-dev libssl-dev dfu-util libusb-1.0-0"
if command -v dpkg-query >/dev/null 2>&1; then
    MISSING=""
    for pkg in $PKGS; do
        dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed" \
            || MISSING="$MISSING $pkg"
    done
    if [ -n "$MISSING" ]; then
        say "apt packages:$MISSING"
        run sudo apt-get update
        # shellcheck disable=SC2086
        run sudo apt-get install -y $MISSING
    else
        skip "apt packages"
    fi
else
    say "no dpkg-query; install these yourself if missing: $PKGS"
fi

# --- ESP-IDF -----------------------------------------------------------------
TAG="v$ESP_IDF_VERSION"
if [ -d "$IDF_DIR/.git" ]; then
    HAVE=$(git -C "$IDF_DIR" describe --tags --abbrev=0 2>/dev/null || echo unknown)
    if [ "$HAVE" = "$TAG" ]; then
        skip "esp-idf $TAG at $IDF_DIR"
    else
        say "esp-idf $HAVE -> $TAG"
        run git -C "$IDF_DIR" fetch --depth 1 origin "refs/tags/$TAG:refs/tags/$TAG"
        run git -C "$IDF_DIR" checkout "$TAG"
        run git -C "$IDF_DIR" submodule update --init --depth 1 --recursive
    fi
else
    say "cloning esp-idf $TAG into $IDF_DIR"
    run mkdir -p "$(dirname "$IDF_DIR")"
    run git clone --branch "$TAG" --depth 1 --recursive \
        https://github.com/espressif/esp-idf.git "$IDF_DIR"
fi

say "esp-idf toolchain for $ESP_IDF_TARGET"
run "$IDF_DIR/install.sh" "$ESP_IDF_TARGET"

# QEMU is an idf_tools package, not a distro one -- the distro
# qemu-system-xtensa has no esp32 machine and cannot run this firmware.
say "espressif qemu ($QEMU_TOOL)"
run "$IDF_DIR/tools/idf_tools.py" install "$QEMU_TOOL"

# --- python ------------------------------------------------------------------
if [ -x "$VENV/bin/python" ]; then
    skip "venv at $VENV"
else
    say "venv at $VENV"
    run "$PYTHON_BIN" -m venv "$VENV"
fi
say "python requirements (pinned)"
run "$VENV/bin/pip" install --quiet --disable-pip-version-check -r "$ROOT/requirements.txt"

# --- verify ------------------------------------------------------------------
if [ "$DRY" -eq 1 ]; then
    printf '\n--dry-run: nothing was changed.\n'
    exit 0
fi

cat <<TXT

Installed. Every shell that builds firmware needs the IDF environment:

    . $IDF_DIR/export.sh
    . $VENV/bin/activate

TXT

say "check-env"
exec "$VENV/bin/python" "$ROOT/scripts/wizard/cli.py" check-env
