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
#include <stdint.h>

double convert_uint8_to_double(void *buf, int i)
{
	return ((uint8_t *) buf)[i];
}

double convert_int16_to_double(void *buf, int i)
{
	return ((int16_t *) buf)[i];
}

double convert_int24_to_double(void *buf, int i)
{
	int32_t tmp;

	tmp = ((uint8_t *) buf)[i * 3 + 2] << 24;
	tmp |= ((uint8_t *) buf)[i * 3 + 1] << 16;
	tmp |= ((uint8_t *) buf)[i * 3] << 8;
	tmp >>= 8;
	return tmp;
}

double convert_int32_to_double(void *buf, int i)
{
	return ((int32_t *) buf)[i];
}

void convert_float_to_uint8(float sinus, void *buf)
{
	*((uint8_t *) buf) = (uint8_t) (sinus);
}

void convert_float_to_int16(float sinus, void *buf)
{
	*((int16_t *) buf) = (int16_t) (sinus);
}

void convert_float_to_int24(float sinus, void *buf)
{
	int32_t sinus_f_i;

	sinus_f_i = (int32_t) sinus;
	*((int8_t *) (buf + 0)) = (int8_t) (sinus_f_i & 0xff);
	*((int8_t *) (buf + 1)) = (int8_t) ((sinus_f_i >> 8) & 0xff);
	*((int8_t *) (buf + 2)) = (int8_t) ((sinus_f_i >> 16) & 0xff);
}

void convert_float_to_int32(float sinus, void *buf)
{
	*((int32_t *) buf) = (int32_t) (sinus);
}
