#include "main.h"

typedef struct {
    uint8_t active;
    uint8_t forever;
    uint8_t restore_state;
    uint8_t output_state;
    uint16_t interval_ms;
    uint16_t elapsed_ms;
    uint32_t remaining_toggles;
} led_blink_t;

static volatile uint8_t led_states[LED_COUNT] = {1U, 0U, 1U, 0U, 1U, 0U};
static volatile led_blink_t led_blinks[LED_COUNT];

static uint8_t led_app_is_valid(led_id_t led)
{
    return ((uint32_t)led < (uint32_t)LED_COUNT);
}

static uint32_t led_app_lock(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void led_app_unlock(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}

void led_app_init(void)
{
    led_init();

    for (uint8_t i = 0U; i < (uint8_t)LED_COUNT; i++)
    {
        led_blinks[i].active = 0U;
        led_set((led_id_t)i, led_states[i]);
    }
}

void led_app_set(led_id_t led, uint8_t on)
{
    uint32_t primask;

    if (led_app_is_valid(led) == 0U)
    {
        return;
    }

    primask = led_app_lock();
    led_blinks[led].active = 0U;
    led_states[led] = (on != 0U) ? 1U : 0U;
    led_set(led, led_states[led]);
    led_app_unlock(primask);
}

void led_app_toggle(led_id_t led)
{
    uint32_t primask;

    if (led_app_is_valid(led) == 0U)
    {
        return;
    }

    primask = led_app_lock();
    led_blinks[led].active = 0U;
    led_states[led] = (led_states[led] == 0U) ? 1U : 0U;
    led_set(led, led_states[led]);
    led_app_unlock(primask);
}

void led_app_blink_on(led_id_t led, uint16_t interval_ms)
{
    uint32_t primask;
    led_blink_t *blink;

    if ((led_app_is_valid(led) == 0U) || (interval_ms == 0U))
    {
        return;
    }

    primask = led_app_lock();
    blink = (led_blink_t *)&led_blinks[led];

    blink->active = 1U;
    blink->forever = 1U;
    blink->restore_state = led_states[led];
    blink->output_state = (blink->restore_state == 0U) ? 1U : 0U;
    blink->interval_ms = interval_ms;
    blink->elapsed_ms = 0U;
    blink->remaining_toggles = 0U;
    led_set(led, blink->output_state);

    led_app_unlock(primask);
}

void led_app_blink_off(led_id_t led)
{
    uint32_t primask;

    if (led_app_is_valid(led) == 0U)
    {
        return;
    }

    primask = led_app_lock();
    led_blinks[led].active = 0U;
    led_set(led, led_states[led]);
    led_app_unlock(primask);
}

void led_app_blink_toggle(led_id_t led, uint16_t interval_ms)
{
    uint32_t primask;
    led_blink_t *blink;

    if ((led_app_is_valid(led) == 0U) || (interval_ms == 0U))
    {
        return;
    }

    primask = led_app_lock();

    blink = (led_blink_t *)&led_blinks[led];
    if (blink->active != 0U)
    {
        blink->active = 0U;
        led_set(led, led_states[led]);
        led_app_unlock(primask);
        return;
    }

    blink->active = 1U;
    blink->forever = 1U;
    blink->restore_state = led_states[led];
    blink->output_state = (blink->restore_state == 0U) ? 1U : 0U;
    blink->interval_ms = interval_ms;
    blink->elapsed_ms = 0U;
    blink->remaining_toggles = 0U;
    led_set(led, blink->output_state);

    led_app_unlock(primask);
}

void led_app_blink_times(led_id_t led, uint16_t times, uint16_t interval_ms)
{
    uint32_t primask;
    led_blink_t *blink;

    if ((led_app_is_valid(led) == 0U) || (times == 0U) || (interval_ms == 0U))
    {
        return;
    }

    primask = led_app_lock();

    blink = (led_blink_t *)&led_blinks[led];
    blink->active = 1U;
    blink->forever = 0U;
    blink->restore_state = led_states[led];
    blink->output_state = (blink->restore_state == 0U) ? 1U : 0U;
    blink->interval_ms = interval_ms;
    blink->elapsed_ms = 0U;
    blink->remaining_toggles = ((uint32_t)times * 2U) - 1U;
    led_set(led, blink->output_state);

    led_app_unlock(primask);
}

void led_app_tick_1ms(void)
{
    for (uint8_t i = 0U; i < (uint8_t)LED_COUNT; i++)
    {
        volatile led_blink_t *blink = &led_blinks[i];

        if (blink->active == 0U)
        {
            continue;
        }

        blink->elapsed_ms++;
        if (blink->elapsed_ms < blink->interval_ms)
        {
            continue;
        }

        blink->elapsed_ms = 0U;

        blink->output_state = (blink->output_state == 0U) ? 1U : 0U;
        led_set((led_id_t)i, blink->output_state);

        if (blink->forever != 0U)
        {
            continue;
        }

        blink->remaining_toggles--;
        if (blink->remaining_toggles == 0U)
        {
            blink->active = 0U;
            led_set((led_id_t)i, blink->restore_state);
        }
    }
}
