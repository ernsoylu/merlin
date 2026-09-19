#include <assert.h>
#include "pid.h"

int main(void)
{
    Pid_StateType pid = {.kp = 1.0f, .ki = 2.0f, .kd = 0.0f, .outMin = -10.0f, .outMax = 10.0f};
    Pid_Init(&pid, -1.0f, 1.0f);
    assert(Pid_Update(&pid, 1.0f, 0.0f, 0.5f) == 2.0f);
    assert(Pid_Update(&pid, 100.0f, 0.0f, 1.0f) == 10.0f);
    assert(pid.integral == 1.0f);
    return 0;
}
