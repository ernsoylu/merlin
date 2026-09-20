#ifndef LIB_PID_H
#define LIB_PID_H

typedef struct {
    float kp;
    float ki;
    float kd;
    float outMin;
    float outMax;
    float integralMin;
    float integralMax;
    float integral;
    float previousError;
    int initialized;
} Pid_StateType;

void Pid_Init(Pid_StateType *state, float integralMin, float integralMax);
float Pid_Update(Pid_StateType *state, float setpoint, float measurement, float dtSeconds);

#endif
