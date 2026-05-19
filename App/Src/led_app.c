#include "led_app.h"
#include "gd32f4xx.h"

/* LED 模式 */
typedef enum
{
    LED_MODE_STATIC = 0U,  // 静态显示
    LED_MODE_BLINK          // 闪烁模式
} led_mode_t;

/* LED 运行状态上下文 */
typedef struct
{
    led_mode_t mode;           // 当前模式

    uint8_t state;             // 当前 LED 状态（0:灭, 1:亮）
    uint8_t saved_state;       // 闪烁开始前保存的状态

    uint16_t blink_interval_ms;// 闪烁间隔
    uint16_t elapsed_ms;       // 已经过的时间

    uint32_t blink_cycles_left;// 闪烁剩余次数
    uint8_t continuous;        // 是否连续闪烁（1:连续, 0:有限次数）

} led_context_t;

/* LED 运行表 */
static led_context_t led_ctx[LED_COUNT] =
{
    [0] = {.state = 1U},
    [1] = {.state = 0U},
    [2] = {.state = 1U},
    [3] = {.state = 0U},
    [4] = {.state = 1U},
    [5] = {.state = 0U},
};

/* 检查 LED ID 是否有效 */
static uint8_t led_is_valid(led_id_t led)
{
    return ((uint32_t)led < (uint32_t)LED_COUNT);
}

/* 进入临界区（关闭全局中断） */
static inline uint32_t led_lock(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    return primask;
}

/* 退出临界区（恢复中断状态） */
static inline void led_unlock(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}

/* 开始 LED 闪烁 */
static void led_start_blink(led_id_t led,
                            uint16_t interval_ms,
                            uint32_t cycles,
                            uint8_t continuous)
{
    led_context_t *ctx = &led_ctx[led];

    ctx->mode = LED_MODE_BLINK;

    /* 保存闪烁前状态 */
    ctx->saved_state = ctx->state;

    /* 立即切换一次状态开始闪烁 */
    ctx->state ^= 1U;

    ctx->blink_interval_ms = interval_ms;
    ctx->elapsed_ms = 0U;

    ctx->blink_cycles_left = cycles;
    ctx->continuous = continuous;

    /* 设置硬件 LED */
    led_set(led, ctx->state);
}

/* 初始化 LED 应用 */
void led_app_init(void)
{
    led_init();

    for (uint8_t i = 0U; i < LED_COUNT; i++)
    {
        led_ctx[i].mode = LED_MODE_STATIC;

        /* 初始化 LED 硬件状态 */
        led_set((led_id_t)i, led_ctx[i].state);
    }
}

/* 设置 LED 状态 */
void led_app_set(led_id_t led, uint8_t on)
{
    uint32_t primask;

    if (!led_is_valid(led))
    {
        return;
    }

    primask = led_lock();

		led_context_t *ctx = &led_ctx[led];
    ctx->mode = LED_MODE_STATIC;
    led_ctx[led].state = (on != 0U);

    led_unlock(primask);

    led_set(led, led_ctx[led].state);
}

/* 切换 LED 状态 */
void led_app_toggle(led_id_t led)
{
    uint32_t primask;

    if (!led_is_valid(led))
    {
        return;
    }

    primask = led_lock();

    led_ctx[led].mode = LED_MODE_STATIC;
    led_ctx[led].state ^= 1U;

    led_unlock(primask);

    led_set(led, led_ctx[led].state);
}

/* 开始连续闪烁 */
void led_app_blink_on(led_id_t led, uint16_t interval_ms)
{
    uint32_t primask;

    if ((!led_is_valid(led)) || (interval_ms == 0U))
    {
        return;
    }

    primask = led_lock();

    led_start_blink(led, interval_ms, 0U, 1U);

    led_unlock(primask);
}

/* 关闭闪烁并恢复原状态 */
void led_app_blink_off(led_id_t led)
{
    uint32_t primask;

    if (!led_is_valid(led))
    {
        return;
    }

    primask = led_lock();

    led_context_t *ctx = &led_ctx[led];
    ctx->mode = LED_MODE_STATIC;

    /* 恢复闪烁前状态 */
    led_ctx[led].state = led_ctx[led].saved_state;

    led_unlock(primask);

    led_set(led, led_ctx[led].state);
}

/* 闪烁指定次数 */
void led_app_blink_times(led_id_t led, uint16_t times, uint16_t interval_ms)
{
    uint32_t primask;

    if ((!led_is_valid(led)) || (times == 0U) || (interval_ms == 0U))
    {
        return;
    }

    primask = led_lock();

    led_start_blink(led, interval_ms, times, 0U);

    led_unlock(primask);
}

/* 1ms 定时调用，处理 LED 闪烁 */
void led_app_tick_1ms(void)
{
    for (uint8_t i = 0U; i < LED_COUNT; i++)
    {
        led_context_t *ctx = &led_ctx[i];

        if (ctx->mode != LED_MODE_BLINK)
        {
            continue;
        }

        /* 更新时间，判断是否达到闪烁间隔 */
        if (++ctx->elapsed_ms < ctx->blink_interval_ms)
        {
            continue;
        }

        ctx->elapsed_ms = 0U;

        /* 切换 LED 状态 */
        ctx->state ^= 1U;

        led_set((led_id_t)i, ctx->state);

        /* 连续闪烁，不处理剩余次数 */
        if (ctx->continuous)
        {
            continue;
        }

        /* 一个闪烁周期：ON->OFF */
        if (ctx->state == ctx->saved_state)
        {
            if (ctx->blink_cycles_left > 0U)
            {
                ctx->blink_cycles_left--;
            }

            /* 闪烁结束，恢复静态模式 */
            if (ctx->blink_cycles_left == 0U)
            {
                ctx->mode = LED_MODE_STATIC;
            }
        }
    }
}
