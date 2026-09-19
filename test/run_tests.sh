#!/bin/sh
# Host-side unit tests for the generated C. No hardware, no framework - gcc and
# assert(). Hardware validation is deferred to phase 2, so until then these are
# the only evidence any arithmetic is right.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

CFLAGS="-std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined"
INC="-I$ROOT/v01-reference/components/Std/include -I$ROOT/v01-reference/components/LibPid/include -I$ROOT/v01-reference/components/Drv_Bme280/include -I$ROOT/v01-reference/components/Rte/include -I$ROOT/v01-reference/components/Os/include -I$ROOT/v01-reference/components/Hm/include"

# One binary per unit: a failure names the unit, and one unit's sanitizer
# finding cannot mask another's.
run() {
    name=$1
    shift
    printf '  %-24s' "$name"
    # shellcheck disable=SC2086
    gcc $CFLAGS $INC -o "$OUT/$name" "$@" -lm
    "$OUT/$name"
    echo ok
}

run pid "$ROOT/v01-reference/components/LibPid/src/pid.c" "$ROOT/test/host/test_pid.c"
run bme280_calc "$ROOT/v01-reference/components/Drv_Bme280/src/bme280_calc.c" "$ROOT/test/host/test_bme280_calc.c"
run rte "$ROOT/v01-reference/components/Rte/src/rte_sample.c" "$ROOT/test/host/test_rte.c"
run os_wrapper "$ROOT/v01-reference/components/Os/src/os_wrapper.c" "$ROOT/test/host/test_os_wrapper.c"
run hm_debounce "$ROOT/v01-reference/components/Hm/src/hm_debounce.c" "$ROOT/test/host/test_hm_debounce.c"
run mcal_result "$ROOT/test/host/test_mcal_result.c"

# The boundary test is expected to fail. A successful compile means the layer
# rule has been weakened, so turn that into a test failure.
if gcc $CFLAGS $INC -c "$ROOT/v01-reference/test/layering/neg_swc_includes_driver.c" -o "$OUT/negative.o" 2>/dev/null; then
    echo "layering: negative compile unexpectedly succeeded" >&2
    exit 1
fi
echo "  layering                 rejected as expected"

echo "all host tests passed"
