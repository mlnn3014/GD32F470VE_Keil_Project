#include "btn_app.h"
#include "button.h"
#include "led_app.h"
#include "systick.h"

static button_t buttons[BTN_COUNT] = {
    {.id = BTN_1},
    {.id = BTN_2},
    {.id = BTN_3},
    {.id = BTN_4},
    {.id = BTN_5},
    {.id = BTN_6},
    {.id = BTN_7},
};

static uint8_t btn_read_state(button_t *button)
{
    if (button == 0)
    {
        return 0U;
    }

    return btn_read((btn_id_t)button->id);
}

static void btn_toggle_led(uint8_t id)
{
    switch ((btn_id_t)id)
    {
    case BTN_1:
        led_app_blink_toggle(LED_1, 100U);
        break;
    case BTN_2:
        led_app_toggle(LED_2);
        break;
    case BTN_3:
        led_app_toggle(LED_3);
        break;
    case BTN_4:
        led_app_toggle(LED_4);
        break;
    case BTN_5:
        led_app_toggle(LED_5);
        break;
    case BTN_6:
    case BTN_7:
        led_app_toggle(LED_6);
        break;
    default:
        break;
    }
}

static void btn_event(button_t *button, button_event_t event)
{
    if (button == 0)
    {
        return;
    }

    switch (event)
    {
    case BUTTON_EVT_PRESS:
        break;
    case BUTTON_EVT_RELEASE:
        break;
    case BUTTON_EVT_CLICK:
        btn_toggle_led(button->id);
        break;
    case BUTTON_EVT_LONG_PRESS:
        if (button->id == BTN_2)
        {
            led_app_blink_toggle(LED_2, 100U);
        }
        break;
    default:
        break;
    }
}

void btn_app_init(void)
{
    btn_init();
    button_init(buttons, BTN_COUNT, btn_read_state, btn_event);
}

void btn_task(void)
{
    button_scan(systick_get_ms());
}
