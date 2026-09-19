#include <assert.h>
#include "Std_Types.h"

int main(void)
{
    assert(MCAL_TIMEOUT != MCAL_NACK);
    assert(MCAL_TIMEOUT != MCAL_OK);
    return 0;
}
