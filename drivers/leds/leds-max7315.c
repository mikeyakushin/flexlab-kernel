/*
 * Copyright 2011
 *
 * author: Mike Yakushin <silicium@altlinux.ru>
 * Based on pca955x by Nate Case <ncase@xes-inc.com>
 *
 * This file is subject to the terms and conditions of version 2 of
 * the GNU General Public License.  See the file COPYING in the main
 * directory of this archive for more details.
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>

enum max7315_type {
	max7315,
};

struct max7315_chipdef {
	int			bits;
	u8			slv_addr;	/* 7-bit slave address mask */
	int			slv_addr_shift;	/* Number of bits to ignore */
};

static struct max7315_chipdef max7315_chipdefs[] = {
	[max7315] = {
		.bits		= 8,
		.slv_addr	= 0x0,
		.slv_addr_shift = 7,
	},
};

static const struct i2c_device_id max7315_id[] = {
	{ "max7315", max7315 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max7315_id);

struct max7315_led {
	struct max7315_chipdef	*chipdef;
	struct i2c_client	*client;
	struct work_struct	work;
	spinlock_t		lock;
	enum led_brightness	brightness;
	struct led_classdev	led_cdev;
	int			led_num;	/* 0 .. 15 potentially */
	char			name[32];
};


static void max7315_led_work(struct work_struct *work)
{
	struct max7315_led *led;
	u8 ls;
	int i,r,t,s,t1,t2;
	led = container_of(work, struct max7315_led, work);
	if(led->led_num >8)
	{
		printk("led->led_num > 8(%i)\n",led->led_num);
		return;
	}
	r=0x10+led->led_num/2;
	s=led->led_num%2;

	if(led->brightness==0)
	{
		ls=i2c_smbus_read_byte_data(led->client,0x01);
		ls=ls | (1 << led->led_num ) ;
		i2c_smbus_write_byte_data(led->client,0x01,ls);
		
		t=i2c_smbus_read_byte_data(led->client,r);
		t|=( 0xf << 4*s );
		i2c_smbus_write_byte_data(led->client,r,t);

		return;
	}
//set on
	ls=i2c_smbus_read_byte_data(led->client,0x01);
	ls=ls & ( ~(1 << led->led_num ));
	i2c_smbus_write_byte_data(led->client,0x01,ls);
//set brightness
	i=led->brightness/16;
	t=i2c_smbus_read_byte_data(led->client,r);
	t2=t&0xf0;
	t1=t&0x0f;
	
	if(s)
		t=(i<<4)|t1;
	else
		t=t2|i;
	i2c_smbus_write_byte_data(led->client,r,t);

}


static void max7315_led_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	struct max7315_led *led;
	
	led = container_of(led_cdev, struct max7315_led, led_cdev);
	
	spin_lock(&led->lock);
	led->brightness = value;
	
	/*
	* Must use workqueue for the actual I/O since I2C operations
	* can sleep.
	*/
	schedule_work(&led->work);
	
	spin_unlock(&led->lock);
}
		 

static int __devinit max7315_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct max7315_led *led;
	struct max7315_chipdef *chip;
	struct i2c_adapter *adapter;
	struct led_platform_data *pdata;
	int i, err;

	chip = &max7315_chipdefs[id->driver_data];
	adapter = to_i2c_adapter(client->dev.parent);
	pdata = client->dev.platform_data;

	/* Make sure the slave address / chip type combo given is possible */
	if ((client->addr & ~((1 << chip->slv_addr_shift) - 1)) !=
	    chip->slv_addr) {
		dev_err(&client->dev, "invalid slave address %02x\n",
				client->addr);
		return -ENODEV;
	}

	printk(KERN_INFO "leds-max7315: Using %s %d-bit LED driver at "
			"slave address 0x%02x\n",
			id->name, chip->bits, client->addr);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
	{
		printk(KERN_INFO "device not found\n");
		return -EIO;
	}

	if (pdata) {
		if (pdata->num_leds != chip->bits) {
			dev_err(&client->dev, "board info claims %d LEDs"
					" on a %d-bit chip\n",
					pdata->num_leds, chip->bits);
			return -ENODEV;
		}
	}

	led = kzalloc(sizeof(struct max7315_led) * chip->bits, GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	i2c_set_clientdata(client, led);
	i2c_smbus_write_byte_data(client,0x03,0);//Set all ports to output
	i2c_smbus_write_byte_data(client,0xf,0);//turn off interrupt, turn on intesity
	i2c_smbus_write_byte_data(client,0xe,0x80);//turn on PWM
	i2c_smbus_write_byte_data(client,0x01,0xff);
	for (i = 0; i < chip->bits; i++) {
		led[i].chipdef = chip;
		led[i].client = client;
		led[i].led_num = i;

		/* Platform data can specify LED names and default triggers */
		if (pdata) {
			if (pdata->leds[i].name)
				snprintf(led[i].name,
					 sizeof(led[i].name), "%s",
					 pdata->leds[i].name);
			if (pdata->leds[i].default_trigger)
				led[i].led_cdev.default_trigger =
					pdata->leds[i].default_trigger;
		} else {
			snprintf(led[i].name, sizeof(led[i].name),
				 "led:%d", i);
		}

		spin_lock_init(&led[i].lock);

		led[i].led_cdev.name= led[i].name;
		led[i].led_cdev.brightness_set = max7315_led_set;

		INIT_WORK(&led[i].work, max7315_led_work);

		err = led_classdev_register(&client->dev, &led[i].led_cdev);
		if (err < 0)
			goto exit;
	}

	return 0;

exit:
	while (i--) {
		led_classdev_unregister(&led[i].led_cdev);
		cancel_work_sync(&led[i].work);
	}

	kfree(led);
	i2c_set_clientdata(client, NULL);

	return err;
}

static int __devexit max7315_remove(struct i2c_client *client)
{
	struct max7315_led *led = i2c_get_clientdata(client);
	int i;

	for (i = 0; i < led->chipdef->bits; i++) {
		led_classdev_unregister(&led[i].led_cdev);
		cancel_work_sync(&led[i].work);
	}

	kfree(led);
	i2c_set_clientdata(client, NULL);

	return 0;
}

static struct i2c_driver max7315_driver = {
	.driver = {
		.name	= "leds-max7315",
		.owner	= THIS_MODULE,
	},
	.probe	= max7315_probe,
	.remove	= __devexit_p(max7315_remove),
	.id_table = max7315_id,
};

static int __init max7315_leds_init(void)
{
	return i2c_add_driver(&max7315_driver);
}

static void __exit max7315_leds_exit(void)
{
	i2c_del_driver(&max7315_driver);
}

module_init(max7315_leds_init);
module_exit(max7315_leds_exit);

MODULE_AUTHOR("Mike Yakushin <silicium@altlinux.ru>");
MODULE_DESCRIPTION("MAX7315 LED driver");
MODULE_LICENSE("GPL v2");
