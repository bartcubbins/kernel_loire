/*
 * Copyright (C) 2010 Trusted Logic S.A.
 * Copyright (C) 2019 Pavel Dubrova <pashadubrova@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/pn547.h>
#include <linux/clk.h>
#include <linux/device.h>

#include <linux/of_gpio.h>

#define MAX_BUFFER_SIZE		512
#define MAX_GPIOMUX_SET_SIZE	4

struct pn547_dev {
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct i2c_client	*client;
	struct miscdevice	pn547_device;
	struct pinctrl		*pinctrl;
	enum pn547_state	state;
	unsigned int 		ven_gpio;
	unsigned int 		firm_gpio;
	unsigned int		irq_gpio;
	bool			irq_enabled;
	spinlock_t		irq_enabled_lock;
};

static void pn547_disable_irq(struct pn547_dev *pn547_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn547_dev->irq_enabled_lock, flags);
	if (pn547_dev->irq_enabled) {
		disable_irq_nosync(pn547_dev->client->irq);
		pn547_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&pn547_dev->irq_enabled_lock, flags);
}

static irqreturn_t pn547_dev_irq_handler(int irq, void *dev_id)
{
	struct pn547_dev *pn547_dev = dev_id;

	pn547_disable_irq(pn547_dev);

	/* Wake up waiting readers */
	wake_up(&pn547_dev->read_wq);

	return IRQ_HANDLED;
}

static ssize_t pn547_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn547_dev *pn547_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	pr_debug("%s : reading %zu bytes.\n", __func__, count);

	mutex_lock(&pn547_dev->read_mutex);

	if (!gpio_get_value(pn547_dev->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto fail;
		}

		while (1) {
			pn547_dev->irq_enabled = true;
			enable_irq(pn547_dev->client->irq);
			ret = wait_event_interruptible(
					pn547_dev->read_wq,
					!pn547_dev->irq_enabled);

			pn547_disable_irq(pn547_dev);

			if (ret)
				goto fail;

			if (gpio_get_value(pn547_dev->irq_gpio))
				break;

			pr_debug("%s: spurious interrupt detected\n", __func__);
		}
	}

	/* Read data */
	ret = i2c_master_recv(pn547_dev->client, tmp, count);

	mutex_unlock(&pn547_dev->read_mutex);

	/* pn547 seems to be slow in handling I2C read requests
	 * so add 1ms delay after recv operation */
	udelay(1000);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}
	if (copy_to_user(buf, tmp, ret)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	return ret;

fail:
	mutex_unlock(&pn547_dev->read_mutex);
	return ret;
}

static ssize_t pn547_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn547_dev  *pn547_dev;
	char tmp[MAX_BUFFER_SIZE];
	int try, ret;

	pn547_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	pr_debug("%s : writing %zu bytes.\n", __func__, count);

	for (try = 0; try < 3; try++) {
		/* Write data */
		ret = i2c_master_send(pn547_dev->client, tmp, count);
		if (ret < 0) {
			pr_debug("%s: write failed, maybe in standby mode. Retry(%d)\n",
					__func__, try);
			usleep_range(1000, 1100);
		} else if (ret == count)
			break;
	}

	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}

	/* pn547 seems to be slow in handling I2C write requests
	 * so add 1ms delay after I2C send oparation */
	udelay(1000);

	return ret;
}

static int pn547_dev_open(struct inode *inode, struct file *filp)
{
	struct pn547_dev *pn547_dev = container_of(filp->private_data,
						struct pn547_dev,
						pn547_device);

	filp->private_data = pn547_dev;

	pr_debug("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}

static int pn547_pinctrl_config(struct pn547_dev *dev, uint8_t active)
{
	struct pinctrl_state *state;
	const char *name = active ? "pn547-active" : "pn547-inactive";

	state = pinctrl_lookup_state(dev->pinctrl, name);
	if (IS_ERR(state)) {
		pr_err("%s: pinctrol lookup state failed\n",
				__func__);
		return PTR_ERR(state);
	}

	return pinctrl_select_state(dev->pinctrl, state);
}

static void pn547_pinctrl_destroy(struct pn547_dev *dev)
{
	int ret = 0;

	ret = pn547_pinctrl_config(dev, 0);
	if (ret)
		pr_err("%s: pinctrol failed on destroy %d\n",
			__func__, ret);

	devm_pinctrl_put(dev->pinctrl);
}

static long  pn547_dev_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct pn547_dev *pn547_dev = filp->private_data;
	int ret, state;

	/* Activate pinctrol before bit banging. */
	ret = pn547_pinctrl_config(pn547_dev, 1);
	if (ret) {
		pr_err("%s: pinctrol failed on chip configuration %d\n",
				__func__, ret);
		return ret;
	}

	switch (cmd) {
	case PN547_SET_PWR:
		if (arg == 2) {
			/* power on with firmware download (requires hw reset)
			 */
			pr_info("%s power on with firmware\n", __func__);
			gpio_set_value(pn547_dev->ven_gpio, 1);
			msleep(20);
			if (pn547_dev->firm_gpio)
				gpio_set_value(pn547_dev->firm_gpio, 1);
			msleep(20);
			gpio_set_value(pn547_dev->ven_gpio, 0);
			msleep(100);
			gpio_set_value(pn547_dev->ven_gpio, 1);
			msleep(20);
			state = PN547_STATE_FWDL;
			msleep(10);
		} else if (arg == 1) {
			/* power on */
			pr_info("%s power on\n", __func__);
			if (pn547_dev->firm_gpio)
				gpio_set_value(pn547_dev->firm_gpio, 0);
			gpio_set_value(pn547_dev->ven_gpio, 1);
			msleep(100);
			state = PN547_STATE_ON;
			msleep(10);
		} else  if (arg == 0) {
			/* power off */
			pr_info("%s power off\n", __func__);
			if (pn547_dev->firm_gpio)
				gpio_set_value(pn547_dev->firm_gpio, 0);
			gpio_set_value(pn547_dev->ven_gpio, 0);
			msleep(100);
			/* Suspend pinctrol when PN547 is turned off. */
			state = PN547_STATE_OFF;
			ret = pn547_pinctrl_config(pn547_dev, 0);
			if (ret) {
				pr_err("%s: pinctrol failed on PN547_STATE_OFF %d\n",
						__func__, ret);
				return ret;
			}
			msleep(60);
		} else {
			pr_err("%s bad arg %lu\n", __func__, arg);
			return -EINVAL;
		}
		break;
	default:
		state = PN547_STATE_UNKNOWN;
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations pn547_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn547_dev_read,
	.write	= pn547_dev_write,
	.open	= pn547_dev_open,
	.unlocked_ioctl  = pn547_dev_ioctl,
};

static int pn547_parse_dt(struct device *dev,
			 struct pn547_i2c_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	const unsigned char *buf;
	int i;
	int ret = 0;

	ret = of_get_named_gpio(np, "nxp,irq_gpio", 0);
	if (ret < 0) {
		dev_err(dev, "failed to get \"nxp,irq_gpio\"\n");
		goto err;
	}
	pdata->irq_gpio = ret;

	ret = of_get_named_gpio(np, "nxp,dwld_en", 0);
	if (ret < 0) {
		dev_err(dev, "failed to get \"nxp,dwld_en\"\n");
		goto err;
	}
	pdata->firm_gpio = ret;

    /*
	ret = of_get_named_gpio(np, "nxp,pvdd_en", 0);
	if (ret < 0) {
		dev_err(dev, "failed to get \"nxp,pvdd_en\"\n");
		goto err;
	}
	pdata->pvdd_en_gpio = ret;
    */

	ret = of_get_named_gpio(np, "nxp,ven", 0);
	if (ret < 0) {
		dev_err(dev, "failed to get \"nxp,ven\"\n");
		goto err;
	}
	pdata->ven_gpio = ret;

	buf = of_get_property(np, "gpio_fwdl_enable", NULL);
	if (buf == NULL) {
		dev_dbg(dev, "failed to get \"gpio_fwdl_enable\"\n");
	} else {
		for (i = 0; i < MAX_GPIOMUX_SET_SIZE; i++) {
			pdata->gpio_fwdl_enable[i] = (buf[i * 4] << 3) +
				(buf[i * 4 + 1] << 2) + (buf[i * 4 + 2] << 1) +
				buf[i * 4 + 3];
			dev_dbg(dev, "gpio_fwdl_enable[%d]=%d", i,
				pdata->gpio_fwdl_enable[i]);
		}
	}

	buf = of_get_property(np, "gpio_fwdl_disable", NULL);
	if (buf == NULL) {
		dev_dbg(dev, "failed to get \"gpio_fwdl_disable\"\n");
	} else {
		for (i = 0; i < MAX_GPIOMUX_SET_SIZE; i++) {
			pdata->gpio_fwdl_disable[i] = (buf[i * 4] << 3) +
				(buf[i * 4 + 1] << 2) + (buf[i * 4 + 2] << 1) +
				buf[i * 4 + 3];
			dev_dbg(dev, "gpio_fwdl_disable[%d]=%d", i,
				pdata->gpio_fwdl_disable[i]);
		}
	}

	pr_info("%s: irq_gpio: %d, firm_gpio: %d, ven_gpio: %d", __func__,
			pdata->irq_gpio, pdata->firm_gpio, pdata->ven_gpio);

err:
	return ret;
}

static int pn547_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	struct pn547_i2c_platform_data *platform_data;
	struct pn547_dev *pn547_dev;
	struct clk *clk_rf = NULL;
	struct pinctrl *pinctrl;

	pr_info("Probe pn547 driver\n");

	platform_data = kzalloc(sizeof(struct pn547_i2c_platform_data),
			GFP_KERNEL);
	if (!platform_data) {
		dev_err(&client->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	if (platform_data == NULL) {
		pr_err("%s : nfc probe fail\n", __func__);
		return  -ENODEV;
	}

	pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pinctrl)) {
		dev_err(&client->dev, "pinctrl get failed\n");
		return PTR_ERR(pinctrl);
	}

	ret = pn547_parse_dt(&client->dev, platform_data);
	if (ret < 0) {
		dev_err(&client->dev, "failed to parse device tree: %d\n", ret);
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	ret = gpio_request(platform_data->irq_gpio, "pn547_irq");
	if (ret)
		return  -ENODEV;
	ret = gpio_request(platform_data->ven_gpio, "pn547_ven");
	if (ret)
		goto err_ven;
	if (platform_data->firm_gpio) {
		ret = gpio_request(platform_data->firm_gpio, "pn547_fw");
		if (ret)
			goto err_firm;
	}

	pn547_dev = kzalloc(sizeof(*pn547_dev), GFP_KERNEL);
	if (pn547_dev == NULL) {
		dev_err(&client->dev,
				"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	pn547_dev->irq_gpio = platform_data->irq_gpio;
	pn547_dev->ven_gpio  = platform_data->ven_gpio;
	pn547_dev->firm_gpio  = platform_data->firm_gpio;
	pn547_dev->client   = client;
	pn547_dev->pinctrl = pinctrl;

	pr_debug("dev name:%s", dev_name(&client->dev));

	ret = gpio_direction_input(pn547_dev->irq_gpio);
	if (ret < 0) {
		pr_err("%s :not able to set irq_gpio as input\n", __func__);
		goto err_ven;
	}
	ret = gpio_direction_output(pn547_dev->ven_gpio, 0);
	if (ret < 0) {
		pr_err("%s : not able to set ven_gpio as output\n", __func__);
		goto err_firm;
	}
	if (platform_data->firm_gpio) {
		ret = gpio_direction_output(pn547_dev->firm_gpio, 0);
		if (ret < 0) {
			pr_err("%s : not able to set firm_gpio as output\n",
				 __func__);
			goto err_exit;
		}
	}

	/* init mutex and queues */
	init_waitqueue_head(&pn547_dev->read_wq);
	mutex_init(&pn547_dev->read_mutex);
	spin_lock_init(&pn547_dev->irq_enabled_lock);

	pn547_dev->pn547_device.minor = MISC_DYNAMIC_MINOR;
	pn547_dev->pn547_device.name = "pn54x";
	pn547_dev->pn547_device.fops = &pn547_dev_fops;

	ret = misc_register(&pn547_dev->pn547_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	clk_rf = clk_get(&client->dev, "nfc_clk");
	if (IS_ERR(clk_rf)) {
	    pr_err("Couldn't get nfc_clk\n");
	    goto err_exit;
	}

	ret = clk_prepare_enable(clk_rf);
	if (ret) {
	    pr_err("clk_rf enable failed\n");
	    goto err_exit;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	pr_info("%s: requesting IRQ %d\n", __func__, client->irq);
	pn547_dev->irq_enabled = true;
	ret = request_irq(client->irq, pn547_dev_irq_handler,
			  IRQF_TRIGGER_RISING, client->name, pn547_dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}
	pn547_disable_irq(pn547_dev);
	i2c_set_clientdata(client, pn547_dev);

	pr_info("%s: probing exited successfully\n", __func__);

	return 0;

err_request_irq_failed:
	misc_deregister(&pn547_dev->pn547_device);
err_misc_register:
	mutex_destroy(&pn547_dev->read_mutex);
	kfree(pn547_dev);
err_exit:
	if (pn547_dev->firm_gpio)
		gpio_free(platform_data->firm_gpio);
err_firm:
	gpio_free(platform_data->ven_gpio);
err_ven:
	gpio_free(platform_data->irq_gpio);
	return ret;
}

static int pn547_remove(struct i2c_client *client)
{
	struct pn547_dev *pn547_dev;

	pn547_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn547_dev);
	misc_deregister(&pn547_dev->pn547_device);
	mutex_destroy(&pn547_dev->read_mutex);
	gpio_free(pn547_dev->irq_gpio);
	gpio_free(pn547_dev->ven_gpio);
	if (pn547_dev->firm_gpio)
		gpio_free(pn547_dev->firm_gpio);
	pn547_pinctrl_destroy(pn547_dev);
	kfree(pn547_dev);

	return 0;
}

static int pn547_pm_suspend(struct device *dev)
{
    struct pn547_dev *d = dev_get_drvdata(dev);

    if (!mutex_trylock(&d->read_mutex))
        return -EAGAIN;
    irq_set_irq_wake(d->client->irq, 1);
    mutex_unlock(&d->read_mutex);

    return 0;
}

static int pn547_pm_resume(struct device *dev)
{
    struct pn547_dev *d = dev_get_drvdata(dev);

    mutex_lock(&d->read_mutex);
    irq_set_irq_wake(d->client->irq, 0);
    mutex_unlock(&d->read_mutex);

    return 0;
}

static const struct i2c_device_id pn547_id[] = {
	{ "PN547_DEVICE_NAME", 0 },
	{ },
};

static struct of_device_id pn547_match_table[] = {
	{ .compatible = "nxp,pn547", },
	{ },
};

static const struct dev_pm_ops pn547_pm_ops = {
    .suspend = pn547_pm_suspend,
    .resume  = pn547_pm_resume,
};

static struct i2c_driver pn547_driver = {
	.id_table	= pn547_id,
	.probe		= pn547_probe,
	.remove		= pn547_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "PN547_DEVICE_NAME",
	    .pm     = &pn547_pm_ops,
		.of_match_table	= pn547_match_table,
	},
};

static int __init pn547_dev_init(void)
{
	pr_info("Loading pn547 driver\n");
	return i2c_add_driver(&pn547_driver);
}
module_init(pn547_dev_init);

static void __exit pn547_dev_exit(void)
{
	pr_info("Unloading pn547 driver\n");
	i2c_del_driver(&pn547_driver);
}
module_exit(pn547_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC PN547 driver");
MODULE_LICENSE("GPL v2");
