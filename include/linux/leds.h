/*
 * Driver model for leds and led triggers
 *
 * Copyright (C) 2005 John Lenz <lenz@cs.wisc.edu>
 * Copyright (C) 2005 Richard Purdie <rpurdie@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LINUX_LEDS_H_INCLUDED
#define __LINUX_LEDS_H_INCLUDED

#include <linux/list.h>
#include <linux/spinlock.h>

struct device;
struct class_device;
/*
 * LED Core
 */

enum led_brightness {
	LED_OFF		= 0,
	LED_HALF	= 127,
	LED_FULL	= 255,
};

struct led_classdev {
	const char		*name;
	int			 brightness;
	int			 flags;

#define LED_SUSPENDED		(1 << 0)
#define LED_SUPPORTS_HWTIMER	(1 << 1)

	/* Set LED brightness level */
	void		(*brightness_set)(struct led_classdev *led_cdev,
					  enum led_brightness brightness);

	struct class_device	*class_dev;
	struct list_head	 node;			/* LED Device list */
	char			*default_trigger;	/* Trigger to use */

#ifdef CONFIG_LEDS_TRIGGERS
	/* Protects the trigger data below */
	rwlock_t		 trigger_lock;

	struct led_trigger	*trigger;
	struct list_head	 trig_list;
	void			*trigger_data;
#endif
};

extern int led_classdev_register(struct device *parent,
				 struct led_classdev *led_cdev);
extern void led_classdev_unregister(struct led_classdev *led_cdev);
extern void led_classdev_suspend(struct led_classdev *led_cdev);
extern void led_classdev_resume(struct led_classdev *led_cdev);

/*
 * LED Triggers
 */
#ifdef CONFIG_LEDS_TRIGGERS

#define TRIG_NAME_MAX 50

struct led_trigger {
	/* Trigger Properties */
	const char	 *name;
	void		(*activate)(struct led_classdev *led_cdev);
	void		(*deactivate)(struct led_classdev *led_cdev);
	int		(*is_led_supported)(struct led_classdev *led_cdev);
	/* Custom event handling function */
	void		(*send_event)(struct led_trigger *trigger, 
					enum led_brightness brightness);

	/* LEDs under control by this trigger (for simple triggers) */
	rwlock_t	  leddev_list_lock;
	struct list_head  led_cdevs;

	/* Link to next registered trigger */
	struct list_head  next_trig;
};

/* Registration functions for complex triggers */
extern int led_trigger_register(struct led_trigger *trigger);
extern void led_trigger_unregister(struct led_trigger *trigger);

/* Registration functions for simple triggers */
#define DEFINE_LED_TRIGGER(x)		static struct led_trigger *x;
#define DEFINE_LED_TRIGGER_GLOBAL(x)	struct led_trigger *x;
extern void led_trigger_register_simple(const char *name,
				struct led_trigger **trigger);
extern void led_trigger_unregister_simple(struct led_trigger *trigger);
extern void led_trigger_event(struct led_trigger *trigger,
				enum led_brightness event);

#else

/* Triggers aren't active - null macros */
#define DEFINE_LED_TRIGGER(x)
#define DEFINE_LED_TRIGGER_GLOBAL(x)
#define led_trigger_register_simple(x, y) do {} while(0)
#define led_trigger_unregister_simple(x) do {} while(0)
#define led_trigger_event(x, y) do {} while(0)

#endif

/* Registration functions for shared triggers */
#ifdef CONFIG_LEDS_TRIGGER_SHARED

struct led_trigger_shared {
	struct led_trigger trigger;
	int cnt;
};
#define DEFINE_LED_TRIGGER_SHARED(x) static struct led_trigger_shared *x;
#define DEFINE_LED_TRIGGER_SHARED_GLOBAL(x) struct led_trigger_shared *x;
#define EXPORT_LED_TRIGGER_SHARED(x) EXPORT_SYMBOL(x);
#define EXTERN_LED_TRIGGER_SHARED(x) extern struct led_trigger_shared *x;
extern void led_trigger_register_shared(const char *name,
                                        struct led_trigger_shared **trig);
extern void led_trigger_unregister_shared(struct led_trigger_shared *trig);
extern void led_trigger_event_shared(struct led_trigger_shared *trig,
                                       enum led_brightness b);

#else

/* Shared triggers aren't active - null macros */
#define DEFINE_LED_TRIGGER_SHARED(x)
#define DEFINE_LED_TRIGGER_SHARED_GLOBAL(x)
#define EXPORT_LED_TRIGGER_SHARED(x)
#define EXTERN_LED_TRIGGER_SHARED(x)
#define led_trigger_register_shared(x, y)  do {} while(0)
#define led_trigger_unregister_shared(x)  do {} while(0)
#define led_trigger_event_shared(x, y)  do {} while(0)

#endif

#if defined(CONFIG_LEDS_TRIGGER_HWTIMER) || defined(CONFIG_LEDS_TRIGGER_HWTIMER_MODULE)

struct hwtimer_data {
	unsigned long delay_on;  /* milliseconds on */
	unsigned long delay_off; /* milliseconds off */
};

extern void led_trigger_register_hwtimer(const char *name,
                                         struct led_trigger **trig);
extern void led_trigger_unregister_hwtimer(struct led_trigger *trig);

#else

#define led_trigger_register_hwtimer(x, y)  do {} while(0)
#define led_trigger_unregister_hwtimer(x)  do {} while(0)

#endif
#if defined(CONFIG_LEDS_TRIGGER_TIMER) || defined(CONFIG_LEDS_TRIGGER_TIMER_MODULE)

extern void led_trigger_register_timer(const char *name,
                                         struct led_trigger **trig);
extern void led_trigger_unregister_timer(struct led_trigger *trig);

#else

#define led_trigger_register_timer(x, y)  do {} while(0)
#define led_trigger_unregister_timer(x)  do {} while(0)

#endif

/* Trigger specific functions */
#ifdef CONFIG_LEDS_TRIGGER_IDE_DISK
extern void ledtrig_ide_activity(void);
#else
#define ledtrig_ide_activity() do {} while(0)
#endif
#if defined(CONFIG_LEDS_TRIGGER_MMC_CARD) || defined(CONFIG_LEDS_TRIGGER_MMC_CARD_MODULE)
extern void ledtrig_mmc_activity(void);
#else
#define ledtrig_mmc_activity() do {} while(0)
#endif

#endif		/* __LINUX_LEDS_H_INCLUDED */
