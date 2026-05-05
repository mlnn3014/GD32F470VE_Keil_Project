#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "stdint.h"

#ifndef SCHEDULER_STATS_ENABLE
#define SCHEDULER_STATS_ENABLE 0U
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    uint32_t period_ms;
    uint32_t run_count;
    uint32_t last_cost_ms;
    uint32_t max_cost_ms;
    uint32_t overrun_count;
} scheduler_stats_t;

void scheduler_init(void);
void scheduler_run(void);
uint32_t scheduler_task_count(void);
int scheduler_get_stats(uint32_t index, scheduler_stats_t *stats);
void scheduler_clear_stats(void);

#ifdef __cplusplus
}
#endif

#endif
