/*
 * NV3052 panel init for ER-TFT3.95-1 (BOE 720x720), ported from the EastRising
 * Arduino tutorial (LCD_init.h). 3-wire, 9-bit SPI: first bit = D/C (0=cmd, 1=data).
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "nv3052.h"

/* Panel-init pins on GPIO0 port (DevKitC wiring from the tutorial) */
#define PIN_CS  14
#define PIN_SCK 13
#define PIN_SDI 12

static const struct gpio_dt_spec cs  = { .port = DEVICE_DT_GET(DT_NODELABEL(gpio0)), .pin = PIN_CS,  .dt_flags = GPIO_ACTIVE_HIGH };
static const struct gpio_dt_spec clk = { .port = DEVICE_DT_GET(DT_NODELABEL(gpio0)), .pin = PIN_SCK, .dt_flags = GPIO_ACTIVE_HIGH };
static const struct gpio_dt_spec sdi = { .port = DEVICE_DT_GET(DT_NODELABEL(gpio0)), .pin = PIN_SDI, .dt_flags = GPIO_ACTIVE_HIGH };

struct nv_cmd { uint8_t cmd; uint8_t data; };

static const struct nv_cmd init_table[] = {
#include "nv3052_table.inc"
};

static inline void bit_out(int dc_or_bit)
{
	gpio_pin_set_dt(&clk, 0);
	gpio_pin_set_dt(&sdi, dc_or_bit);
	gpio_pin_set_dt(&clk, 1);
}

static void send9(bool is_data, uint8_t v)
{
	gpio_pin_set_dt(&cs, 0);
	bit_out(is_data ? 1 : 0);
	for (int i = 0; i < 8; i++) {
		bit_out((v & 0x80) ? 1 : 0);
		v <<= 1;
	}
	gpio_pin_set_dt(&cs, 1);
}

int nv3052_init(void)
{
	const struct gpio_dt_spec *pins[] = { &cs, &clk, &sdi };

	for (size_t i = 0; i < ARRAY_SIZE(pins); i++) {
		if (!gpio_is_ready_dt(pins[i])) {
			return -ENODEV;
		}
		gpio_pin_configure_dt(pins[i], GPIO_OUTPUT_ACTIVE);
	}
	/* idle: CS high, CLK high */
	gpio_pin_set_dt(&cs, 1);
	gpio_pin_set_dt(&clk, 1);

	for (size_t i = 0; i < ARRAY_SIZE(init_table); i++) {
		send9(false, init_table[i].cmd);
		send9(true, init_table[i].data);
	}

	send9(false, 0x11);          /* sleep out */
	k_msleep(120);
	send9(false, 0x29);          /* display on */
	k_msleep(20);
	return 0;
}
