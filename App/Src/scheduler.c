#include "scheduler.h"

#include "adc_app.h"
#include "btn_app.h"
#include "dac_app.h"
#include "gd30_app.h"
#include "oled_app.h"
#include "rtc_app.h"
#include "systick.h"
#include "usart_app.h"

/* 任务结构体 */
typedef struct {
    void (*run)(void);   // 任务函数指针
    uint32_t period_ms;  // 任务运行间隔（毫秒）
    uint32_t last_ms;    // 上次运行时间记录（tick）
} task_t;

/* 任务列表：按执行顺序排列 */
static task_t tasks[] =
{
    {uart_task, 5U,   0U},  // 调试串口任务
    {btn_task,  5U,   0U},  // 按键扫描任务
    {gd30_task, 1U,   0U},  // GD30传感器采样任务
    {rtc_task,  50U,  0U},  // RTC时钟读取任务
    {adc_task,  100U, 0U},  // 板载电位器采样任务
    {dac_task,  100U, 0U},  // DAC输出任务
    {oled_task, 10U,  0U}   // OLED显示更新任务
};

/* 任务数量 */
static const uint32_t task_count = sizeof(tasks) / sizeof(tasks[0]);

/* 调度器初始化 */
void scheduler_init(void)
{
    uint32_t now_ms = systick_get_ms(); // 获取当前系统tick（毫秒）

    /* 初始化每个任务的上次运行时间 */
    for (uint32_t i = 0U; i < task_count; i++)
    {
        tasks[i].last_ms = now_ms; // 设置初始上次运行时间
    }
}

/* 调度器运行函数（在主循环中反复调用） */
void scheduler_run(void)
{
    uint32_t now_ms = systick_get_ms(); // 获取当前系统tick

    for (uint32_t i = 0U; i < task_count; i++)
    {
        /* 如果任务达到运行时间，进入循环处理 */
        while ((uint32_t)(now_ms - tasks[i].last_ms) >= tasks[i].period_ms)
        {
            /* 累加上次运行时间，保证任务周期稳定 */
            tasks[i].last_ms += tasks[i].period_ms;

            /* 如果任务落后太多（执行时间超过周期），直接跳过多余周期，避免无限追赶 */
            if ((uint32_t)(now_ms - tasks[i].last_ms) >= tasks[i].period_ms)
            {
                tasks[i].last_ms = now_ms; // 将上次运行时间更新为当前时间
            }

            tasks[i].run(); // 执行任务函数
        }
    }
}
