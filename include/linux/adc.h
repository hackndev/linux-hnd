/* 
 * Linux ADC class device
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

#ifndef __ADC_H__
#define __ADC_H__

struct adc_classdev;

struct adc_sense {
	char *name;
	int value;

	/* private to ADC Class and driver */
	int pin_id;
};

struct adc_request {
	struct adc_sense *senses;
	size_t num_senses;

	/* private to ADC Class */
	int (*sense)(struct device *dev, struct adc_request *req);
	struct device *dev;
	struct list_head node;
};

struct adc_classdev {
	char *name;
	int pin_id;
	char *units;
	int min_sense;
	int max_sense;

	int (*sense)(struct device *dev, struct adc_request *req);

	struct class_device *cdev;
};

extern int adc_request_sample(struct adc_request *req);
extern void adc_request_register(struct adc_request *req);
extern void adc_request_unregister(struct adc_request *req);

extern int adc_classdev_register(struct device *parent,
                                 struct adc_classdev *acdev);
extern void adc_classdev_unregister(struct adc_classdev *acdev);

#endif /* __ADC_H__ */
