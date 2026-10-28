/*
 * Thin register-level layer for the LT7680 engine features that the upstream
 * Zephyr display driver (drivers/display/display_lt7680.c) does not expose through
 * the display API: 2D drawing engine, BTE, serial-flash DMA, CGROM / external fonts.
 *
 * It talks to the same SPI device node as the driver. The driver is only used
 * through display_*() calls from the same thread, so accesses never overlap.
 * Names follow the EastRising Arduino library (ER_TFT.*) to ease comparison.
 */
#ifndef LT7680_HW_H_
#define LT7680_HW_H_

#include <stdint.h>
#include <stdbool.h>

/* ---- panel geometry / memory map (from the tutorial's LCD.h) ---- */
#define LCD_XSIZE_TFT 720
#define LCD_YSIZE_TFT 720

#define layer1_start_addr 0u
#define layer2_start_addr 1036800u
#define layer3_start_addr 2073600u
#define layer4_start_addr 3110400u
#define layer5_start_addr 4147200u

/* RGB565 colours */
#define Black   0x0000
#define White   0xffff
#define Red     0xf800
#define Green   0x07e0
#define Blue    0x001f
#define Yellow  (Red | Green)
#define Cyan    (Green | Blue)
#define Purple  (Red | Blue)
#define LIGHTRED 0xfc10

/* ---- raw access ---- */
void lt_cmd(uint8_t reg);                 /* select register index */
void lt_data_write(uint8_t v);            /* write to selected index */
uint8_t lt_data_read(void);               /* read from selected index */
uint8_t lt_status(void);
void lt_reg_write(uint8_t reg, uint8_t v);
void lt_reg_update(uint8_t reg, uint8_t clear_mask, uint8_t set_mask);

void lt_wait_wr_fifo_not_full(void);
void lt_wait_wr_fifo_empty(void);
void lt_wait_rd_fifo_not_empty(void);
void lt_wait_2d_idle(void);               /* also used for BTE busy */

/* ---- windows / layers ---- */
void lt_main_window_16bpp(void);
void lt_main_image(uint32_t addr, uint16_t width);
void lt_main_window_xy(uint16_t x, uint16_t y);
void lt_canvas(uint32_t addr, uint16_t width);
void lt_active_xy(uint16_t x, uint16_t y);
void lt_active_wh(uint16_t w, uint16_t h);
void lt_goto_pixel(uint16_t x, uint16_t y);

/* ---- drawing engine ---- */
void lt_fg(uint16_t rgb565);
void lt_bg(uint16_t rgb565);
void lt_fill_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lt_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);          /* outline, uses fg */
void lt_round_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t rx, uint16_t ry);
void lt_circle(uint16_t cx, uint16_t cy, uint16_t r);
void lt_ellipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry);
void lt_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3);
void lt_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/* ---- text ---- */
enum lt_font { LT_FONT_8x16, LT_FONT_12x24, LT_FONT_16x32 };
void lt_cgrom_internal(void);
void lt_font_select(enum lt_font f);
void lt_font_scale(uint8_t xn, uint8_t yn);                     /* 1..4 */
void lt_text_xy(uint16_t x, uint16_t y);
void lt_show_string(const char *s);

/* external (flash) font */
void lt_ext_font_init(uint8_t scs, uint8_t clk, uint32_t flash_addr, uint32_t mem_addr,
		      uint32_t num, enum lt_font size, uint8_t xn, uint8_t yn,
		      bool transparent_bg, bool align);
void lt_ext_font_print(uint16_t x, uint16_t y, uint16_t fg, uint16_t bg, const uint8_t *gbk);

/* ---- BTE ---- */
void lt_bte_copy(uint32_t s0_addr, uint16_t s0_w, uint16_t xs0, uint16_t ys0,
		 uint32_t s1_addr, uint16_t s1_w, uint16_t xs1, uint16_t ys1,
		 uint32_t d_addr, uint16_t d_w, uint16_t xd, uint16_t yd,
		 uint8_t rop, uint16_t w, uint16_t h);
void lt_bte_setup_16bpp(uint32_t s1_addr, uint16_t s1_w, uint16_t xs1, uint16_t ys1,
			uint32_t d_addr, uint16_t d_w, uint16_t xd, uint16_t yd,
			uint16_t w, uint16_t h, uint8_t rop, uint8_t op);
void lt_bte_enable(void);
void lt_bte_disable(void);

/* ---- serial-flash DMA (flash attached to the LT7680's own SPI master) ---- */
void lt_dma_block(uint8_t scs, uint8_t clk, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		  uint16_t src_w, uint32_t flash_addr);
void lt_dma_linear(uint8_t scs, uint8_t clk, uint32_t flash_addr, uint32_t mem_addr,
		   uint32_t num);

/* raw memory-port streaming (index 0x04) */
void lt_mem_port_select(void);

#endif
