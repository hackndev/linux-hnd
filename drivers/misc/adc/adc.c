/* 
 * Linux ADC class
 *
 * Copyright (c) 2007  Anton Vorontsov <cbou@mail.ru>
 *
 * Based on ADC API:
 * Copyright (c) 2006-2007  Paul Sokolovsky <pmiscml@gmail.com>
 * Copyright (c) 2006  Kevin O'Connor <kevin@koconnor.net>
 *
 * ADC class device idea by Kevin O'Connor <kevin@koconnor.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/rwsem.h>
#include <linux/adc.h>

static struct class *adc_class;

static LIST_HEAD(requests);
static struct rw_semaphore requests_sem;

int adc_request_sample(struct adc_request *req)
{
	down_read(&requests_sem);
	if (req->sense) 
		req->sense(req->dev, req);
	up_read(&requests_sem);
	return 0;
}

void adc_request_register(struct adc_request *req)
{
	size_t i;
	struct class_device *cdev;
	struct adc_classdev *acdev;

	down_write(&requests_sem);
	list_add_tail(&req->node, &requests);

	down(&adc_class->sem);

	for (i = 0; i < req->num_senses; i++) {
		req->senses[i].pin_id = -1;

		if (!req->senses[i].name)
			continue;

		list_for_each_entry(cdev, &adc_class->children, node) {
			acdev = class_get_devdata(cdev);
			if (!strncmp(req->senses[i].name,
			            acdev->cdev->class_id, BUS_ID_SIZE)) {
				req->senses[i].pin_id = acdev->pin_id;
				req->sense = acdev->sense;
				req->dev = acdev->cdev->dev;
				break;
			}
		}
	}

	up(&adc_class->sem);
	up_write(&requests_sem);

	return;
}

void adc_request_unregister(struct adc_request *req)
{
	down_write(&requests_sem);
	list_del(&req->node);
	up_write(&requests_sem);
	return;
}

static ssize_t adc_classdev_show_max_sense(struct class_device *cdev,
                                           char *buf)
{
	struct adc_classdev *acdev = class_get_devdata(cdev);
	return sprintf(buf, "%d\n", acdev->max_sense);
}

static ssize_t adc_classdev_show_min_sense(struct class_device *cdev,
                                           char *buf)
{
	struct adc_classdev *acdev = class_get_devdata(cdev);
	return sprintf(buf, "%d\n", acdev->min_sense);
}

static ssize_t adc_classdev_show_units(struct class_device *cdev, char *buf)
{
	struct adc_classdev *acdev = class_get_devdata(cdev);
	return sprintf(buf, "%s\n", acdev->units ? acdev->units : "unknown");
}

static ssize_t adc_classdev_show_sense(struct class_device *cdev, char *buf)
{
	struct adc_classdev *acdev = class_get_devdata(cdev);
	struct adc_request req;
	struct adc_sense sen;

	sen.name = acdev->cdev->class_id;
	sen.pin_id = acdev->pin_id;
	req.dev = acdev->cdev->dev;
	req.sense = acdev->sense;
	req.num_senses = 1;
	req.senses = &sen;

	adc_request_sample(&req);

	return sprintf(buf, "%d\n", sen.value);
}

CLASS_DEVICE_ATTR(max_sense, 0400, adc_classdev_show_max_sense, NULL);
CLASS_DEVICE_ATTR(min_sense, 0400, adc_classdev_show_min_sense, NULL);
CLASS_DEVICE_ATTR(units, 0400, adc_classdev_show_units, NULL);
CLASS_DEVICE_ATTR(sense, 0400, adc_classdev_show_sense, NULL);

int adc_classdev_register(struct device *parent, struct adc_classdev *acdev)
{
	int ret = 0;
	struct adc_request *req;
	size_t i;

	acdev->cdev = class_device_create(adc_class, NULL, 0, parent,
	                                  "%s:%s", parent->bus_id,
	                                  acdev->name);
	if (IS_ERR(acdev->cdev)) {
		ret = PTR_ERR(acdev->cdev);
		goto cdev_create_failed;
	}

	#define adc_class_device_create_file(x)                         \
		ret = class_device_create_file(acdev->cdev,             \
		                               &class_device_attr_##x); \
		if (ret)                                                \
			goto cdev_create_##x##_failed;

	adc_class_device_create_file(sense);
	adc_class_device_create_file(units);
	adc_class_device_create_file(min_sense);
	adc_class_device_create_file(max_sense);

	#undef adc_class_device_create_file

	class_set_devdata(acdev->cdev, acdev);

	down_write(&requests_sem);
	list_for_each_entry(req, &requests, node) {
		for (i = 0; i < req->num_senses; i++) {
			if (!req->senses[i].name)
				continue;
			if (!strncmp(req->senses[i].name,
			             acdev->cdev->class_id, BUS_ID_SIZE)) {
				req->senses[i].pin_id = acdev->pin_id;
				req->sense = acdev->sense;
				req->dev = acdev->cdev->dev;
			}
		}
	}
	up_write(&requests_sem);

	goto success;

cdev_create_max_sense_failed:
	class_device_remove_file(acdev->cdev, &class_device_attr_min_sense);
cdev_create_min_sense_failed:
	class_device_remove_file(acdev->cdev, &class_device_attr_units);
cdev_create_units_failed:
	class_device_remove_file(acdev->cdev, &class_device_attr_sense);
cdev_create_sense_failed:
	class_device_unregister(acdev->cdev);
cdev_create_failed:
success:
	return ret;
}

void adc_classdev_unregister(struct adc_classdev *acdev)
{
	struct adc_request *req;

	down_write(&requests_sem);
	list_for_each_entry(req, &requests, node) {
		if (req->dev == acdev->cdev->dev) {
			req->sense = NULL;
			req->dev = NULL;
		}
	}
	up_write(&requests_sem);

	class_device_remove_file(acdev->cdev, &class_device_attr_max_sense);
	class_device_remove_file(acdev->cdev, &class_device_attr_min_sense);
	class_device_remove_file(acdev->cdev, &class_device_attr_units);
	class_device_remove_file(acdev->cdev, &class_device_attr_sense);

	class_device_unregister(acdev->cdev);

	return;
}

static int __init adc_init(void)
{
	int ret = 0;

	adc_class = class_create(THIS_MODULE, "adc");
	if (IS_ERR(adc_class)) {
		printk(KERN_ERR "adc: failed to create adc class\n");
		ret = PTR_ERR(adc_class);
		goto class_create_failed;
	}

	init_rwsem(&requests_sem);

	goto success;

class_create_failed:
success:
	return ret;
}

static void __exit adc_exit(void)
{
	class_destroy(adc_class);
	return;
}

subsys_initcall(adc_init);
module_exit(adc_exit);

EXPORT_SYMBOL_GPL(adc_request_sample);
EXPORT_SYMBOL_GPL(adc_request_register);
EXPORT_SYMBOL_GPL(adc_request_unregister);

EXPORT_SYMBOL_GPL(adc_classdev_register);
EXPORT_SYMBOL_GPL(adc_classdev_unregister);

MODULE_DESCRIPTION("Linux ADC class");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Vorontsov <cbou@mail.ru>");
