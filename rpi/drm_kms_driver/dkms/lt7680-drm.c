// SPDX-License-Identifier: GPL-2.0-only
/*
 * DRM/KMS driver for the Levetop LT7680 TFT controller on an SPI bus, as used on
 * EastRising ER-TFT3.95-1 (720x720 panel behind an NV3052 controller).
 *
 * Register sequences follow EastRising's Arduino/ESP32 tutorial
 * (Display_DMA_Cartoon_Font_test). Only the framebuffer path is implemented;
 * the LT7680 2D engine, BTE, flash DMA and font engine are not exposed.
 *
 * SPI protocol (every access is its own chip-select cycle):
 *   select index : 0x00, reg
 *   write data   : 0x80, val [, val ...]   (stream when index = 0x04)
 *   read data    : 0xC0 -> val
 *   read status  : 0x40 -> status
 *
 * Targets Raspberry Pi OS kernels (6.6 / 6.12, DMA GEM + simple display pipe). Not built
 * or tested on hardware. Panel timing is parsed from the "panel-timing" DT node by hand, so
 * no CONFIG_OF_DISPLAY_TIMING / VIDEOMODE_HELPERS dependency is needed.
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/gpio/consumer.h>
#include <linux/iosys-map.h>
#include <linux/ktime.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/version.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_modes.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
#if __has_include(<drm/clients/drm_client_setup.h>)
#include <drm/clients/drm_client_setup.h>
#else
#include <drm/drm_client_setup.h>
#endif
#include <drm/drm_fbdev_dma.h>
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
#include <drm/drm_fbdev_dma.h>
#else
#include <drm/drm_fbdev_generic.h>
#endif

#include "lt7680-nv3052-boe395.h"

#define DRIVER_NAME		"lt7680-drm"

/* Status register bits */
#define ST_POWER_SAVING		BIT(1)
#define ST_SDRAM_READY		BIT(2)
#define ST_CORE_BUSY		BIT(3)
#define ST_WR_FIFO_EMPTY	BIT(6)
#define ST_WR_FIFO_FULL		BIT(7)

/* Registers (names from the LT768x datasheet / EastRising library) */
#define R_SRR			0x00
#define R_CCR			0x01
#define R_MACR			0x02
#define R_ICR			0x03
#define R_MRWDP			0x04
#define R_PPLLC1		0x05	/* SCLK (pixel) PLL */
#define R_PPLLC2		0x06
#define R_MPLLC1		0x07	/* MCLK (SDRAM) PLL */
#define R_MPLLC2		0x08
#define R_CPLLC1		0x09	/* CCLK (core) PLL */
#define R_CPLLC2		0x0a
#define R_MPWCTR		0x10	/* main window control */
#define R_DPCR			0x12
#define R_PCSR			0x13
#define R_HDWR			0x14
#define R_HDWFTR		0x15
#define R_HNDR			0x16
#define R_HNDFTR		0x17
#define R_HSTR			0x18
#define R_HPWR			0x19
#define R_VDHR0			0x1a
#define R_VNDR0			0x1c
#define R_VSTR			0x1e
#define R_VPWR			0x1f
#define R_MISA0			0x20
#define R_MIW0			0x24
#define R_MWULX0		0x26
#define R_CVSSA0		0x50
#define R_CVS_IMWTH0		0x54
#define R_AWUL_X0		0x56
#define R_AW_WTH0		0x5a
#define R_MWCR0			0x5e
#define R_CURH0			0x5f
#define R_SDRAR			0xe0
#define R_SDRMD			0xe1
#define R_SDR_REF_ITVL0		0xe2
#define R_SDRCR			0xe4

#define LT_FRAME_MAX_W		1280
#define LT_CHUNK_MAX		32768

static unsigned int fifo_poll = 1;
module_param(fifo_poll, uint, 0644);
MODULE_PARM_DESC(fifo_poll, "Poll the write FIFO status between SPI chunks (default 1)");

static int pclk_edge = -1;
module_param(pclk_edge, int, 0644);
MODULE_PARM_DESC(pclk_edge, "Force PCLK edge: 0 or 1 (DPCR bit7), -1 = from DT (default)");

static bool pageflip = true;
module_param(pageflip, bool, 0644);
MODULE_PARM_DESC(pageflip, "Double-buffer in SDRAM and flip the display start address (default 1)");

static unsigned int sclk_mhz;
module_param(sclk_mhz, uint, 0644);
MODULE_PARM_DESC(sclk_mhz, "Override pixel clock PLL N (MHz), 0 = from mode (default)");

struct lt7680_panel_cfg {
	const u8 (*init)[2];
	unsigned int n_init;
};

struct lt7680 {
	struct drm_device drm;
	struct drm_simple_display_pipe pipe;
	struct drm_connector connector;
	struct spi_device *spi;

	struct gpio_desc *reset;
	struct gpio_desc *pcs, *psck, *psdi;	/* NV3052 3-wire init bus */
	struct gpio_desc *bl;			/* optional backlight enable */
	const struct lt7680_panel_cfg *panel;

	struct drm_display_mode mode;
	bool hsync_high, vsync_high, de_low, pclk_inv;
	u16 pll_n_sclk;

	bool pageflip;			/* two SDRAM pages, flipped on update */
	unsigned int front;		/* page currently scanned out (0/1) */
	struct drm_rect prev;		/* region the back page is stale in */

	u8 *txbuf;			/* [0x80][pixel stream...] */
	size_t chunk;			/* payload bytes per SPI message */
	u8 *rowbuf;			/* one converted RGB565 row */

	int err;			/* sticky SPI error during init */
};

static inline struct lt7680 *to_lt(struct drm_device *drm)
{
	return container_of(drm, struct lt7680, drm);
}

/* ----------------------------------------------------------------- SPI access */

static void lt_spi2(struct lt7680 *lt, u8 a, u8 b)
{
	u8 tx[2] = { a, b };
	int ret = spi_write_then_read(lt->spi, tx, 2, NULL, 0);

	if (ret && !lt->err)
		lt->err = ret;
}

static void lt_cmd(struct lt7680 *lt, u8 reg)
{
	lt_spi2(lt, 0x00, reg);
}

static void lt_data(struct lt7680 *lt, u8 val)
{
	lt_spi2(lt, 0x80, val);
}

static u8 lt_data_read(struct lt7680 *lt)
{
	u8 cmd = 0xc0, val = 0;
	int ret = spi_write_then_read(lt->spi, &cmd, 1, &val, 1);

	if (ret && !lt->err)
		lt->err = ret;
	return val;
}

static u8 lt_status(struct lt7680 *lt)
{
	u8 cmd = 0x40, val = 0;
	int ret = spi_write_then_read(lt->spi, &cmd, 1, &val, 1);

	if (ret && !lt->err)
		lt->err = ret;
	return val;
}

static void lt_wr(struct lt7680 *lt, u8 reg, u8 val)
{
	lt_cmd(lt, reg);
	lt_data(lt, val);
}

static u8 lt_rd(struct lt7680 *lt, u8 reg)
{
	lt_cmd(lt, reg);
	return lt_data_read(lt);
}

static void lt_upd(struct lt7680 *lt, u8 reg, u8 clr, u8 set)
{
	u8 v = lt_rd(lt, reg);

	v &= ~clr;
	v |= set;
	lt_data(lt, v);		/* index still selected */
}

static void lt_wr16(struct lt7680 *lt, u8 reg, u16 v)
{
	lt_wr(lt, reg, v & 0xff);
	lt_wr(lt, reg + 1, v >> 8);
}

static void lt_wr32(struct lt7680 *lt, u8 reg, u32 v)
{
	int i;

	for (i = 0; i < 4; i++)
		lt_wr(lt, reg + i, (v >> (8 * i)) & 0xff);
}

static int lt_wait_status(struct lt7680 *lt, u8 mask, bool want_set, unsigned int ms)
{
	ktime_t end = ktime_add_ms(ktime_get(), ms);

	while (((lt_status(lt) & mask) != 0) != want_set) {
		if (lt->err)
			return lt->err;
		if (ktime_after(ktime_get(), end))
			return -ETIMEDOUT;
		usleep_range(100, 300);
	}
	return 0;
}

/* ------------------------------------------------------------- chip bring-up */

static void lt_hw_reset(struct lt7680 *lt)
{
	if (!lt->reset)
		return;
	gpiod_set_value_cansleep(lt->reset, 1);	/* asserted (pin low) */
	msleep(100);
	gpiod_set_value_cansleep(lt->reset, 0);
	msleep(200);
}

static int lt_system_check(struct lt7680 *lt)
{
	int i = 0, loops;

	for (loops = 0; loops < 100; loops++) {
		if (!(lt_status(lt) & ST_POWER_SAVING)) {
			msleep(2);
			if (lt_rd(lt, R_CCR) & 0x80)
				return lt->err;
			msleep(2);
			lt_wr(lt, R_CCR, 0x80);
		} else if (++i == 5) {
			lt_hw_reset(lt);
			i = 0;
		}
		if (lt->err)
			return lt->err;
		msleep(2);
	}
	return -ETIMEDOUT;
}

static void lt_pll_init(struct lt7680 *lt)
{
	/* XI = 10 MHz, R = 5, OD = 2  =>  Fout = N MHz */
	const u8 od = 2, r = 5;
	const u16 n_mclk = 100, n_cclk = 100;

	lt_wr(lt, R_PPLLC1, (od << 6) | (r << 1) | ((lt->pll_n_sclk >> 8) & 1));
	lt_wr(lt, R_MPLLC1, (od << 6) | (r << 1) | ((n_mclk >> 8) & 1));
	lt_wr(lt, R_CPLLC1, (od << 6) | (r << 1) | ((n_cclk >> 8) & 1));
	lt_wr(lt, R_PPLLC2, lt->pll_n_sclk & 0xff);
	lt_wr(lt, R_MPLLC2, n_mclk & 0xff);
	lt_wr(lt, R_CPLLC2, n_cclk & 0xff);
	lt_cmd(lt, R_SRR);
	udelay(1);
	lt_data(lt, 0x80);		/* reconfigure PLLs */
	msleep(1);
}

static int lt_sdram_init(struct lt7680 *lt)
{
	u16 itv = (64000000 / 8192) / (1000 / 60);

	itv -= 2;
	lt_wr(lt, R_SDRAR, 0x29);
	lt_wr(lt, R_SDRMD, 0x03);
	lt_wr16(lt, R_SDR_REF_ITVL0, itv);
	lt_wr(lt, R_SDRCR, 0x01);
	return lt_wait_status(lt, ST_SDRAM_READY, true, 1000);
}

/* horizontal values are programmed in units of 8 pixels (see EastRising library) */
static void lt_h8(struct lt7680 *lt, u8 reg_hi, int reg_fine, u16 v)
{
	if (v < 8) {
		lt_wr(lt, reg_hi, 0);
		if (reg_fine >= 0)
			lt_wr(lt, reg_fine, v);
	} else {
		lt_wr(lt, reg_hi, v / 8 - 1);
		if (reg_fine >= 0)
			lt_wr(lt, reg_fine, v % 8);
	}
}

static void lt_timing_init(struct lt7680 *lt)
{
	const struct drm_display_mode *m = &lt->mode;
	u16 hfp = m->hsync_start - m->hdisplay;
	u16 hsw = m->hsync_end - m->hsync_start;
	u16 hbp = m->htotal - m->hsync_end;
	u16 vfp = m->vsync_start - m->vdisplay;
	u16 vsw = m->vsync_end - m->vsync_start;
	u16 vbp = m->vtotal - m->vsync_end;

	/* display width / height */
	lt_h8(lt, R_HDWR, R_HDWFTR, m->hdisplay);
	lt_wr16(lt, R_VDHR0, m->vdisplay - 1);
	/* back porch, front porch, sync */
	lt_h8(lt, R_HNDR, R_HNDFTR, hbp);
	lt_h8(lt, R_HSTR, -1, hfp);
	lt_h8(lt, R_HPWR, -1, hsw);
	lt_wr16(lt, R_VNDR0, vbp - 1);
	lt_wr(lt, R_VSTR, vfp - 1);
	lt_wr(lt, R_VPWR, vsw - 1);
}

static void lt_init_chip(struct lt7680 *lt)
{
	lt_pll_init(lt);
	if (lt_sdram_init(lt) && !lt->err)
		lt->err = -ETIMEDOUT;

	lt_upd(lt, R_CCR, BIT(4), BIT(3));		/* TFT 18-bit */
	lt_upd(lt, R_CCR, 0, BIT(0));			/* host bus 16-bit */
	lt_upd(lt, R_MACR, BIT(7), BIT(6));		/* RGB 16-bit / 16bpp */
	lt_upd(lt, R_MACR, BIT(2) | BIT(1), 0);		/* write L->R, T->B */
	lt_upd(lt, R_ICR, BIT(2), 0);			/* graphic mode */
	lt_upd(lt, R_ICR, BIT(1) | BIT(0), 0);		/* SDRAM */
	lt_upd(lt, R_DPCR, BIT(4), 0);			/* H scan L->R */
	lt_upd(lt, R_DPCR, BIT(3), 0);			/* V scan T->B */
	lt_upd(lt, R_DPCR, 0x07, 0);			/* RGB order */
	/* PCLK edge */
	lt_upd(lt, R_DPCR, BIT(7), lt->pclk_inv ? BIT(7) : 0);
	lt_upd(lt, R_PCSR, BIT(7), lt->hsync_high ? BIT(7) : 0);
	lt_upd(lt, R_PCSR, BIT(6), lt->vsync_high ? BIT(6) : 0);
	/* DE: bit set = low-active */
	lt_upd(lt, R_PCSR, BIT(5), lt->de_low ? BIT(5) : 0);

	lt_timing_init(lt);

	lt_upd(lt, R_MPWCTR, BIT(3), BIT(2));		/* main window 16bpp */
	lt_upd(lt, R_MWCR0, BIT(2), 0);			/* block (XY) mode */
	lt_upd(lt, R_MWCR0, BIT(1), BIT(0));		/* 16bpp memory */
	lt_upd(lt, R_MPWCTR, BIT(3), BIT(2));

	/* layer 1 at SDRAM address 0, full-size windows */
	lt_wr32(lt, R_MISA0, 0);
	lt_wr16(lt, R_MIW0, lt->mode.hdisplay);
	lt_wr16(lt, R_MWULX0, 0);
	lt_wr16(lt, R_MWULX0 + 2, 0);
	lt_wr32(lt, R_CVSSA0, 0);
	lt_wr16(lt, R_CVS_IMWTH0, lt->mode.hdisplay);
	lt_wr16(lt, R_AWUL_X0, 0);
	lt_wr16(lt, R_AWUL_X0 + 2, 0);
	lt_wr16(lt, R_AW_WTH0, lt->mode.hdisplay);
	lt_wr16(lt, R_AW_WTH0 + 2, lt->mode.vdisplay);
}

/* ------------------------------------------- NV3052 panel init (bit-banged 9-bit SPI) */

static void nv_bit(struct lt7680 *lt, int v)
{
	gpiod_set_value_cansleep(lt->psck, 0);
	gpiod_set_value_cansleep(lt->psdi, v);
	gpiod_set_value_cansleep(lt->psck, 1);
}

static void nv_send(struct lt7680 *lt, bool is_data, u8 v)
{
	int i;

	gpiod_set_value_cansleep(lt->pcs, 1);		/* select (pin low) */
	nv_bit(lt, is_data);
	for (i = 0; i < 8; i++, v <<= 1)
		nv_bit(lt, !!(v & 0x80));
	gpiod_set_value_cansleep(lt->pcs, 0);
}

static void nv3052_init(struct lt7680 *lt)
{
	unsigned int i;

	for (i = 0; i < lt->panel->n_init; i++) {
		nv_send(lt, false, lt->panel->init[i][0]);
		nv_send(lt, true, lt->panel->init[i][1]);
	}
	nv_send(lt, false, 0x11);			/* sleep out */
	msleep(120);
	nv_send(lt, false, 0x29);			/* display on */
	msleep(20);
}

/* ------------------------------------------------------------------- blitting */

static int lt_flush_chunk(struct lt7680 *lt, size_t len)
{
	struct spi_transfer xfer = { .tx_buf = lt->txbuf, .len = len + 1 };
	struct spi_message msg;
	int ret;

	if (fifo_poll) {
		ret = lt_wait_status(lt, ST_WR_FIFO_FULL, false, 100);
		if (ret)
			return ret;
	}
	spi_message_init_with_transfers(&msg, &xfer, 1);
	return spi_sync(lt->spi, &msg);
}

static void lt_convert_row(u16 *dst, const void *src, unsigned int w, u32 format)
{
	unsigned int x;

	if (format == DRM_FORMAT_RGB565) {
		memcpy(dst, src, w * 2);
		return;
	}
	for (x = 0; x < w; x++) {
		u32 p = ((const u32 *)src)[x];

		dst[x] = cpu_to_le16(((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0) |
				     ((p >> 3) & 0x001f));
	}
}

/*
 * On-chip BTE memory copy (ROP 0x0C = source 0, op 0x02), 16bpp, same
 * register sequence as the EastRising tutorial. Used to bring the hidden
 * page back in sync after a flip without sending pixels over SPI.
 */
static int lt_bte_copy_rect(struct lt7680 *lt, u32 src, u32 dst,
			    const struct drm_rect *r)
{
	u16 W = lt->mode.hdisplay;

	lt->err = 0;
	lt_upd(lt, 0x92, BIT(6), BIT(5));		/* S0 16bpp */
	lt_upd(lt, 0x92, BIT(4) | BIT(3), BIT(2));	/* S1 16bpp */
	lt_upd(lt, 0x92, BIT(1), BIT(0));		/* dest 16bpp */
	lt_wr32(lt, 0x93, src);
	lt_wr16(lt, 0x97, W);
	lt_wr16(lt, 0x99, r->x1);
	lt_wr16(lt, 0x9b, r->y1);
	lt_wr32(lt, 0x9d, src);				/* S1 unused with ROP 0x0C */
	lt_wr16(lt, 0xa1, W);
	lt_wr16(lt, 0xa3, r->x1);
	lt_wr16(lt, 0xa5, r->y1);
	lt_wr32(lt, 0xa7, dst);
	lt_wr16(lt, 0xab, W);
	lt_wr16(lt, 0xad, r->x1);
	lt_wr16(lt, 0xaf, r->y1);
	lt_wr16(lt, 0xb1, drm_rect_width(r));
	lt_wr16(lt, 0xb3, drm_rect_height(r));
	lt_upd(lt, 0x91, 0xf0, 0xc0);			/* ROP 0x0C */
	lt_upd(lt, 0x91, 0x0f, 0x02);			/* memory copy with ROP */
	lt_upd(lt, 0x90, 0, BIT(4));			/* start BTE */
	if (lt->err)
		return lt->err;
	return lt_wait_status(lt, ST_CORE_BUSY, false, 200);
}

static int lt_blit(struct lt7680 *lt, struct drm_framebuffer *fb, const void *vaddr,
		   const struct drm_rect *clip)
{
	unsigned int w = drm_rect_width(clip), h = drm_rect_height(clip);
	unsigned int cpp = fb->format->cpp[0];
	size_t row_bytes = w * 2, fill = 0, n;
	unsigned int y;
	int ret;

	lt->err = 0;
	lt_wr16(lt, R_AWUL_X0, clip->x1);
	lt_wr16(lt, R_AWUL_X0 + 2, clip->y1);
	lt_wr16(lt, R_AW_WTH0, w);
	lt_wr16(lt, R_AW_WTH0 + 2, h);
	lt_wr16(lt, R_CURH0, clip->x1);
	lt_wr16(lt, R_CURH0 + 2, clip->y1);
	lt_cmd(lt, R_MRWDP);
	if (lt->err)
		return lt->err;

	lt->txbuf[0] = 0x80;
	for (y = 0; y < h; y++) {
		const u8 *src = vaddr + (clip->y1 + y) * fb->pitches[0] + clip->x1 * cpp;
		size_t done = 0;

		lt_convert_row((u16 *)lt->rowbuf, src, w, fb->format->format);

		while (done < row_bytes) {
			n = min(row_bytes - done, lt->chunk - fill);
			memcpy(lt->txbuf + 1 + fill, lt->rowbuf + done, n);
			fill += n;
			done += n;
			if (fill == lt->chunk) {
				ret = lt_flush_chunk(lt, fill);
				if (ret)
					return ret;
				fill = 0;
			}
		}
	}
	return fill ? lt_flush_chunk(lt, fill) : 0;
}

static void lt_fb_dirty(struct lt7680 *lt, struct drm_framebuffer *fb,
			const struct iosys_map *map, struct drm_rect *rect)
{
	struct drm_rect full = DRM_RECT_INIT(0, 0, fb->width, fb->height);
	int ret;

	if (!drm_rect_intersect(rect, &full))
		return;
	if (map->is_iomem)
		return;

	if (!lt->pageflip) {
		ret = lt_blit(lt, fb, map->vaddr, rect);
		if (ret)
			drm_err_once(&lt->drm, "frame transfer failed: %d\n", ret);
		return;
	}

	/*
	 * Double buffering: the back page is stale in the region that was
	 * damaged last time, so rewrite damage + previous damage there, wait
	 * for the chip to drain its write FIFO, then flip the scan-out address.
	 */
	{
		struct drm_rect dmg = *rect, u = *rect;
		unsigned int back = lt->front ^ 1;
		u32 page = lt->mode.hdisplay * lt->mode.vdisplay * 2;

		if (drm_rect_visible(&lt->prev)) {
			u.x1 = min(u.x1, lt->prev.x1);
			u.y1 = min(u.y1, lt->prev.y1);
			u.x2 = max(u.x2, lt->prev.x2);
			u.y2 = max(u.y2, lt->prev.y2);
		}

		lt->err = 0;
		lt_wr32(lt, R_CVSSA0, back * page);
		ret = lt_blit(lt, fb, map->vaddr, &u);
		if (!ret)
			ret = lt_wait_status(lt, ST_WR_FIFO_EMPTY, true, 100);
		if (!ret) {
			lt_wr32(lt, R_MISA0, back * page);
			ret = lt->err;
		}
		if (ret) {
			drm_err_once(&lt->drm, "frame transfer failed: %d\n", ret);
			/* unknown state: rewrite everything next time */
			lt->prev = full;
			return;
		}
		lt->front = back;
		lt->prev = dmg;

		/* sync the old page (now hidden) from the new front on-chip */
		if (!lt_bte_copy_rect(lt, back * page, (back ^ 1) * page, &dmg))
			lt->prev = (struct drm_rect){ 0 };
	}
}

/* ------------------------------------------------------------------ KMS side */

static enum drm_mode_status lt_pipe_mode_valid(struct drm_simple_display_pipe *pipe,
					       const struct drm_display_mode *mode)
{
	struct lt7680 *lt = to_lt(pipe->crtc.dev);

	if (mode->hdisplay != lt->mode.hdisplay || mode->vdisplay != lt->mode.vdisplay)
		return MODE_BAD;
	return MODE_OK;
}

static void lt_pipe_enable(struct drm_simple_display_pipe *pipe,
			   struct drm_crtc_state *crtc_state,
			   struct drm_plane_state *plane_state)
{
	struct lt7680 *lt = to_lt(pipe->crtc.dev);
	struct drm_shadow_plane_state *shadow = to_drm_shadow_plane_state(plane_state);
	struct drm_rect rect = DRM_RECT_INIT(0, 0, plane_state->fb->width,
					     plane_state->fb->height);
	int idx;

	if (!drm_dev_enter(pipe->crtc.dev, &idx))
		return;
	lt_fb_dirty(lt, plane_state->fb, &shadow->data[0], &rect);
	lt->err = 0;
	lt_upd(lt, R_DPCR, 0, BIT(6));			/* display on */
	if (lt->bl)
		gpiod_set_value_cansleep(lt->bl, 1);
	drm_dev_exit(idx);
}

static void lt_pipe_disable(struct drm_simple_display_pipe *pipe)
{
	struct lt7680 *lt = to_lt(pipe->crtc.dev);
	int idx;

	if (!drm_dev_enter(pipe->crtc.dev, &idx))
		return;
	lt->err = 0;
	if (lt->bl)
		gpiod_set_value_cansleep(lt->bl, 0);
	lt_upd(lt, R_DPCR, BIT(6), 0);			/* display off */
	drm_dev_exit(idx);
}

static void lt_pipe_update(struct drm_simple_display_pipe *pipe,
			   struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = pipe->plane.state;
	struct drm_shadow_plane_state *shadow = to_drm_shadow_plane_state(state);
	struct lt7680 *lt = to_lt(pipe->crtc.dev);
	struct drm_rect rect;
	int idx;

	if (!pipe->crtc.state->active || !state->fb)
		return;
	if (!drm_dev_enter(pipe->crtc.dev, &idx))
		return;
	if (drm_atomic_helper_damage_merged(old_state, state, &rect))
		lt_fb_dirty(lt, state->fb, &shadow->data[0], &rect);
	drm_dev_exit(idx);
}

static const struct drm_simple_display_pipe_funcs lt_pipe_funcs = {
	.mode_valid = lt_pipe_mode_valid,
	.enable = lt_pipe_enable,
	.disable = lt_pipe_disable,
	.update = lt_pipe_update,
	DRM_GEM_SIMPLE_DISPLAY_PIPE_SHADOW_PLANE_FUNCS,
};

static int lt_conn_get_modes(struct drm_connector *connector)
{
	struct lt7680 *lt = to_lt(connector->dev);
	struct drm_display_mode *mode = drm_mode_duplicate(connector->dev, &lt->mode);

	if (!mode)
		return 0;
	drm_mode_probed_add(connector, mode);
	return 1;
}

static const struct drm_connector_helper_funcs lt_conn_helper_funcs = {
	.get_modes = lt_conn_get_modes,
};

static const struct drm_connector_funcs lt_conn_funcs = {
	.reset = drm_atomic_helper_connector_reset,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_mode_config_funcs lt_mode_config_funcs = {
	.fb_create = drm_gem_fb_create_with_dirty,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static const u32 lt_formats[] = { DRM_FORMAT_RGB565, DRM_FORMAT_XRGB8888 };

DEFINE_DRM_GEM_DMA_FOPS(lt_fops);

static const struct drm_driver lt_drm_driver = {
	.driver_features = DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
	.fops = &lt_fops,
	DRM_GEM_DMA_DRIVER_OPS,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
	DRM_FBDEV_DMA_DRIVER_OPS,
#endif
	.name = "lt7680",
	.desc = "Levetop LT7680 SPI display",
	.major = 1,
	.minor = 0,
};

/* -------------------------------------------------------------------- probe */

static int lt_rd_u32(struct device *dev, struct device_node *np, const char *name, u32 *v)
{
	int ret = of_property_read_u32(np, name, v);

	if (ret)
		dev_err(dev, "panel-timing: missing '%s'\n", name);
	return ret;
}

static int lt_parse_timing(struct lt7680 *lt, struct device *dev)
{
	struct device_node *t = of_get_child_by_name(dev->of_node, "panel-timing");
	struct drm_display_mode *m = &lt->mode;
	u32 clk, ha, va, hfp, hbp, hsw, vfp, vbp, vsw;
	u32 hs = 0, vs = 0, de = 1, pc = 0, w_mm = 0, h_mm = 0;
	int ret;

	if (!t)
		return dev_err_probe(dev, -EINVAL, "missing panel-timing node\n");

	ret = lt_rd_u32(dev, t, "clock-frequency", &clk) ?:
	      lt_rd_u32(dev, t, "hactive", &ha) ?:
	      lt_rd_u32(dev, t, "vactive", &va) ?:
	      lt_rd_u32(dev, t, "hfront-porch", &hfp) ?:
	      lt_rd_u32(dev, t, "hback-porch", &hbp) ?:
	      lt_rd_u32(dev, t, "hsync-len", &hsw) ?:
	      lt_rd_u32(dev, t, "vfront-porch", &vfp) ?:
	      lt_rd_u32(dev, t, "vback-porch", &vbp) ?:
	      lt_rd_u32(dev, t, "vsync-len", &vsw);
	if (ret)
		goto out;

	/* optional polarity properties; defaults match the tutorial */
	of_property_read_u32(t, "hsync-active", &hs);
	of_property_read_u32(t, "vsync-active", &vs);
	of_property_read_u32(t, "de-active", &de);
	of_property_read_u32(t, "pixelclk-active", &pc);

	memset(m, 0, sizeof(*m));
	m->clock = clk / 1000;
	m->hdisplay = ha;
	m->hsync_start = ha + hfp;
	m->hsync_end = m->hsync_start + hsw;
	m->htotal = m->hsync_end + hbp;
	m->vdisplay = va;
	m->vsync_start = va + vfp;
	m->vsync_end = m->vsync_start + vsw;
	m->vtotal = m->vsync_end + vbp;
	m->flags = (hs ? DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC) |
		   (vs ? DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC);
	m->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(m);

	lt->hsync_high = hs;
	lt->vsync_high = vs;
	lt->de_low = !de;
	lt->pclk_inv = pclk_edge >= 0 ? !!pclk_edge : pc;	/* DPCR bit 7 */

	of_property_read_u32(dev->of_node, "width-mm", &w_mm);
	of_property_read_u32(dev->of_node, "height-mm", &h_mm);
	m->width_mm = w_mm;
	m->height_mm = h_mm;

	if (ha > LT_FRAME_MAX_W || ha < 8 || va < 2) {
		ret = dev_err_probe(dev, -EINVAL, "unsupported size %ux%u\n", ha, va);
		goto out;
	}

	/* Fout(SCLK) = N MHz with the fixed XI = 10 MHz, R = 5, OD = 2 setup */
	lt->pll_n_sclk = sclk_mhz ? sclk_mhz : DIV_ROUND_CLOSEST(m->clock, 1000);
	if (lt->pll_n_sclk < 1 || lt->pll_n_sclk > 511)
		ret = dev_err_probe(dev, -EINVAL, "pixel clock %d kHz out of range\n", m->clock);
out:
	of_node_put(t);
	return ret;
}

static int lt7680_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct drm_device *drm;
	struct lt7680 *lt;
	size_t maxx;
	int ret;

	/*
	 * An spi_device has no DMA mask, so GEM DMA allocations (fbdev
	 * dumb buffer) fail with -ENOMEM. Give it a valid mask and, on
	 * newer kernels, allocate from the SPI controller's DMA-capable
	 * parent device instead.
	 */
	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	lt = devm_drm_dev_alloc(dev, &lt_drm_driver, struct lt7680, drm);
	if (IS_ERR(lt))
		return PTR_ERR(lt);
	drm = &lt->drm;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 17, 0)
	if (spi->controller->dev.parent)
		drm_dev_set_dma_dev(drm, spi->controller->dev.parent);
#endif
	lt->spi = spi;
	lt->panel = of_device_get_match_data(dev);
	lt->pageflip = pageflip;

	lt->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(lt->reset))
		return dev_err_probe(dev, PTR_ERR(lt->reset), "reset gpio\n");

	lt->bl = devm_gpiod_get_optional(dev, "backlight", GPIOD_OUT_LOW);
	if (IS_ERR(lt->bl))
		return dev_err_probe(dev, PTR_ERR(lt->bl), "backlight gpio\n");

	if (lt->panel) {
		lt->pcs = devm_gpiod_get(dev, "panel-cs", GPIOD_OUT_LOW);
		lt->psck = devm_gpiod_get(dev, "panel-sck", GPIOD_OUT_HIGH);
		lt->psdi = devm_gpiod_get(dev, "panel-sdi", GPIOD_OUT_HIGH);
		if (IS_ERR(lt->pcs) || IS_ERR(lt->psck) || IS_ERR(lt->psdi))
			return dev_err_probe(dev, -EINVAL,
					     "panel-cs/panel-sck/panel-sdi gpios required\n");
	}

	ret = lt_parse_timing(lt, dev);
	if (ret)
		return ret;

	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret)
		return ret;

	maxx = min_t(size_t, spi_max_transfer_size(spi), LT_CHUNK_MAX);
	lt->chunk = (maxx - 1) & ~1UL;
	if (lt->chunk < 64)
		return -EINVAL;
	lt->txbuf = devm_kmalloc(dev, lt->chunk + 1, GFP_KERNEL);
	lt->rowbuf = devm_kmalloc(dev, LT_FRAME_MAX_W * 4, GFP_KERNEL);
	if (!lt->txbuf || !lt->rowbuf)
		return -ENOMEM;

	/* chip bring-up in the order used by the EastRising tutorial */
	lt_hw_reset(lt);
	ret = lt_system_check(lt);
	if (!ret)
		ret = lt_wait_status(lt, ST_POWER_SAVING, false, 1000);
	if (ret)
		return dev_err_probe(dev, ret, "LT7680 not responding\n");

	lt_init_chip(lt);
	if (lt->err)
		return dev_err_probe(dev, lt->err, "chip init failed\n");
	if (lt->panel)
		nv3052_init(lt);
	lt->err = 0;

	/* DRM */
	ret = drmm_mode_config_init(drm);
	if (ret)
		return ret;
	drm->mode_config.min_width = lt->mode.hdisplay;
	drm->mode_config.max_width = lt->mode.hdisplay;
	drm->mode_config.min_height = lt->mode.vdisplay;
	drm->mode_config.max_height = lt->mode.vdisplay;
	drm->mode_config.funcs = &lt_mode_config_funcs;
	drm->mode_config.preferred_depth = 16;

	ret = drm_connector_init(drm, &lt->connector, &lt_conn_funcs, DRM_MODE_CONNECTOR_SPI);
	if (ret)
		return ret;
	drm_connector_helper_add(&lt->connector, &lt_conn_helper_funcs);

	ret = drm_simple_display_pipe_init(drm, &lt->pipe, &lt_pipe_funcs, lt_formats,
					   ARRAY_SIZE(lt_formats), NULL, &lt->connector);
	if (ret)
		return ret;
	drm_plane_enable_fb_damage_clips(&lt->pipe.plane);

	drm_mode_config_reset(drm);
	ret = drm_dev_register(drm, 0);
	if (ret)
		return ret;

	spi_set_drvdata(spi, drm);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
	drm_client_setup_with_fourcc(drm, DRM_FORMAT_RGB565);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
	drm_fbdev_dma_setup(drm, 16);
#else
	drm_fbdev_generic_setup(drm, 16);
#endif
	return 0;
}

static void lt7680_remove(struct spi_device *spi)
{
	struct drm_device *drm = spi_get_drvdata(spi);

	drm_dev_unplug(drm);
	drm_atomic_helper_shutdown(drm);
}

static void lt7680_shutdown(struct spi_device *spi)
{
	drm_atomic_helper_shutdown(spi_get_drvdata(spi));
}

static const struct lt7680_panel_cfg er_tft395_cfg = {
	.init = er_tft395_init,
	.n_init = ARRAY_SIZE(er_tft395_init),
};

static const struct of_device_id lt7680_of_match[] = {
	{ .compatible = "eastrising,er-tft3.95-1", .data = &er_tft395_cfg },
	{ .compatible = "levetop,lt7680" },	/* bare controller, no panel init */
	{ }
};
MODULE_DEVICE_TABLE(of, lt7680_of_match);

static const struct spi_device_id lt7680_spi_id[] = {
	{ "lt7680" },
	{ }
};
MODULE_DEVICE_TABLE(spi, lt7680_spi_id);

static struct spi_driver lt7680_spi_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = lt7680_of_match,
	},
	.id_table = lt7680_spi_id,
	.probe = lt7680_probe,
	.remove = lt7680_remove,
	.shutdown = lt7680_shutdown,
};
module_spi_driver(lt7680_spi_driver);

MODULE_DESCRIPTION("Levetop LT7680 SPI DRM driver (EastRising ER-TFT3.95-1)");
MODULE_LICENSE("GPL");
