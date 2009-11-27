/*
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


static void __init ataman_map_io(void)
{
	/* Initialize processor: 18.432 MHz crystal */
	at91rm9200_initialize(18432000, AT91RM9200_BGA);

	/* DBGU on ttyS0. (Rx & Tx only) */
	at91_register_uart(0, 0, 0);

	/* USART1 on ttyS1. (Rx, Tx, CTS, RTS, DTR, DSR, DCD, RI) */
	at91_register_uart(AT91RM9200_ID_US0, 1, ATMEL_UART_CTS | ATMEL_UART_RTS);
	at91_register_uart(AT91RM9200_ID_US1, 2, ATMEL_UART_CTS | ATMEL_UART_RTS);
	at91_register_uart(AT91RM9200_ID_US2, 3, 0);
	at91_register_uart(AT91RM9200_ID_US3, 4, ATMEL_UART_CTS | ATMEL_UART_RTS);

	/* set serial console to ttyS0 (ie, DBGU) */
	at91_set_serial_console(0);
}

static void __init ataman_init_irq(void)
{
	at91rm9200_init_interrupts(NULL);
}

static struct at91_eth_data __initdata ataman_eth_data = {
	.phy_irq_pin	= 0,
	.is_rmii	= 0,
};
static struct at91_usbh_data __initdata ataman_usbh_data = {
	.ports		= 1,
};

static struct at91_udc_data __initdata ataman_udc_data = {
	.vbus_pin	= AT91_PIN_PB28,
	.pullup_pin	= AT91_PIN_PB29,
};

static struct at91_mmc_data __initdata ataman_mmc_data = {
	.slot_b		= 0,
	.wire4		= 1,
	.wp_pin         =  AT91_PIN_PA24,
	.det_pin	=  AT91_PIN_PB2,
};

#define ATAMAN_FLASH_BASE	AT91_CHIPSELECT_0
#define ATAMAN_FLASH_SIZE	SZ_8M
static struct mtd_partition ataman_part[] = {
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

static void ataman_flash_set_vpp(struct map_info* mi,int i)
{
}
static struct physmap_flash_data ataman_flash_data = {
	.parts = ataman_part,
	.width = 2, 
	.nr_parts = 3,
	.set_vpp = ataman_flash_set_vpp,
};

static struct resource ataman_flash_resource = {
	.start		= ATAMAN_FLASH_BASE,
	.end		= ATAMAN_FLASH_BASE + ATAMAN_FLASH_SIZE - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device ataman_flash = {
	.name		= "physmap-flash",
	.id		= 0,
	.dev		= {
				.platform_data	= &ataman_flash_data,
			},
	.resource	= &ataman_flash_resource,
	.num_resources	= 1,
};

static struct gpio_led ataman_leds[] = {
	{
		.name			= "D2",
		.gpio			= AT91_PIN_PC10,
		.active_low		= 0,
		.default_trigger	= "heartbeat",
	},
	{
		.name			= "D3",
		.gpio			= AT91_PIN_PC11,
		.active_low		= 0,
		.default_trigger	= "heartbeat",
	},
	{
		.name			= "D4",
		.gpio			= AT91_PIN_PC12,
		.active_low		= 0,
		.default_trigger	= "heartbeat",
	},
		{
		.name			= "D5",
		.gpio			= AT91_PIN_PC13,
		.active_low		= 0,
		.default_trigger	= "heartbeat",
	}


};
static struct i2c_board_info __initdata flcpu_i2c_devices[] = {
	{
		I2C_BOARD_INFO("ds1672", 0x68),
	},
};


static void __init ataman_board_init(void)
{
	/* Serial */
	at91_add_device_serial();
	/* Ethernet */
	at91_add_device_eth(&ataman_eth_data);
	/* USB Host */
	at91_add_device_usbh(&ataman_usbh_data);
	/* USB Device */
	at91_add_device_udc(&ataman_udc_data);
	at91_set_multi_drive(ataman_udc_data.pullup_pin, 1);	/* pullup_pin is connected to reset */
	/* I2C */
	at91_add_device_i2c(flcpu_i2c_devices, ARRAY_SIZE(flcpu_i2c_devices));
	/* SPI */
	//at91_add_device_spi(ataman_spi_devices, ARRAY_SIZE(ataman_spi_devices));
	/* DataFlash card */
//	at91_set_gpio_output(AT91_PIN_PB7, 0);

	/* MMC */
//	at91_set_gpio_output(AT91_PIN_PB7, 1);	/* this MMC card slot can optionally use SPI signaling (CS3). */
	at91_add_device_mmc(0, &ataman_mmc_data);

	/* Parallel Flash */
	platform_device_register(&ataman_flash);
	/* LEDs */
	at91_gpio_leds(ataman_leds, ARRAY_SIZE(ataman_leds));
	/* VGA */
//	ataman_add_device_video();
}

MACHINE_START(FLCPU, "FlexLAB CPUBoard")
	/* Maintainer: SAN People/Atmel */
	.phys_io	= AT91_BASE_SYS,
	.io_pg_offst	= (AT91_VA_BASE_SYS >> 18) & 0xfffc,
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91rm9200_timer,
	.map_io		= ataman_map_io,
	.init_irq	= ataman_init_irq,
	.init_machine	= ataman_board_init,
MACHINE_END
