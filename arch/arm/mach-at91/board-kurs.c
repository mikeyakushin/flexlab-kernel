/*
 * (c) Copyright 2011 FlexLab Ltd.
 * (c) Copyright 2011 Radio Complex Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <linux/mtd/physmap.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/at91rm9200_mc.h>
#include <asm/mach/flash.h>
#include "generic.h"


static void __init kurs_map_io(void)
{
	/* Initialize processor: 18.432 MHz crystal */
	at91rm9200_initialize(18432000, AT91RM9200_PQFP);

	/* DBGU on ttyS0. (Rx & Tx only) */
	at91_register_uart(0, 0, 0);

	/* USART1 on ttyS1. (Rx, Tx, CTS, RTS, DTR, DSR, DCD, RI) */
	at91_register_uart(AT91RM9200_ID_US0, 1, 0);
	at91_register_uart(AT91RM9200_ID_US1, 2, 0);
	at91_register_uart(AT91RM9200_ID_US2, 3, 0);
	at91_register_uart(AT91RM9200_ID_US3, 4, 0);

	/* set serial console to ttyS0 (ie, DBGU) */
	at91_set_serial_console(0);
}

static void __init kurs_init_irq(void)
{
	at91rm9200_init_interrupts(NULL);
}

static struct at91_eth_data __initdata kurs_eth_data = {
	.phy_irq_pin	= 0,
	.is_rmii	= 0,
};
static struct at91_usbh_data __initdata kurs_usbh_data = {
	.ports		= 1,
};

static struct at91_udc_data __initdata kurs_udc_data = {
	.vbus_pin	= 0,
	.pullup_pin	= 0,
};

static struct at91_mmc_data __initdata kurs_mmc_data = {
	.slot_b		= 0,
	.wire4		= 1,
	.det_pin	=  AT91_PIN_PB23,
};

#define KURS_FLASH_BASE	AT91_CHIPSELECT_0
#define KURS_FLASH_SIZE	SZ_8M
static struct mtd_partition kurs_part[] = {
	{
		.name = "uboot",
		.size = 0x30000,//196KiB
		.offset = 0,
	},
	{
		.name = "uImage",
		.size = 2*1024*1024, //2MiB
		.offset = MTDPART_OFS_APPEND,
	},
	{
		.name = "root",
		.size = MTDPART_SIZ_FULL ,
		.offset = MTDPART_OFS_APPEND,
	},

};

static void kurs_flash_set_vpp(struct map_info* mi,int i)
{
}

static struct physmap_flash_data kurs_flash_data = {
	.parts = kurs_part,
	.width = 2, 
	.nr_parts = 3,
	.set_vpp = kurs_flash_set_vpp,
};

static struct resource kurs_flash_resource = {
	.start		= KURS_FLASH_BASE,
	.end		= KURS_FLASH_BASE + KURS_FLASH_SIZE - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device kurs_flash = {
	.name		= "physmap-flash",
	.id		= 0,
	.dev		= {
				.platform_data	= &kurs_flash_data,
			},
	.resource	= &kurs_flash_resource,
	.num_resources	= 1,
};
static struct led_info kurs_leds[] = {
	{ 
		.name = "gsm",
		.default_trigger = "default-on",
		.flags= 0,
	},
	{ 
		.name = "gnss",
		.default_trigger = "default-on",
		.flags= 0,
	},
	{ 
		.name = "ofab",
		.default_trigger = "default-on",
		.flags= 0,
	}, 
	{
		.name = "busy",
		.default_trigger = "default-on",
		.flags= 0,
	},
	{
		.name = "come",
		.default_trigger = "default-on",
		.flags= 0,
	},
	{
		.name = "alarm",
		.default_trigger = "default-on",
		.flags= 0,
	},
	{
		.name = "stw",
		.default_trigger = "default-on",
		.flags= 0,
	},
	{ 
		.name = "free",
		.default_trigger = "default-on",
		.flags= 0,
	},
};
static struct led_platform_data kurs_led_pdata = {
	.num_leds=8,
	.leds=kurs_leds
};
static struct i2c_board_info __initdata kurs_i2c_devices[] = {
	{
		I2C_BOARD_INFO("ds1672", 0x68),
	},
	{
		I2C_BOARD_INFO("max7315", 0x20),
		.platform_data=&kurs_led_pdata,
	},
};


static void __init kurs_board_init(void)
{
	/* Serial */
	at91_add_device_serial();
	/* Ethernet */
	at91_add_device_eth(&kurs_eth_data);
	/* USB Host */
	at91_add_device_usbh(&kurs_usbh_data);
	/* USB Device */
	at91_add_device_udc(&kurs_udc_data);
	at91_set_multi_drive(kurs_udc_data.pullup_pin, 1);	/* pullup_pin is connected to reset */
	/* I2C */
	at91_add_device_i2c(kurs_i2c_devices, ARRAY_SIZE(kurs_i2c_devices));
	/* SPI */
	//at91_add_device_spi(ataman_spi_devices, ARRAY_SIZE(ataman_spi_devices));
	/* DataFlash card */
//	at91_set_gpio_output(AT91_PIN_PB7, 0);

	/* MMC */
//	at91_set_gpio_output(AT91_PIN_PB7, 1);	/* this MMC card slot can optionally use SPI signaling (CS3). */
//	at91_add_device_mmc(0, &kurs_mmc_data);

	/* Parallel Flash */
	platform_device_register(&kurs_flash);
	/* LEDs */
//	at91_gpio_leds(psk_leds, ARRAY_SIZE(psk_leds));
	/* VGA */
//	ataman_add_device_video();
}

MACHINE_START(KURS, "Radio Complex KURS Board (c) FlexLab")
	/* Maintainer: Wagan Sarukhanov/FlexLab */
	.phys_io	= AT91_BASE_SYS,
	.io_pg_offst	= (AT91_VA_BASE_SYS >> 18) & 0xfffc,
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91rm9200_timer,
	.map_io		= kurs_map_io,
	.init_irq	= kurs_init_irq,
	.init_machine	= kurs_board_init,
MACHINE_END
