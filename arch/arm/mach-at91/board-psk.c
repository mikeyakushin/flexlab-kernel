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


static void __init psk_map_io(void)
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

static void __init psk_init_irq(void)
{
	at91rm9200_init_interrupts(NULL);
}

static struct at91_eth_data __initdata psk_eth_data = {
	.phy_irq_pin	= 0,
	.is_rmii	= 0,
};
static struct at91_usbh_data __initdata psk_usbh_data = {
	.ports		= 1,
};

static struct at91_udc_data __initdata psk_udc_data = {
	.vbus_pin	= 0,
	.pullup_pin	= 0,
};

static struct at91_mmc_data __initdata psk_mmc_data = {
	.slot_b		= 0,
	.wire4		= 1,
	.det_pin	=  AT91_PIN_PB23,
};

#define PSK_FLASH_BASE	AT91_CHIPSELECT_0
#define PSK_FLASH_SIZE	SZ_8M
static struct mtd_partition psk_part[] = {
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

static void psk_flash_set_vpp(struct map_info* mi,int i)
{
}

static struct physmap_flash_data psk_flash_data = {
	.parts = psk_part,
	.width = 2, 
	.nr_parts = 3,
	.set_vpp = psk_flash_set_vpp,
};

static struct resource psk_flash_resource = {
	.start		= PSK_FLASH_BASE,
	.end		= PSK_FLASH_BASE + PSK_FLASH_SIZE - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device psk_flash = {
	.name		= "physmap-flash",
	.id		= 0,
	.dev		= {
				.platform_data	= &psk_flash_data,
			},
	.resource	= &psk_flash_resource,
	.num_resources	= 1,
};

static struct gpio_led psk_leds[] = {
	{
		.name			= "LED0",
		.gpio			= AT91_PIN_PC10,
		.active_low		= 0,
		.default_trigger	= "none",
	},
	{
		.name			= "LED1",
		.gpio			= AT91_PIN_PC11,
		.active_low		= 0,
		.default_trigger	= "none",
	},
	{
		.name			= "LED2",
		.gpio			= AT91_PIN_PC12,
		.active_low		= 0,
		.default_trigger	= "none",
	},
	{
		.name			= "LED3",
		.gpio			= AT91_PIN_PC13,
		.active_low		= 0,
		.default_trigger	= "none",
	},
	{
		.name			= "LED4",
		.gpio			= AT91_PIN_PC14,
		.active_low		= 0,
		.default_trigger	= "none",
	},
	{
		.name			= "LED5",
		.gpio			= AT91_PIN_PC15,
		.active_low		= 0,
		.default_trigger	= "none",
	}
};

static struct i2c_board_info __initdata psk_i2c_devices[] = {
	{
		I2C_BOARD_INFO("ds1672", 0x68),
	},
};


static void __init psk_board_init(void)
{
	/* Serial */
	at91_add_device_serial();
	/* Ethernet */
	at91_add_device_eth(&psk_eth_data);
	/* USB Host */
	at91_add_device_usbh(&psk_usbh_data);
	/* USB Device */
	at91_add_device_udc(&psk_udc_data);
	at91_set_multi_drive(psk_udc_data.pullup_pin, 1);	/* pullup_pin is connected to reset */
	/* I2C */
	at91_add_device_i2c(psk_i2c_devices, ARRAY_SIZE(psk_i2c_devices));
	/* SPI */
	//at91_add_device_spi(ataman_spi_devices, ARRAY_SIZE(ataman_spi_devices));
	/* DataFlash card */
//	at91_set_gpio_output(AT91_PIN_PB7, 0);

	/* MMC */
//	at91_set_gpio_output(AT91_PIN_PB7, 1);	/* this MMC card slot can optionally use SPI signaling (CS3). */
	at91_add_device_mmc(0, &psk_mmc_data);

	/* Parallel Flash */
	platform_device_register(&psk_flash);
	/* LEDs */
	at91_gpio_leds(psk_leds, ARRAY_SIZE(psk_leds));
	/* VGA */
//	ataman_add_device_video();
}

MACHINE_START(PSK, "Radio Complex PSK Board (c) FlexLab")
	/* Maintainer: Wagan Sarukhanov/FlexLab */
	.phys_io	= AT91_BASE_SYS,
	.io_pg_offst	= (AT91_VA_BASE_SYS >> 18) & 0xfffc,
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91rm9200_timer,
	.map_io		= psk_map_io,
	.init_irq	= psk_init_irq,
	.init_machine	= psk_board_init,
MACHINE_END
