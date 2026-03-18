// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Amlogic WiFi platform driver for mainline kernel
 * Adapted from CoreELEC common_drivers
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#define WIFI_INFO(fmt, args...) pr_info("[wifi_dt] " fmt, ##args)
#define WIFI_DEBUG(fmt, args...) pr_debug("[wifi_dt] " fmt, ##args)

struct wifi_plat_info {
	int interrupt_pin;
	int irq_num;
	int irq_trigger_type;

	int power_on_pin;
	int power_on_pin_level;
	int power_on_pin2;

	int plat_info_valid;
	struct device *dev;
};

static struct wifi_plat_info wifi_info;
static DEFINE_MUTEX(wifi_power_mutex);
static int wifi_power_state;

#define BT_BIT	0
#define WIFI_BIT	1

static int set_power(int value)
{
	if (!gpio_is_valid(wifi_info.power_on_pin))
		return -EINVAL;

	if (wifi_info.power_on_pin_level)
		return gpio_direction_output(wifi_info.power_on_pin, !value);
	else
		return gpio_direction_output(wifi_info.power_on_pin, value);
}

static int set_power2(int value)
{
	if (!gpio_is_valid(wifi_info.power_on_pin2))
		return 0;

	if (wifi_info.power_on_pin_level)
		return gpio_direction_output(wifi_info.power_on_pin2, !value);
	else
		return gpio_direction_output(wifi_info.power_on_pin2, value);
}

static int set_wifi_power(int is_power)
{
	int ret = 0;

	WIFI_DEBUG("power %s\n", is_power ? "UP" : "DOWN");

	if (is_power) {
		if (gpio_is_valid(wifi_info.power_on_pin)) {
			ret = set_power(1);
			if (ret)
				WIFI_INFO("power up failed(%d)\n", ret);
		}
		if (gpio_is_valid(wifi_info.power_on_pin2)) {
			ret = set_power2(1);
			if (ret)
				WIFI_INFO("power2 up failed(%d)\n", ret);
		}
	} else {
		if (gpio_is_valid(wifi_info.power_on_pin)) {
			ret = set_power(0);
			if (ret)
				WIFI_INFO("power down failed(%d)\n", ret);
		}
		if (gpio_is_valid(wifi_info.power_on_pin2)) {
			ret = set_power2(0);
			if (ret)
				WIFI_INFO("power2 down failed(%d)\n", ret);
		}
	}
	return ret;
}

static void wifi_power_control(int is_power, int shift)
{
	mutex_lock(&wifi_power_mutex);
	if (is_power) {
		if (!wifi_power_state) {
			set_wifi_power(1);
			WIFI_INFO("Set %s power on\n", (shift ? "WiFi" : "BT"));
			msleep(200);
		}
		wifi_power_state |= (1 << shift);
	} else {
		wifi_power_state &= ~(1 << shift);
		if (!wifi_power_state) {
			set_wifi_power(0);
			WIFI_INFO("Set %s power down\n", (shift ? "WiFi" : "BT"));
			msleep(200);
		}
	}
	mutex_unlock(&wifi_power_mutex);
}

void set_usb_bt_power(int is_power)
{
	wifi_power_control(is_power, BT_BIT);
}
EXPORT_SYMBOL(set_usb_bt_power);

void set_usb_wifi_power(int is_power)
{
	wifi_power_control(is_power, WIFI_BIT);
}
EXPORT_SYMBOL(set_usb_wifi_power);

void extern_wifi_set_enable(int is_on)
{
	if (is_on) {
		set_wifi_power(1);
		WIFI_INFO("WiFi Enable! pin=%d\n", wifi_info.power_on_pin);
	} else {
		set_wifi_power(0);
		WIFI_INFO("WiFi Disable! pin=%d\n", wifi_info.power_on_pin);
	}
}
EXPORT_SYMBOL(extern_wifi_set_enable);

void extern_bt_set_enable(int is_on)
{
	WIFI_DEBUG("BT %s (stub)\n", is_on ? "enable" : "disable");
}
EXPORT_SYMBOL(extern_bt_set_enable);

int wifi_irq_num(void)
{
	return wifi_info.irq_num;
}
EXPORT_SYMBOL(wifi_irq_num);

int wifi_irq_trigger_level(void)
{
	return wifi_info.irq_trigger_type;
}
EXPORT_SYMBOL(wifi_irq_trigger_level);

static int wifi_dt_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const char *trigger_type_str;
	int ret;

	WIFI_INFO("probe\n");

	memset(&wifi_info, 0, sizeof(wifi_info));
	wifi_info.dev = &pdev->dev;
	wifi_info.power_on_pin = -1;
	wifi_info.power_on_pin2 = -1;
	wifi_info.interrupt_pin = -1;

	/* Get power GPIO */
	wifi_info.power_on_pin = of_get_named_gpio(np, "power_on-gpios", 0);
	if (gpio_is_valid(wifi_info.power_on_pin)) {
		ret = devm_gpio_request_one(&pdev->dev, wifi_info.power_on_pin,
					    GPIOF_OUT_INIT_LOW, "wifi_power");
		if (ret) {
			WIFI_INFO("failed to request power gpio: %d\n", ret);
			wifi_info.power_on_pin = -1;
		} else {
			WIFI_INFO("power_on_pin: %d\n", wifi_info.power_on_pin);
		}
	}

	/* Get power level (active high or low) */
	of_property_read_u32(np, "power_on_pin_level", &wifi_info.power_on_pin_level);

	/* Get secondary power GPIO if exists */
	wifi_info.power_on_pin2 = of_get_named_gpio(np, "power_on_2-gpios", 0);
	if (gpio_is_valid(wifi_info.power_on_pin2)) {
		ret = devm_gpio_request_one(&pdev->dev, wifi_info.power_on_pin2,
					    GPIOF_OUT_INIT_LOW, "wifi_power2");
		if (ret) {
			wifi_info.power_on_pin2 = -1;
		} else {
			WIFI_INFO("power_on_pin2: %d\n", wifi_info.power_on_pin2);
		}
	}

	/* Get interrupt GPIO */
	wifi_info.interrupt_pin = of_get_named_gpio(np, "interrupt-gpios", 0);
	if (gpio_is_valid(wifi_info.interrupt_pin)) {
		ret = devm_gpio_request_one(&pdev->dev, wifi_info.interrupt_pin,
					    GPIOF_IN, "wifi_irq");
		if (ret) {
			wifi_info.interrupt_pin = -1;
		} else {
			wifi_info.irq_num = gpio_to_irq(wifi_info.interrupt_pin);
			WIFI_INFO("interrupt_pin: %d, irq_num: %d\n",
				  wifi_info.interrupt_pin, wifi_info.irq_num);
		}
	}

	/* Get IRQ trigger type */
	ret = of_property_read_string(np, "irq_trigger_type", &trigger_type_str);
	if (!ret) {
		if (strcmp(trigger_type_str, "IRQF_TRIGGER_HIGH") == 0)
			wifi_info.irq_trigger_type = IRQF_TRIGGER_HIGH;
		else if (strcmp(trigger_type_str, "IRQF_TRIGGER_LOW") == 0)
			wifi_info.irq_trigger_type = IRQF_TRIGGER_LOW;
		else if (strcmp(trigger_type_str, "IRQF_TRIGGER_RISING") == 0)
			wifi_info.irq_trigger_type = IRQF_TRIGGER_RISING;
		else if (strcmp(trigger_type_str, "IRQF_TRIGGER_FALLING") == 0)
			wifi_info.irq_trigger_type = IRQF_TRIGGER_FALLING;
		else
			wifi_info.irq_trigger_type = IRQF_TRIGGER_HIGH;
	} else {
		wifi_info.irq_trigger_type = IRQF_TRIGGER_HIGH;
	}

	wifi_info.plat_info_valid = 1;

	/* Power on WiFi by default */
	if (gpio_is_valid(wifi_info.power_on_pin))
		set_wifi_power(1);

	WIFI_INFO("probe done\n");
	return 0;
}

static void wifi_dt_remove(struct platform_device *pdev)
{
	WIFI_INFO("remove\n");
	set_wifi_power(0);
}

static const struct of_device_id wifi_dt_match[] = {
	{ .compatible = "amlogic,aml-wifi" },
	{ .compatible = "amlogic, aml-wifi" },
	{},
};
MODULE_DEVICE_TABLE(of, wifi_dt_match);

static struct platform_driver wifi_dt_driver = {
	.probe = wifi_dt_probe,
	.remove = wifi_dt_remove,
	.driver = {
		.name = "aml_wifi",
		.of_match_table = wifi_dt_match,
	},
};

int wifi_dt_init(void)
{
	WIFI_INFO("init\n");
	return platform_driver_register(&wifi_dt_driver);
}
EXPORT_SYMBOL(wifi_dt_init);

void wifi_dt_exit(void)
{
	WIFI_INFO("exit\n");
	platform_driver_unregister(&wifi_dt_driver);
}
EXPORT_SYMBOL(wifi_dt_exit);
