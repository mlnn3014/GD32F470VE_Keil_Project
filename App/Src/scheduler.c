#include "mcu_cmic_gd32f470vet6.h"

typedef struct {
    void (*run)(void);
    uint32_t period_ms;
    uint32_t last_ms;
} task_t;

static task_t tasks[] =
{
    {btn_task,  5U,   0U},
    {adc_task,  100U, 0U},
    {dac_task,  100U, 0U},
    {oled_task, 10U,  0U},
    {uart_task, 5U,  0U},
    {rtc_task,  50U, 0U}
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
            tasks[i].last_ms = now;
            tasks[i].run();
        }
    }
}
