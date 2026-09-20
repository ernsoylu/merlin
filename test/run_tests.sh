#!/bin/sh
# Host-side unit tests for the generated C. No hardware, no framework - gcc and
# assert(). Hardware validation is deferred to phase 2, so until then these are
# the only evidence any arithmetic is right.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

CFLAGS="-std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined"
INC="-I$ROOT/v01-reference/components/Std/include -I$ROOT/v01-reference/components/Mcal_Dio/include -I$ROOT/v01-reference/components/Mcal_Uart/include -I$ROOT/v01-reference/components/Mcal_Wdg/include -I$ROOT/v01-reference/components/Mcal_Gpt/include -I$ROOT/v01-reference/components/Mcal_Radio/include -I$ROOT/v01-reference/components/Mcal_Wlan/include -I$ROOT/v01-reference/components/Mcal_Pwm/include -I$ROOT/v01-reference/components/Mcal_Adc/include -I$ROOT/v01-reference/components/Mcal_Spi/include -I$ROOT/v01-reference/components/Mcal_I2c/include -I$ROOT/v01-reference/components/LibPid/include -I$ROOT/v01-reference/components/Drv_Bme280/include -I$ROOT/v01-reference/components/Drv_Ssd1306/include -I$ROOT/v01-reference/components/Rte/include -I$ROOT/v01-reference/components/Os/include -I$ROOT/v01-reference/components/Hm/include -I$ROOT/v01-reference/components/Log/include -I$ROOT/v01-reference/components/Det/include -I$ROOT/v01-reference/components/Swc_ClimateController/include -I$ROOT/v01-reference/components/Swc_DisplayDemo/include -I$ROOT/v01-reference/components/IoHwAb/include -I$ROOT/v01-reference/components/EcuM/include"
INC="$INC -I$ROOT/v01-reference/components/Mcal_Port/include"
LAYER_INC="-I$ROOT/v01-reference/components/Std/include -I$ROOT/v01-reference/components/Rte/include"

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
run bme280 "$ROOT/v01-reference/components/Drv_Bme280/src/bme280.c" "$ROOT/v01-reference/components/Drv_Bme280/src/bme280_calc.c" "$ROOT/test/host/test_bme280.c"
run rte "$ROOT/v01-reference/components/Rte/src/rte_sample.c" "$ROOT/test/host/test_rte.c"
run os_wrapper "$ROOT/v01-reference/components/Os/src/os_wrapper.c" "$ROOT/test/host/test_os_wrapper.c"
run hm_debounce "$ROOT/v01-reference/components/Hm/src/hm_debounce.c" "$ROOT/test/host/test_hm_debounce.c"
run mcal_result "$ROOT/test/host/test_mcal_result.c"
run mcal_port "$ROOT/v01-reference/components/Mcal_Port/src/mcal_port.c" "$ROOT/test/host/test_mcal_port.c"
run mcal_i2c "$ROOT/v01-reference/components/Mcal_I2c/src/mcal_i2c.c" "$ROOT/test/host/test_mcal_i2c.c"
run mcal_dio "$ROOT/v01-reference/components/Mcal_Dio/src/mcal_dio.c" "$ROOT/test/host/test_mcal_dio.c"
run mcal_uart "$ROOT/v01-reference/components/Mcal_Uart/src/mcal_uart.c" "$ROOT/test/host/test_mcal_uart.c"
run mcal_wdg "$ROOT/v01-reference/components/Mcal_Wdg/src/mcal_wdg.c" "$ROOT/test/host/test_mcal_wdg.c"
run mcal_gpt "$ROOT/v01-reference/components/Mcal_Gpt/src/mcal_gpt.c" "$ROOT/test/host/test_mcal_gpt.c"
run mcal_radio "$ROOT/v01-reference/components/Mcal_Radio/src/mcal_radio.c" "$ROOT/test/host/test_mcal_radio.c"
run mcal_wlan "$ROOT/v01-reference/components/Mcal_Wlan/src/mcal_wlan.c" "$ROOT/test/host/test_mcal_wlan.c"
run mcal_pwm "$ROOT/v01-reference/components/Mcal_Pwm/src/mcal_pwm.c" "$ROOT/test/host/test_mcal_pwm.c"
run mcal_adc "$ROOT/v01-reference/components/Mcal_Adc/src/mcal_adc.c" "$ROOT/test/host/test_mcal_adc.c"
run mcal_spi "$ROOT/v01-reference/components/Mcal_Spi/src/mcal_spi.c" "$ROOT/test/host/test_mcal_spi.c"
run ssd1306_frame "$ROOT/v01-reference/components/Drv_Ssd1306/src/ssd1306_frame.c" "$ROOT/test/host/test_ssd1306_frame.c"
run ssd1306 "$ROOT/v01-reference/components/Drv_Ssd1306/src/ssd1306_frame.c" "$ROOT/test/host/test_ssd1306.c"
run log_det "$ROOT/v01-reference/components/Log/src/log_ring.c" "$ROOT/v01-reference/components/Det/src/det.c" "$ROOT/test/host/test_log_det.c"
run climate_io "$ROOT/v01-reference/components/LibPid/src/pid.c" "$ROOT/v01-reference/components/Swc_ClimateController/src/climate_controller.c" "$ROOT/v01-reference/components/IoHwAb/src/iohwab_fan.c" "$ROOT/test/host/test_climate_io.c"
run ecum "$ROOT/v01-reference/components/EcuM/src/ecum.c" "$ROOT/test/host/test_ecum.c"
run display_demo "$ROOT/v01-reference/components/Swc_DisplayDemo/src/display_demo.c" "$ROOT/v01-reference/components/Drv_Ssd1306/src/ssd1306_frame.c" "$ROOT/test/host/test_display_demo.c"

# Prove the allowed side first: an SWC-shaped source compiles without driver
# include paths.
gcc $CFLAGS $LAYER_INC -c "$ROOT/v01-reference/test/layering/pos_swc_no_driver.c" -o "$OUT/positive.o"

# The forbidden include itself must cause rejection. A successful compile, or
# an unrelated diagnostic, means the layer rule is not being tested.
if gcc $CFLAGS $LAYER_INC -c "$ROOT/v01-reference/test/layering/neg_swc_includes_driver.c" -o "$OUT/negative.o" 2>"$OUT/layering.stderr"; then
    echo "layering: negative compile unexpectedly succeeded" >&2
    exit 1
fi
if ! grep -Fq 'bme280.h: No such file or directory' "$OUT/layering.stderr"; then
    echo "layering: rejected for an unexpected reason" >&2
    cat "$OUT/layering.stderr" >&2
    exit 1
fi
if gcc $CFLAGS $LAYER_INC -c "$ROOT/v01-reference/test/layering/neg_swc_includes_oled.c" -o "$OUT/negative-oled.o" 2>"$OUT/layering-oled.stderr"; then
    echo "layering: OLED negative compile unexpectedly succeeded" >&2
    exit 1
fi
if ! grep -Fq 'ssd1306_frame.h: No such file or directory' "$OUT/layering-oled.stderr"; then
    echo "layering: OLED rejected for an unexpected reason" >&2
    cat "$OUT/layering-oled.stderr" >&2
    exit 1
fi
echo "  layering                 rejected as expected"

echo "all host tests passed"
