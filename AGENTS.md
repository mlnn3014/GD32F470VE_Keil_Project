# Codex Project Instructions

## Engineering Preferences

- Keep code simple, direct, and easy to continue programming against. Do not add abstraction unless it clearly reduces real complexity.
- Prefer short, readable names over overly formal names. Names should be clear but not awkward.
- Make changes in small, focused steps and preserve existing behavior unless the task explicitly asks for behavior changes.
- Prefer direct app-layer code for this project. Do not introduce callback/event-dispatch layers unless the user explicitly asks for them.
- After changing source layout or adding files, update the Keil project include paths and source file references.
- Keep VSCode EIDE configuration in sync with Keil when changing include paths, source paths, or project structure.
- Run or request Keil build verification when possible. Treat unrelated existing build errors separately from the current task.
- Keil is installed under `C:\Users\ASUS\AppData\Local\Keil_v5`, but `UV4.exe` and `armcc.exe` may not be in the shell `PATH`. When building from Codex, prefer the absolute UV4 path:
  - `C:\Users\ASUS\AppData\Local\Keil_v5\UV4\UV4.exe`
  - Project: `Project\GD32F470VE.uvprojx`
  - Target: `GD32F470VET6_Project`
  - Write build logs under `Project\output\codex_*.log`
- Keil generated files under `Project/output`, `Project/list`, and user GUI state such as `Project/*.uvguix.*` should not be committed unless the user explicitly asks for build artifacts.

## BSP Style

- BSP modules should isolate hardware details while exposing simple interfaces.
- Store BSP headers in `Bsp/Inc` and BSP source files in `Bsp/Src`.
- Prefer GD32 standard peripheral library calls over control-style macros for hardware operations.
- Avoid public hardware-control macros such as `LED1_SET` or `LED_WRITE`; use functions instead.

## LED Rules

- Board LEDs are active-high: GPIO `SET` turns an LED on, GPIO `RESET` turns it off.
- Keep the LED API simple and convenient for application code:
  - `led_init()`
  - `led_on(LED_1)`
  - `led_off(LED_1)`
  - `led_set(LED_1, 1U)`
  - `led_toggle(LED_1)`
- Use simple LED identifiers: `LED_1`, `LED_2`, `LED_3`, `LED_4`, `LED_5`, `LED_6`, `LED_COUNT`.
- LED app logic should stay straightforward. Avoid unnecessary state masks, cache layers, or extra wrapper functions unless there is a clear need.

## Button Rules

- Keep `btn_bsp` limited to hardware initialization and raw button reads.
- Keep button business behavior in `btn_app.c` for now. The user prefers directly editing button-to-action mappings there over routing through callbacks in `main.c`.
- `btn_app_init()` should initialize the button BSP internally, then initialize the shared button scanner.
- Preserve the current button behavior unless explicitly asked to change it:
  - `BTN_1` click toggles `LED_1` blinking at 100 ms.
  - `BTN_2` click toggles `LED_2`.
  - `BTN_2` long press toggles `LED_2` blinking at 100 ms.
  - `BTN_3`, `BTN_4`, and `BTN_5` clicks toggle `LED_3`, `LED_4`, and `LED_5`.
  - `BTN_6` and `BTN_7` clicks toggle `LED_6`.

## OLED Rules

- Current stable OLED direction is a 128x32 SSD1306 I2C driver split into:
  - `Compenents/oled`: display buffer, fonts, drawing, dirty tracking, and text APIs.
  - `Bsp/Inc/oled_bsp.h` and `Bsp/Src/oled_bsp.c`: GD32 I2C0 + DMA0 CH6 hardware transfer details.
- Do not reintroduce `libdriver/ssd1306`; the project intentionally removed that dependency.
- Do not switch back to a pure interrupt-driven async OLED DMA state machine without explicit user approval. Earlier pure async attempts caused unstable display behavior over time.
- Prefer the stable dirty-page + batched I2C DMA refresh model. Drawing APIs update GRAM and mark dirty; `oled_service()` or `oled_update()` submits changes.
- App code should prefer row/column text APIs instead of pixel math:
  - `oled_text_show(font, row, col, cols, str)`
  - `oled_text_printf(font, row, col, cols, format, ...)`
  - `cols == 0U` means clear/show from the current column to the logical row end.
- Keep the old two-size font behavior:
  - `OLED_FONT_8`: 6x8 characters, rows `0..3`, columns `0..20`.
  - `OLED_FONT_16`: 8x16 characters, rows `0..1`, columns `0..15`.
