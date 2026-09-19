#include <stdio.h>

#include "EcuM.h"

void EcuM_Startup(void)
{
    puts("{\"instance\":\"ambientSensor\",\"health\":\"INITIAL\",\"sequence\":0}");
    puts("{\"instance\":\"enclosureSensor\",\"health\":\"INITIAL\",\"sequence\":0}");
}
