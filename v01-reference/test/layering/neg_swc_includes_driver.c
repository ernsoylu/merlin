/* Layering control: an SWC must not reach the driver layer. The include itself
   must fail, so nothing below it is ever compiled. */
#include "bme280.h"

int main(void)
{
    return 0;
}
