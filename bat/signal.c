/*
 * Copyright (C) 2013-2015 Intel Corporation
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
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#include "common.h"

float sinf_sign(float x, int max)
{
	return sinf(x) * max * RANGE_FACTOR;
}

float sinf_unsign(float x, int max)
{
	/* remove DC introduced by RANGE_FACTOR */
	float offset = (1 - RANGE_FACTOR) / 2.0 * max;

	return (sinf(x) + 1.0) * max / 2.0 * RANGE_FACTOR + offset;
}

void generate_sine_wave(struct bat *bat, int length, void *buf, int max)
{
	static int i;
	int k, c;
	float sin_val[MAX_CHANNELS], sinus_f;

	for (c = 0; c < bat->channels; c++)
		sin_val[c] = (float) bat->target_freq[c] / (float) bat->rate;

	for (k = 0; k < length; k++) {
		for (c = 0; c < bat->channels; c++) {
			sinus_f = bat->sinf_func(i * 2.0 * M_PI * sin_val[c],
					max);
			bat->convert_float_to_sample(sinus_f, buf);
			buf += bat->sample_size;
		}
		i += 1;
		if (i == bat->rate)
			i = 0; /* Restart from 0 after one sine wave period */
	}
}
