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
#include <linux/interrupt.h>
#include <mach/gpio.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
enum max7315k_type {
	max7315k,
};
static const struct i2c_device_id max7315k_id[] = {
	{ "max7315k", max7315k },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max7315k_id);

struct max7315_keys {
	struct i2c_client	*client;
	struct work_struct	work;
	u32			prevstate;
};
#define MAX_USERS 16
#define BUFFER_SIZE 256
static struct max7315_keys_buffer 
{
	u8 change[BUFFER_SIZE];
	u8 state[BUFFER_SIZE];
	int seek;
	struct mutex lock;
	wait_queue_head_t wait;
	int num;
	u8 ok;
} *keys_buffers[MAX_USERS];
struct mutex keys_buffer_lock;

static const char *keynames[]={"alarm","busy","come","stw","free"};
static const char *keystate[]={	"pressed","released"};
static void max7315_keys_work(struct work_struct *work)
{
	struct max7315_keys *k;
	int i;
	u32 state,change;
	k= container_of(work, struct max7315_keys, work);	
	if(unlikely(!k->client))
		BUG();
	state=i2c_smbus_read_byte_data(k->client,0x00);
	change=state^k->prevstate;
	if(!change)
		return;
	mutex_lock(&keys_buffer_lock);
	for(i=0;i<MAX_USERS;i++)
	{
		struct max7315_keys_buffer *b;
		if(!keys_buffers[i])
			continue;
		b=keys_buffers[i];
		mutex_lock(&b->lock);
		b->change[b->seek]=(u8)change;
		b->state[b->seek]=(u8)state;
		b->seek++;
		mutex_unlock(&b->lock);
		wake_up_interruptible(&b->wait);
	}
	mutex_unlock(&keys_buffer_lock);

	k->prevstate=state;
	
}


irqreturn_t max7315k_irq(int irq, void *v)
{
	struct max7315_keys *k=(struct max7315_keys*)v;
	schedule_work(&k->work);
	return IRQ_HANDLED;
}
static int max7315k_open(struct inode *inode,struct file *file)
{
	int i;
	struct max7315_keys_buffer *b;
	mutex_lock(&keys_buffer_lock);
	for(i=0;i<MAX_USERS;i++)
	{
		if(keys_buffers[i]==0)
			break;
	}
	if(i==MAX_USERS)
	{
		printk("max7315keys: maximum user number reached\n");
		mutex_unlock(&keys_buffer_lock);
		return -EAGAIN;
	}
	b=kzalloc(sizeof(struct max7315_keys_buffer),GFP_KERNEL);
	if(!b)
	{
		mutex_unlock(&keys_buffer_lock);
		return -ENOMEM;
	}
	keys_buffers[i]=b;
	mutex_unlock(&keys_buffer_lock);
	b->num=i;
	file->private_data=b;
	mutex_init(&b->lock);
	init_waitqueue_head(&b->wait);
	b->ok=1;
	return 0;
}
static int max7315k_read(struct file *file,char __user * buf,size_t count, loff_t *ppos)
{
	int readed=0;
	int ret=0;
	int i,j;
	struct max7315_keys_buffer *b=file->private_data;
	if(b->num>MAX_USERS)
		BUG();
	mutex_lock(&b->lock);
	if(b->seek==0)
	{
		mutex_unlock(&b->lock);
		ret=wait_event_interruptible(b->wait,b->seek>0);
		if(ret==-ERESTARTSYS)
			return ret;
	}
	mutex_lock(&b->lock);
	for(i=0;i<b->seek;i++)
	{
		for(j=0;j<5;j++)
		{
			if(test_bit(j,&b->change[i]))
			{
				ret=snprintf(buf,count,"%s %s\n",keynames[j],keystate[test_bit(j,&b->state[i])]);
				if(!ret)
					break;
				readed+=ret;
				*ppos+=ret;
				count-=ret;
			}
		}
	}
	b->seek=0;
	mutex_unlock(&b->lock);

	return readed;
}
static int max7315k_release(struct inode *indoe,struct file *file)
{
	struct max7315_keys_buffer *b=file->private_data;
	if(b->num>MAX_USERS)
		BUG();

	mutex_lock(&keys_buffer_lock);
	keys_buffers[b->num]=0;
	mutex_unlock(&keys_buffer_lock);
	
	mutex_lock(&b->lock);
	b->ok=0;
	mutex_unlock(&b->lock);
	
	kfree(b);
	return 0;
}
struct file_operations max7315misc_fops = {
	.owner = THIS_MODULE,
	.open=max7315k_open,
	.read=max7315k_read,
	.release=max7315k_release,
};
static struct miscdevice max7315misc = {11,"kurskeys",&max7315misc_fops,};
static int __devinit max7315k_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct max7315_keys *keys;
	struct i2c_adapter *adapter;
	int i, err;

	adapter = to_i2c_adapter(client->dev.parent);

	printk(KERN_INFO "keys-max7315: Using %s KEYS driver at "
			"slave address 0x%02x\n",
			id->name,  client->addr);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
	{
		printk(KERN_INFO "device not found\n");
		return -EIO;
	}
	
	keys = kzalloc(sizeof(struct max7315_keys), GFP_KERNEL);
	if (!keys)
		return -ENOMEM;

	i2c_set_clientdata(client, keys);
	keys->client=client;
	INIT_WORK(&keys->work, max7315_keys_work);
	err=request_irq(AT91_PIN_PC13,max7315k_irq,IRQF_SHARED,"max7315-keys",keys);
	if(err)
	{
		printk("Can`t request IRQ %i\n",err);
		goto exit;
	}
	misc_register(&max7315misc);
	return 0;

exit:	

	kfree(keys);
	i2c_set_clientdata(client, NULL);

	return err;
}

static int __devexit max7315k_remove(struct i2c_client *client)
{
	struct max7315_keys *k = i2c_get_clientdata(client);
	
	cancel_work_sync(&k->work);
	misc_deregister(&max7315misc);
	free_irq(AT91_PIN_PC13,k);
	kfree(k);
	i2c_set_clientdata(client, NULL);

	return 0;
}

static struct i2c_driver max7315k_driver = {
	.driver = {
		.name	= "max7315-keys",
		.owner	= THIS_MODULE,
	},
	.probe	= max7315k_probe,
	.remove	= __devexit_p(max7315k_remove),
	.id_table = max7315k_id,
};

static int __init max7315_keys_init(void)
{
	memset(keys_buffers,0,sizeof(void*)*MAX_USERS);
	mutex_init(&keys_buffer_lock);
	return i2c_add_driver(&max7315k_driver);
}

static void __exit max7315_keys_exit(void)
{
	i2c_del_driver(&max7315k_driver);
}

module_init(max7315_keys_init);
module_exit(max7315_keys_exit);

MODULE_AUTHOR("Mike Yakushin <silicium@altlinux.ru>");
MODULE_DESCRIPTION("MAX7315 KEYS driver");
MODULE_LICENSE("GPL v2");
