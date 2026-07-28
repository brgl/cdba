/*
 * Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "device.h"
#include "tty.h"

#define PIC32CX_PIN_BATTERY	4
#define PIC32CX_PIN_FASTBOOT	321
#define PIC32CX_PIN_EDL		216

#define PIC32CX_POWER_DWELL_US	1000000

struct pic32cx {
	int fd;
	struct termios tios;
};

static void pic32cx_device_write(struct pic32cx *pic32cx, const char *fmt, ...)
{
	char buf[48];
	va_list va;
	int count;

	va_start(va, fmt);
	count = vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	if (count < 0)
		return;
	if (count >= (int)sizeof(buf))
		count = sizeof(buf) - 1;

	write(pic32cx->fd, buf, count);

	/* Discard whatever response the firmware sends back; nothing in
	 * cdba reads this fd, so drop it before the tty's input buffer
	 * has a chance to fill up.
	 */
	tcflush(pic32cx->fd, TCIFLUSH);
}

static void pic32cx_set_pin(struct pic32cx *pic32cx, unsigned int pin, bool on)
{
	char pinstr[8];

	snprintf(pinstr, sizeof(pinstr), "%u", pin);
	if (strlen(pinstr) < 3)
		pic32cx_device_write(pic32cx, "CONF:DIG:ON %d (@0%s)\n", on, pinstr);
	else
		pic32cx_device_write(pic32cx, "CONF:DIG:ON %d (@%s)\n", on, pinstr);
}

static void pic32cx_device_power(struct pic32cx *pic32cx, int on)
{
	if (!on) {
		pic32cx_set_pin(pic32cx, PIC32CX_PIN_BATTERY, true);
		return;
	}

	/* Force a full VBAT drop-and-restore rather than a bare write, so
	 * a power-on request always yields a real cold boot regardless of
	 * whatever state the rail was already in.
	 */
	pic32cx_set_pin(pic32cx, PIC32CX_PIN_BATTERY, true);
	usleep(PIC32CX_POWER_DWELL_US);
	pic32cx_set_pin(pic32cx, PIC32CX_PIN_BATTERY, false);
}

static void *pic32cx_open(struct device *dev)
{
	struct pic32cx *pic32cx;

	dev->has_power_key = false;

	pic32cx = calloc(1, sizeof(*pic32cx));
	if (!pic32cx)
		err(1, "failed to allocate pic32cx");

	pic32cx->fd = tty_open(dev->control_dev, &pic32cx->tios);
	if (pic32cx->fd < 0)
		err(1, "failed to open %s", dev->control_dev);

	/* Clear the firmware's command buffer, matching what both
	 * reference tools do on connect. No pin state is touched here.
	 */
	pic32cx_device_write(pic32cx, "echo 1\n");

	return pic32cx;
}

static int pic32cx_power(struct device *dev, bool on)
{
	pic32cx_device_power(dev->cdb, on);

	return 0;
}

static void pic32cx_key(struct device *dev, int key, bool asserted)
{
	struct pic32cx *pic32cx = dev->cdb;

	switch (key) {
	case DEVICE_KEY_FASTBOOT:
		pic32cx_set_pin(pic32cx, PIC32CX_PIN_FASTBOOT, asserted);
		break;
	case DEVICE_KEY_EDL:
		pic32cx_set_pin(pic32cx, PIC32CX_PIN_EDL, asserted);
		break;
	}
}

const struct control_ops pic32cx_ops = {
	.open = pic32cx_open,
	.power = pic32cx_power,
	.key = pic32cx_key,
};
