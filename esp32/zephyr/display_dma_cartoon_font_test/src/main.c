/*
 * Zephyr port of EastRising's Display_DMA_Cartoon_Font_test (ER-TFT3.95-1,
 * ESP32).
 *
 * Split of responsibilities:
 *   - upstream driver (display API): PLL, SDRAM, panel timing, blanking, pixel
 * blit
 *   - nv3052.c:                      panel controller init over bit-banged
 * 9-bit SPI
 *   - lt7680_hw.c:                   LT7680 2D engine, BTE, flash DMA,
 * CGROM/external fonts
 *
 * The flash image FLASH_bin/PicFlashFont.bin must already be programmed into
 * the serial flash attached to the LT7680 (see the tutorial's "READ ME"); the
 * ESP32 never touches that flash.
 */
#include "lt7680_hw.h"
#include "nv3052.h"
#include "pic80.h"
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define W LCD_XSIZE_TFT
#define H LCD_YSIZE_TFT

/* Flash map (PicFlashFont-Addr.txt) */
#define Picture_1_Addr 0x00000000u
#define Cartoon_Addr 0x003F4800u
#define FLASH_ADDR_16 0x005AA398u
#define FLASH_ADDR_24 0x005EB798u
#define FLASH_ADDR_32 0x0067E498u

#define SIZE_16_NUM 267264u
#define SIZE_24_NUM 601344u
#define SIZE_32_NUM 1069056u

#define MEMORY_ADDR_16 layer5_start_addr
#define MEMORY_ADDR_24 (MEMORY_ADDR_16 + SIZE_16_NUM)
#define MEMORY_ADDR_32 (MEMORY_ADDR_24 + SIZE_24_NUM)

#define BL_PIN 26

static const struct device *const disp =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* GBK string from the tutorial */
static const uint8_t gbk_str[] = {0xD0, 0xF1, 0xC8, 0xD5, 0xB6, 0xAB, 0xB7,
                                  0xBD, 0xBF, 0xC6, 0xBC, 0xBC, 0x00};

static uint16_t pic_px[80 * 80]; /* pic_80x80 converted to native RGB565 for
                                    display_write() */

/* ------------------------------------------------------------------ helpers */

static void layer1_setup(uint32_t canvas_addr) {
  lt_main_window_16bpp();
  lt_main_image(layer1_start_addr, W);
  lt_main_window_xy(0, 0);
  lt_canvas(canvas_addr, W);
  lt_active_xy(0, 0);
  lt_active_wh(W, H);
}

static void clear_black(void) {
  lt_fg(Black);
  lt_fill_rect(0, 0, W - 1, H - 1, Black);
}

/* Blit the 80x80 picture through the upstream display API */
static void show_pic(uint16_t x, uint16_t y) {
  const struct display_buffer_descriptor d = {
      .buf_size = sizeof(pic_px),
      .width = 80,
      .height = 80,
      .pitch = 80,
  };

  display_write(disp, x, y, &d, pic_px);
}

/* ------------------------------------------------------------------- demos */

static void demo_colors(void) {
  static const uint16_t c[] = {Red,    Green,  Blue,  Cyan,
                               Yellow, Purple, Black, White};

  layer1_setup(0);
  for (int i = 0; i < ARRAY_SIZE(c); i++) {
    lt_fill_rect(0, 0, W, H, c[i]);
    k_msleep(1000);
  }
}

/* grow, then erase, using one engine primitive */
#define GROW_ERASE(step_max, color, body)                                      \
  do {                                                                         \
    for (int i = 0; i <= (step_max); i += 8) {                                 \
      lt_fg(color);                                                            \
      body;                                                                    \
      k_msleep(10);                                                            \
    }                                                                          \
    for (int i = 0; i <= (step_max); i += 8) {                                 \
      lt_fg(Black);                                                            \
      body;                                                                    \
      k_msleep(10);                                                            \
    }                                                                          \
    k_msleep(100);                                                             \
  } while (0)

static void demo_shapes(void) {
  layer1_setup(layer1_start_addr);

  clear_black();
  GROW_ERASE(W / 2 - 10, Red, lt_rect(i, i, W - 1 - i, H - 1 - i));

  clear_black();
  GROW_ERASE(W / 2 - 10, Green,
             lt_round_rect(i, i, W - 1 - i, H - 1 - i, 10, 10));

  clear_black();
  GROW_ERASE(W / 2 - 10, Blue, lt_circle(W / 2, H / 2, i));

  clear_black();
  GROW_ERASE(W / 2 - 100, White, lt_ellipse(W / 2, H / 2, i + 100, i));

  clear_black();
  GROW_ERASE(W / 2 - 10, Yellow,
             lt_triangle(W / 2, i, i, H - 1 - i, W - 1 - i, H - 1 - i));

  clear_black();
  /* lines: two sweeps in red, then erased in black */
  for (int pass = 0; pass < 2; pass++) {
    uint16_t col = pass ? Black : Red;

    lt_fg(col);
    for (int i = 0; i <= W; i += 8) {
      lt_line(i, 0, W - 1 - i, H - 1);
      k_msleep(10);
    }
    for (int i = 0; i <= H; i += 8) {
      lt_line(0, H - 1 - i, W - 1, i);
      k_msleep(10);
    }
  }
  k_msleep(100);
}

static void demo_internal_text(void) {
  layer1_setup(layer1_start_addr);
  clear_black();

  lt_bg(Black);
  lt_fg(Red);
  lt_cgrom_internal();
  lt_font_select(LT_FONT_8x16);
  lt_text_xy(0, 10);
  lt_show_string("buydisplay.com");

  lt_fg(Green);
  lt_font_select(LT_FONT_12x24);
  lt_text_xy(0, 26);
  lt_show_string("buydisplay.com");

  lt_fg(Blue);
  lt_font_select(LT_FONT_16x32);
  lt_text_xy(0, 50);
  lt_show_string("buydisplay.com");

  lt_fg(Yellow);
  lt_font_scale(4, 4);
  lt_text_xy(0, 90);
  lt_show_string("buydisplay.com");
  k_msleep(2000);
}

static void ext_font_row(uint16_t y, uint16_t fg, uint16_t bg, uint8_t scale) {
  lt_font_scale(scale, scale);
  lt_ext_font_print(0, y, fg, bg, gbk_str);
}

static void demo_external_font(void) {
  layer1_setup(0);
  lt_fill_rect(0, 0, W, H, Black);

  /* 16x16 */
  lt_ext_font_init(1, 0, FLASH_ADDR_16, MEMORY_ADDR_16, SIZE_16_NUM,
                   LT_FONT_8x16, 1, 1, false, false);
  lt_ext_font_print(0, 50, Red, White, gbk_str);
  ext_font_row(75, LIGHTRED, Black, 2);
  ext_font_row(120, Blue, White, 3);
  ext_font_row(178, Purple, Black, 4);
  k_msleep(3000);

  /* 24x24 */
  lt_ext_font_init(1, 0, FLASH_ADDR_24, MEMORY_ADDR_24, SIZE_24_NUM,
                   LT_FONT_12x24, 1, 1, false, false);
  lt_fill_rect(0, 0, W, H, Blue);
  lt_ext_font_print(0, 20, Red, White, gbk_str);
  ext_font_row(55, Green, Black, 2);
  ext_font_row(115, Cyan, White, 3);
  ext_font_row(195, Yellow, Black, 4);
  k_msleep(3000);

  /* 32x32 */
  lt_ext_font_init(1, 0, FLASH_ADDR_32, MEMORY_ADDR_32, SIZE_32_NUM,
                   LT_FONT_16x32, 1, 1, false, false);
  lt_fill_rect(0, 0, W, H, White);
  lt_ext_font_print(0, 55, Red, White, gbk_str);
  ext_font_row(115, Green, Black, 2);
  ext_font_row(200, Blue, White, 3);
  ext_font_row(310, Yellow, Black, 4);
  k_msleep(3000);
}

static void demo_bte(void) {
  layer1_setup(0);

  lt_fg(Black);
  lt_fill_rect(0, 0, W - 1, H - 25, Black);
  lt_fill_rect(0, H - 24, W - 1, H - 1, Blue);

  lt_fg(White);
  lt_bg(Blue);
  lt_cgrom_internal();
  lt_font_scale(1, 1);
  lt_font_select(LT_FONT_12x24);
  lt_text_xy(0, H - 24);
  lt_show_string("  Demo BTE Compare");

  lt_fg(Black);
  lt_bg(White);
  lt_font_select(LT_FONT_8x16);
  lt_text_xy(0, H - 48);
  lt_show_string("Execute Logic 'OR' 0xf000");

  /* three copies of the picture via the upstream display_write() */
  show_pic(20, 40);
  show_pic(120, 40);
  show_pic(220, 40);

  lt_active_xy(0, 0);
  lt_active_wh(W, H);
  lt_fg(Black);
  lt_bg(White);
  lt_cgrom_internal();
  lt_font_select(LT_FONT_8x16);
  lt_text_xy(20, 130);
  lt_show_string("Without BTE");
  lt_text_xy(120, 130);
  lt_show_string("BTE Write");
  lt_text_xy(120, 150);
  lt_show_string("ROP");
  lt_text_xy(220, 130);
  lt_show_string("BTE Move");
  lt_text_xy(220, 150);
  lt_show_string("ROP");
  k_msleep(1000);

  /* 1) MCU read-modify-write: OR 0xf000 into first picture (raw memory port) */
  lt_active_xy(20, 40);
  lt_active_wh(80, 80);
  lt_goto_pixel(20, 40);
  lt_mem_port_select();
  (void)lt_data_read(); /* dummy */
  lt_wait_rd_fifo_not_empty();
  for (int i = 0; i < 80 * 80; i++) {
    uint16_t px = lt_data_read();

    px |= (uint16_t)lt_data_read() << 8;
    lt_wait_rd_fifo_not_empty();
    px |= 0xf000;
    lt_data_write(px & 0xff);
    lt_data_write(px >> 8);
    lt_wait_wr_fifo_not_full();
  }
  lt_active_xy(0, 0);
  lt_active_wh(W, H);
  k_msleep(1000);

  /* 2) BTE write with ROP: stream 0xf000 into the second picture */
  lt_bte_setup_16bpp(0, W, 120, 40, 0, W, 120, 40, 80, 80, 14, 0);
  lt_bte_enable();
  lt_mem_port_select();
  for (int i = 0; i < 80 * 80; i++) {
    lt_data_write(0xf000 & 0xff);
    lt_data_write(0xf000 >> 8);
    lt_wait_wr_fifo_not_full();
  }
  lt_wait_wr_fifo_empty();
  lt_wait_2d_idle();
  k_msleep(1000);

  /* 3) BTE move with ROP using a red source block on layer 2 */
  lt_canvas(layer2_start_addr, W);
  lt_active_xy(0, 0);
  lt_active_wh(W, H);
  lt_fg(0xf000);
  lt_bg(Black);
  lt_fill_rect(0, 40, 80, 120, 0xf000);

  lt_bte_copy(layer2_start_addr, W, 0, 40, layer1_start_addr, W, 220, 40,
              layer1_start_addr, W, 220, 40, 14, 80, 80);
  k_msleep(1000);
  lt_bte_disable();
}

static void demo_dma_pictures(void) {
  layer1_setup(layer1_start_addr);
  clear_black();

  for (uint32_t im = 0; im < 4; im++) {
    lt_dma_block(1, 0, 0, 0, W, H, W, im * W * H * 2);
    k_msleep(2000);
  }
}

static void demo_cartoon(void) {
  layer1_setup(layer2_start_addr);
  /* main window shows layer 2 */
  lt_main_image(layer2_start_addr, W);
  lt_fill_rect(0, 0, W, H, Blue);
  lt_dma_block(1, 0, 0, 0, W, H, W, Picture_1_Addr);

  /* 30 frames of 223x134 stacked vertically on layer 3 */
  lt_canvas(layer3_start_addr, 223);
  lt_dma_block(1, 0, 0, 0, 223, 134 * 30, 223, Cartoon_Addr);

  while (1) {
    for (int i = 0; i < 30; i++) {
      lt_bte_copy(layer3_start_addr, 223, 0, 134 * i, layer3_start_addr, 223, 0,
                  134 * i, layer2_start_addr, W, 10, 53, 0x0C, 220, 134);
      k_msleep(40);
    }
  }
}

/* -------------------------------------------------------------------- main */
#include <zephyr/drivers/gpio.h>
int main(void) {
  k_msleep(500);
  int ret = -EAGAIN;
  for (int try = 0; try < 5 && ret; try++) {
    ret = device_init(disp);
    if (ret == -EALREADY) {
      ret = 0;
    } else if (ret) {
      LOG_WRN("Retrying...");
      k_msleep(300);
    }
  }

  if (ret || !device_is_ready(disp)) {
    LOG_ERR("LT7680 display driver not ready");
    return 0;
  }

  const struct gpio_dt_spec bl = {.port = DEVICE_DT_GET(DT_NODELABEL(gpio0)),
                                  .pin = BL_PIN,
                                  .dt_flags = GPIO_ACTIVE_HIGH};
  gpio_pin_configure_dt(&bl, GPIO_OUTPUT_ACTIVE);

  /* big-endian pixel pairs -> native uint16 (driver pushes low byte first) */
  for (int i = 0; i < 80 * 80; i++) {
    pic_px[i] = ((uint16_t)pic_80x80[2 * i] << 8) | pic_80x80[2 * i + 1];
  }

  ret = nv3052_init();

  if (ret) {
    LOG_ERR("panel init failed: %d", ret);
    return 0;
  }

  ret = display_set_pixel_format(disp, PIXEL_FORMAT_RGB_565);
  if (ret && ret != -ENOSYS) {
    LOG_ERR("set pixel format failed: %d", ret);
  }
  display_blanking_off(disp); /* = Arduino Display_ON() */

  while (1) {
    demo_colors();
    demo_shapes();
    demo_internal_text();
    demo_external_font();
    demo_bte();
    demo_dma_pictures();
    demo_cartoon(); /* never returns, as in the original */
  }
}
