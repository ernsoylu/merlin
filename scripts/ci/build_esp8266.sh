#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
SDK=${ESP8266_RTOS_SDK:-$HOME/esp/ESP8266_RTOS_SDK}
TOOLCHAIN=${ESP8266_TOOLCHAIN:-$HOME/esp/xtensa-lx106-elf}
PYTHON_ENV=${ESP8266_PYTHON_ENV_PATH:-$HOME/esp/esp8266-venv}

[ -f "$SDK/export.sh" ] || { echo "ESP8266 RTOS SDK not found: $SDK" >&2; exit 2; }
[ -x "$TOOLCHAIN/bin/xtensa-lx106-elf-gcc" ] || { echo "ESP8266 toolchain not found: $TOOLCHAIN" >&2; exit 2; }
[ -x "$PYTHON_ENV/bin/python" ] || { echo "ESP8266 Python environment not found: $PYTHON_ENV" >&2; exit 2; }

export IDF_PATH=$SDK
export IDF_PYTHON_ENV_PATH=$PYTHON_ENV
export PATH=$PYTHON_ENV/bin:$TOOLCHAIN/bin:$PATH
make -C "$ROOT/v01-hw364a-reference"
