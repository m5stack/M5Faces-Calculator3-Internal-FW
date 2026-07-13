#ifndef __PT_TASK_H__
#define __PT_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "i2c.h"

#define PT_USE_TIMER
#define PT_USE_SEM

#include "pt.h"

#define PT_SECOND(sec)  ((sec) * 1000)
#define PT_MILLIS_SECOND(millis_sec)  (millis_sec)

extern struct pt main_pt;

void init_pt_task(void);
void main_scheduler(struct pt *pt);

#ifdef __cplusplus
}
#endif

#endif /* __I2C_H__ */

