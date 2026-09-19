#include "pid.h"

static float clamp(float value, float low, float high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

void Pid_Init(Pid_StateType *state, float integralMin, float integralMax)
{
    state->integral = 0.0f;
    state->previousError = 0.0f;
    state->integralMin = integralMin;
    state->integralMax = integralMax;
    state->initialized = 0;
}

float Pid_Update(Pid_StateType *state, float setpoint, float measurement, float dtSeconds)
{
    const float error = setpoint - measurement;
    const float derivative = state->initialized && dtSeconds > 0.0f
        ? (error - state->previousError) / dtSeconds
        : 0.0f;

    if (dtSeconds > 0.0f) {
        state->integral = clamp(state->integral + error * dtSeconds,
                                state->integralMin, state->integralMax);
    }
    state->previousError = error;
    state->initialized = 1;
    return clamp(state->kp * error + state->ki * state->integral + state->kd * derivative,
                 state->outMin, state->outMax);
}
