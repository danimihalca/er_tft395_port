#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include "lt7680_hw.h"

LOG_MODULE_REGISTER(lt7680_hw, LOG_LEVEL_INF);

/* Same SPI device node the upstream display driver uses. */
static const struct spi_dt_spec spi =
	SPI_DT_SPEC_GET(DT_NODELABEL(lt7680), SPI_WORD_SET(8) | SPI_TRANSFER_MSB);

/*
 * LT768x SPI protocol (see upstream driver header comment):
 *   select index : [0x00, reg]
 *   write data   : [0x80, val]
 *   read data    : [0xC0, 0xFF] -> rx[1]
 *   read status  : [0x40, 0xFF] -> rx[1]
 * Each is its own chip-select cycle.
 */
static void spi_tx2(uint8_t a, uint8_t b)
{
	uint8_t tx[2] = {a, b};
	struct spi_buf buf = {.buf = tx, .len = 2};
	struct spi_buf_set set = {.buffers = &buf, .count = 1};

	if (spi_write_dt(&spi, &set) < 0) {
		LOG_ERR("spi write failed");
	}
}

static uint8_t spi_rx2(uint8_t a)
{
	uint8_t tx[2] = {a, 0xFF};
	uint8_t rx[2] = {0, 0};
	struct spi_buf txb = {.buf = tx, .len = 2};
	struct spi_buf rxb = {.buf = rx, .len = 2};
	struct spi_buf_set txs = {.buffers = &txb, .count = 1};
	struct spi_buf_set rxs = {.buffers = &rxb, .count = 1};

	if (spi_transceive_dt(&spi, &txs, &rxs) < 0) {
		LOG_ERR("spi read failed");
	}
	return rx[1];
}

void lt_cmd(uint8_t reg)        { spi_tx2(0x00, reg); }
void lt_data_write(uint8_t v)   { spi_tx2(0x80, v); }
uint8_t lt_data_read(void)      { return spi_rx2(0xC0); }
uint8_t lt_status(void)         { return spi_rx2(0x40); }

void lt_reg_write(uint8_t reg, uint8_t v)
{
	lt_cmd(reg);
	lt_data_write(v);
}

void lt_reg_update(uint8_t reg, uint8_t clear_mask, uint8_t set_mask)
{
	lt_cmd(reg);
	uint8_t t = lt_data_read();

	t &= ~clear_mask;
	t |= set_mask;
	lt_data_write(t);
}

static void wait_status(uint8_t mask, bool want_set, const char *what)
{
	int64_t end = k_uptime_get() + 5000;

	while (((lt_status() & mask) != 0) != want_set) {
		if (k_uptime_get() > end) {
			LOG_ERR("timeout waiting for %s", what);
			return;
		}
	}
}

void lt_wait_wr_fifo_not_full(void)  { wait_status(0x80, false, "WR FIFO not full"); }
void lt_wait_wr_fifo_empty(void)     { wait_status(0x40, true, "WR FIFO empty"); }
void lt_wait_rd_fifo_not_empty(void) { wait_status(0x10, false, "RD FIFO not empty"); }
void lt_wait_2d_idle(void)           { wait_status(0x08, false, "2D/BTE idle"); }

static void reg16(uint8_t reg, uint16_t v)
{
	lt_reg_write(reg, v & 0xFF);
	lt_reg_write(reg + 1, v >> 8);
}

static void reg32(uint8_t reg, uint32_t v)
{
	for (int i = 0; i < 4; i++) {
		lt_reg_write(reg + i, (v >> (8 * i)) & 0xFF);
	}
}

/* ---- windows / layers ---- */
void lt_main_window_16bpp(void)            { lt_reg_update(0x10, BIT(3), BIT(2)); }
void lt_main_image(uint32_t a, uint16_t w) { reg32(0x20, a); reg16(0x24, w); }
void lt_main_window_xy(uint16_t x, uint16_t y) { reg16(0x26, x); reg16(0x28, y); }
void lt_canvas(uint32_t a, uint16_t w)     { reg32(0x50, a); reg16(0x54, w); }
void lt_active_xy(uint16_t x, uint16_t y)  { reg16(0x56, x); reg16(0x58, y); }
void lt_active_wh(uint16_t w, uint16_t h)  { reg16(0x5A, w); reg16(0x5C, h); }
void lt_goto_pixel(uint16_t x, uint16_t y) { reg16(0x5F, x); reg16(0x61, y); }

/* ---- drawing engine ---- */
void lt_fg(uint16_t c)
{
	lt_reg_write(0xD2, c >> 8);
	lt_reg_write(0xD3, (c >> 3) & 0xFF);
	lt_reg_write(0xD4, (c << 3) & 0xFF);
}

void lt_bg(uint16_t c)
{
	lt_reg_write(0xD5, c >> 8);
	lt_reg_write(0xD6, (c >> 3) & 0xFF);
	lt_reg_write(0xD7, (c << 3) & 0xFF);
}

static void p1(uint16_t x, uint16_t y) { reg16(0x68, x); reg16(0x6A, y); }
static void p2(uint16_t x, uint16_t y) { reg16(0x6C, x); reg16(0x6E, y); }
static void p3(uint16_t x, uint16_t y) { reg16(0x70, x); reg16(0x72, y); }
static void radius(uint16_t rx, uint16_t ry) { reg16(0x77, rx); reg16(0x79, ry); }

static void start(uint8_t reg, uint8_t v)
{
	lt_reg_write(reg, v);
	lt_wait_2d_idle();
}

void lt_fill_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
	lt_fg(color);
	p1(x1, y1);
	p2(x2, y2);
	start(0x76, 0xE0);
}

void lt_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	p1(x1, y1);
	p2(x2, y2);
	start(0x76, 0xA0);
}

void lt_round_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t rx, uint16_t ry)
{
	p1(x1, y1);
	p2(x2, y2);
	radius(rx, ry);
	start(0x76, 0xB0);
}

void lt_circle(uint16_t cx, uint16_t cy, uint16_t r)
{
	reg16(0x7B, cx);
	reg16(0x7D, cy);
	radius(r, r);
	start(0x76, 0x80);
}

void lt_ellipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry)
{
	reg16(0x7B, cx);
	reg16(0x7D, cy);
	radius(rx, ry);
	start(0x76, 0x80);
}

void lt_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3)
{
	p1(x1, y1);
	p2(x2, y2);
	p3(x3, y3);
	start(0x67, 0x82);
}

void lt_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	p1(x1, y1);
	p2(x2, y2);
	start(0x67, 0x80);
}

/* ---- text ---- */
static void text_mode(bool on)
{
	if (on) {
		lt_reg_update(0x03, 0, BIT(2));
	} else {
		lt_reg_update(0x03, BIT(2), 0);
	}
}

void lt_mem_port_select(void) { lt_cmd(0x04); }

void lt_cgrom_internal(void)     { lt_reg_update(0xCC, BIT(7) | BIT(6), 0); }
static void user_font_mode(void) { lt_reg_update(0xCC, BIT(6), BIT(7)); }

void lt_font_select(enum lt_font f)
{
	switch (f) {
	case LT_FONT_8x16:  lt_reg_update(0xCC, BIT(5) | BIT(4), 0); break;
	case LT_FONT_12x24: lt_reg_update(0xCC, BIT(5), BIT(4)); break;
	case LT_FONT_16x32: lt_reg_update(0xCC, BIT(4), BIT(5)); break;
	}
}

void lt_font_scale(uint8_t xn, uint8_t yn)
{
	/* 0xCD: bits[3:2] width-1, bits[1:0] height-1 */
	lt_reg_update(0xCD, 0x0F, (((xn - 1) & 3) << 2) | ((yn - 1) & 3));
}

void lt_text_xy(uint16_t x, uint16_t y) { reg16(0x63, x); reg16(0x65, y); }

void lt_show_string(const char *s)
{
	text_mode(true);
	lt_mem_port_select();
	for (; *s; s++) {
		lt_data_write(*s);
		lt_wait_wr_fifo_not_full();
	}
	lt_wait_2d_idle();
	text_mode(false);
}

/* ---- serial flash DMA ---- */
static void sfi_common(uint8_t scs, uint8_t clk)
{
	lt_reg_update(0x01, 0, BIT(1));                       /* enable SFlash SPI */
	if (scs == 0) {
		lt_reg_update(0xB7, BIT(7), 0);
	} else {
		lt_reg_update(0xB7, 0, BIT(7));
	}
	lt_reg_update(0xB7, 0, BIT(6));                       /* DMA mode */
	lt_reg_write(0xBB, clk);                              /* SPI clock period */
}

static void sfi_wait_idle(void)
{
	int64_t end = k_uptime_get() + 20000;

	lt_cmd(0xB6);
	while (lt_data_read() & 0x01) {
		if (k_uptime_get() > end) {
			LOG_ERR("SFI DMA timeout");
			return;
		}
	}
}

static void sfi_src_addr(uint32_t a)  { reg32(0xBC, a); }

void lt_dma_block(uint8_t scs, uint8_t clk, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		  uint16_t src_w, uint32_t flash_addr)
{
	sfi_common(scs, clk);
	lt_reg_update(0x5E, BIT(2), 0);                       /* XY (block) mode */
	lt_goto_pixel(x, y);
	reg16(0xC0, x);                                       /* dest upper-left */
	reg16(0xC2, y);
	reg16(0xC6, w);                                       /* transfer w/h */
	reg16(0xC8, h);
	reg16(0xCA, src_w);
	sfi_src_addr(flash_addr);
	lt_reg_update(0xB6, 0, BIT(0));                       /* start */
	sfi_wait_idle();
}

void lt_dma_linear(uint8_t scs, uint8_t clk, uint32_t flash_addr, uint32_t mem_addr, uint32_t num)
{
	sfi_common(scs, clk);
	lt_reg_update(0x5E, 0, BIT(2));                       /* linear mode */
	reg32(0xC0, mem_addr);                                /* dest start */
	reg32(0xC6, num);                                     /* transfer number */
	sfi_src_addr(flash_addr);
	sfi_wait_idle();
	lt_reg_update(0xB6, 0, BIT(0));
	sfi_wait_idle();
	lt_reg_update(0x5E, BIT(2), 0);                       /* back to XY mode */
}

/* ---- external font ---- */
void lt_ext_font_init(uint8_t scs, uint8_t clk, uint32_t flash_addr, uint32_t mem_addr,
		      uint32_t num, enum lt_font size, uint8_t xn, uint8_t yn,
		      bool transparent_bg, bool align)
{
	lt_font_select(size);
	lt_font_scale(xn, yn);
	lt_reg_update(0xCD, BIT(6), transparent_bg ? BIT(6) : 0);
	lt_reg_update(0xCD, BIT(7), align ? BIT(7) : 0);
	lt_dma_linear(scs, clk, flash_addr, mem_addr, num);
	reg32(0xDB, mem_addr);                                /* CGRAM start address */
}

void lt_ext_font_print(uint16_t x, uint16_t y, uint16_t fg, uint16_t bg, const uint8_t *c)
{
	text_mode(true);
	user_font_mode();
	lt_fg(fg);
	lt_bg(bg);
	lt_text_xy(x, y);

	for (unsigned int i = 0; c[i] != '\0';) {
		if (c[i] < 0xa1) {
			lt_cgrom_internal();
			lt_mem_port_select();
			lt_data_write(c[i]);
			lt_wait_wr_fifo_not_full();
			i += 1;
		} else {
			user_font_mode();
			lt_mem_port_select();
			uint16_t t = ((c[i] - 0xa1) & 0xff) * 94 + (c[i + 1] - 0xa1) + 0x8000;

			lt_data_write(t >> 8);
			lt_wait_wr_fifo_not_full();
			lt_data_write(t & 0xff);
			lt_wait_wr_fifo_not_full();
			i += 2;
		}
	}
	lt_wait_2d_idle();
	text_mode(false);
}

/* ---- BTE ---- */
void lt_bte_enable(void)  { lt_reg_update(0x90, 0, BIT(4)); }
void lt_bte_disable(void) { lt_reg_update(0x90, BIT(4), 0); }

static void bte_colors_16bpp(void)
{
	lt_reg_update(0x92, BIT(6), BIT(5));                  /* S0 16bpp */
	lt_reg_update(0x92, BIT(4) | BIT(3), BIT(2));         /* S1 16bpp */
	lt_reg_update(0x92, BIT(1), BIT(0));                  /* dest 16bpp */
}

static void bte_rop_op(uint8_t rop, uint8_t op)
{
	lt_reg_update(0x91, 0xF0, (rop << 4) & 0xF0);
	lt_reg_update(0x91, 0x0F, op & 0x0F);
}

void lt_bte_setup_16bpp(uint32_t s1_addr, uint16_t s1_w, uint16_t xs1, uint16_t ys1,
			uint32_t d_addr, uint16_t d_w, uint16_t xd, uint16_t yd,
			uint16_t w, uint16_t h, uint8_t rop, uint8_t op)
{
	bte_colors_16bpp();
	reg32(0x9D, s1_addr);
	reg16(0xA1, s1_w);
	reg16(0xA3, xs1);
	reg16(0xA5, ys1);
	reg32(0xA7, d_addr);
	reg16(0xAB, d_w);
	reg16(0xAD, xd);
	reg16(0xAF, yd);
	reg16(0xB1, w);
	reg16(0xB3, h);
	bte_rop_op(rop, op);
}

void lt_bte_copy(uint32_t s0_addr, uint16_t s0_w, uint16_t xs0, uint16_t ys0,
		 uint32_t s1_addr, uint16_t s1_w, uint16_t xs1, uint16_t ys1,
		 uint32_t d_addr, uint16_t d_w, uint16_t xd, uint16_t yd,
		 uint8_t rop, uint16_t w, uint16_t h)
{
	reg32(0x93, s0_addr);
	reg16(0x97, s0_w);
	reg16(0x99, xs0);
	reg16(0x9B, ys0);
	lt_bte_setup_16bpp(s1_addr, s1_w, xs1, ys1, d_addr, d_w, xd, yd, w, h, rop, 0x02);
	lt_bte_enable();
	lt_wait_2d_idle();
}
