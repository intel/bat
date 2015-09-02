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

#include "gettext.h"
#include "common.h"

void sinf_sign(struct bat *bat,
		float *out, int length, int max, int *phase, float *val)
{
	int i = *phase, k, c, idx;
	float factor = max * RANGE_FACTOR;

	for (k = 0; k < length; k++) {
		for (c = 0; c < bat->channels; c++) {
			idx = k * bat->channels + c;
			out[idx] = sinf(val[c] * i) * factor;
		}
		i++;
		if (i == bat->rate)
			i = 0; /* Restart from 0 after one sine wave period */
	}
	*phase = i;
}

void sinf_unsign(struct bat *bat,
		float *out, int length, int max, int *phase, float *val)
{
	int i = *phase, k, c, idx;
	float factor = max * RANGE_FACTOR / 2.0;
	float offset = max * (1 - RANGE_FACTOR) / 2.0;

	for (k = 0; k < length; k++) {
		for (c = 0; c < bat->channels; c++) {
			idx = k * bat->channels + c;
			out[idx] = (sinf(val[c] * i) + 1.0) * factor
				+ offset;
		}
		i++;
		if (i == bat->rate)
			i = 0; /* Restart from 0 after one sine wave period */
	}
	*phase = i;
}

int generate_sine_wave(struct bat *bat, int length, void *buf, int max)
{
	static int i;
	int c, buf_bytes;
	float sin_val[MAX_CHANNELS];
	float *sinus_f = NULL;

	buf_bytes = bat->channels * length;
	sinus_f = (float *) malloc(buf_bytes * sizeof(float));
	if (sinus_f == NULL) {
		fprintf(bat->err, _("Not enough memory.\n"));
		return -ENOMEM;
	}

	for (c = 0; c < bat->channels; c++)
		sin_val[c] = 2.0 * M_PI * bat->target_freq[c]
			/ (float) bat->rate;

	bat->sinf_func(bat, sinus_f, length, max, &i, sin_val);

	bat->convert_float_to_sample(sinus_f, buf, length, bat->channels);

	free(sinus_f);

	return 0;
}
