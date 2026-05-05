#include "scheduler.h"

#include "adc_app.h"
#include "btn_app.h"
#include "dac_app.h"
#include "gd30_app.h"
#include "oled_app.h"
#include "rtc_app.h"
#include "systick.h"
#include "usart_app.h"

typedef struct {
    void (*run)(void);
    const char *name;
    uint32_t period_ms;
    uint32_t last_ms;
#if SCHEDULER_STATS_ENABLE
    uint32_t run_count;
    uint32_t last_cost_ms;
    uint32_t max_cost_ms;
    uint32_t overrun_count;
#endif
} task_t;

static task_t tasks[] =
{
    {btn_task,  "btn",  5U,   0U},
    {gd30_task, "gd30", 1U,   0U},
    {adc_task,  "adc",  100U, 0U},
    {dac_task,  "dac",  100U, 0U},
    {oled_task, "oled", 10U,  0U},
    {uart_task, "uart", 5U,   0U},
    {rtc_task,  "rtc",  50U,  0U}
};

static const uint32_t task_count = sizeof(tasks) / sizeof(tasks[0]);

void scheduler_init(void)
{
    uint32_t now = systick_get_ms();

    for (uint32_t i = 0U; i < task_count; i++)
    {
        tasks[i].last_ms = now;
    }
}

void scheduler_run(void)
{
    uint32_t now = systick_get_ms();

    for (uint32_t i = 0U; i < task_count; i++)
    {
        if ((uint32_t)(now - tasks[i].last_ms) >= tasks[i].period_ms)
        {
#if SCHEDULER_STATS_ENABLE
            uint32_t start_ms;
            uint32_t cost_ms;
#endif

            tasks[i].last_ms = now;
#if SCHEDULER_STATS_ENABLE
            start_ms = systick_get_ms();
#endif
            tasks[i].run();
#if SCHEDULER_STATS_ENABLE
            cost_ms = (uint32_t)(systick_get_ms() - start_ms);
            tasks[i].run_count++;
            tasks[i].last_cost_ms = cost_ms;
            if (cost_ms > tasks[i].max_cost_ms) {
                tasks[i].max_cost_ms = cost_ms;
            }
            if (cost_ms > tasks[i].period_ms) {
                tasks[i].overrun_count++;
            }
#endif
        }
    }
}

uint32_t scheduler_task_count(void)
{
    return task_count;
}

int scheduler_get_stats(uint32_t index, scheduler_stats_t *stats)
{
    if ((stats == 0) || (index >= task_count)) {
        return -1;
    }

    stats->name = tasks[index].name;
    stats->period_ms = tasks[index].period_ms;
#if SCHEDULER_STATS_ENABLE
    stats->run_count = tasks[index].run_count;
    stats->last_cost_ms = tasks[index].last_cost_ms;
    stats->max_cost_ms = tasks[index].max_cost_ms;
    stats->overrun_count = tasks[index].overrun_count;
#else
    stats->run_count = 0U;
    stats->last_cost_ms = 0U;
    stats->max_cost_ms = 0U;
    stats->overrun_count = 0U;
#endif

    return 0;
}

void scheduler_clear_stats(void)
{
#if SCHEDULER_STATS_ENABLE
    uint32_t i;

    for (i = 0U; i < task_count; i++) {
        tasks[i].run_count = 0U;
        tasks[i].last_cost_ms = 0U;
        tasks[i].max_cost_ms = 0U;
        tasks[i].overrun_count = 0U;
    }
#endif
}
