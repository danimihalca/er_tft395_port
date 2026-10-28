/*
 * ER-TFT3.95-1 on ESP32 (Zephyr): LT7680 display + GT911 touch.
 * - runs the NV3052 panel init (bit-banged SPI)
 * - draws colour bars
 * - draws a 16x16 square wherever the screen is touched
 */
#include "nv3052.h"
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define W 720
#define H 720
#define BAND 60 /* rows per colour-bar write */

static const struct device *const disp =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static const struct device *const touch = DEVICE_DT_GET(DT_NODELABEL(gt911));

static uint16_t line_buf[W * BAND]; /* 86 kB, RGB565 */
static uint16_t dot_buf[16 * 16];

static const uint16_t bars[] = {0xF800, 0xFFE0, 0x07E0, 0x07FF,
                                0x001F, 0xF81F, 0xFFFF, 0x0000};

static void draw_bars(void) {
  struct display_buffer_descriptor d = {
      .buf_size = sizeof(line_buf),
      .width = W,
      .height = BAND,
      .pitch = W,
  };

  for (int y = 0, b = 0; y < H; y += BAND, b++) {
    for (int i = 0; i < W * BAND; i++) {
      line_buf[i] =
          bars[(b + (i % W) / (W / ARRAY_SIZE(bars))) % ARRAY_SIZE(bars)];
    }
    display_write(disp, 0, y, &d, line_buf);
  }
}

/* ---- touch ---- */
static int tx, ty;
static bool tdown;
K_SEM_DEFINE(touch_sem, 0, 1);

static void input_cb(struct input_event *evt, void *user_data) {
  switch (evt->code) {
  case INPUT_ABS_X:
    tx = evt->value;
    break;
  case INPUT_ABS_Y:
    ty = evt->value;
    break;
  case INPUT_BTN_TOUCH:
    tdown = evt->value;
    break;
  }
  if (evt->sync) {
    k_sem_give(&touch_sem);
  }
}
INPUT_CALLBACK_DEFINE(touch, input_cb, NULL);

#include <zephyr/drivers/gpio.h>

int main(void) {
  LOG_INF("Hello\n");
  printf("Main\n");
  if (!device_is_ready(disp) || !device_is_ready(touch)) {
    LOG_ERR("display or touch not ready");
    return 0;
  }

#define BL_PIN 26
  const struct gpio_dt_spec bl = {.port = DEVICE_DT_GET(DT_NODELABEL(gpio0)),
                                  .pin = BL_PIN,
                                  .dt_flags = GPIO_ACTIVE_HIGH};
  gpio_pin_configure_dt(&bl, GPIO_OUTPUT_ACTIVE);

  int ret = nv3052_init();
  if (ret) {
    LOG_ERR("nv3052 init failed: %d", ret);
    return 0;
  }

  struct display_capabilities caps;
  display_get_capabilities(disp, &caps);
  LOG_INF("display %ux%u", caps.x_resolution, caps.y_resolution);
  if (!(caps.current_pixel_format & PIXEL_FORMAT_RGB_565)) {
    display_set_pixel_format(disp, PIXEL_FORMAT_RGB_565);
  }

  display_blanking_off(disp);
  draw_bars();

  for (int i = 0; i < ARRAY_SIZE(dot_buf); i++) {
    dot_buf[i] = 0xFFFF;
  }
  struct display_buffer_descriptor dd = {
      .buf_size = sizeof(dot_buf),
      .width = 16,
      .height = 16,
      .pitch = 16,
  };

  while (1) {
    k_sem_take(&touch_sem, K_FOREVER);
    if (tdown) {
      int x = CLAMP(tx - 8, 0, W - 16);
      int y = CLAMP(ty - 8, 0, H - 16);
      display_write(disp, x, y, &dd, dot_buf);
    }
  }
}
