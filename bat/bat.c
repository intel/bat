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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <getopt.h>

#include "aconfig.h"
#include "gettext.h"
#include "version.h"

#include "common.h"

#include "alsa.h"
#include "convert.h"
#include "analyze.h"

static int get_duration(struct bat *bat)
{
	float duration_f;
	int duration_i;
	char *ptrf, *ptri;

	errno = 0;

	duration_f = strtod(bat->narg, &ptrf);
	duration_i = strtol(bat->narg, &ptri, 10);

	if (errno != 0)
		return -errno;

	if (duration_i < 0 || duration_f < 0.0)
		return -EINVAL;

	if (*ptrf == 's')
		bat->frames = duration_f * bat->rate;
	else if (*ptri == 0)
		bat->frames = duration_i;
	else
		return -EINVAL;

	if (bat->frames <= 0 || bat->frames > MAX_FRAMES)
		return -EINVAL;

	return 0;
}

static void get_sine_frequencies(struct bat *bat, char *freq)
{
	char *tmp1;

	tmp1 = strchr(freq, ',');
	if (tmp1 == NULL) {
		bat->target_freq[1] = bat->target_freq[0] = atof(optarg);
	} else {
		*tmp1 = '\0';
		bat->target_freq[0] = atof(optarg);
		bat->target_freq[1] = atof(tmp1 + 1);
	}
}

static inline int thread_wait_completion(struct bat *bat,
		pthread_t id, int **val)
{
	int err;

	err = pthread_join(id, (void **) val);
	if (err)
		pthread_cancel(id);

	return err;
}

/* loopback test where we play sine wave and capture the same sine wave */
static void test_loopback(struct bat *bat)
{
	pthread_t capture_id, playback_id;
	int ret;
	int *thread_result_capture, *thread_result_playback;

	/* start playback */
	ret = pthread_create(&playback_id, NULL,
			(void *) bat->playback.fct, bat);
	if (ret != 0) {
		fprintf(bat->err, _(E_MSG_NEWTHREADP "%d\n"), ret);
		exit(EXIT_FAILURE);
	}

	/* TODO: use a pipe to signal stream start etc - i.e. to sync threads */
	/* Let some time for playing something before capturing */
	usleep(CAPTURE_DELAY * 1000);

	/* start capture */
	ret = pthread_create(&capture_id, NULL, (void *) bat->capture.fct, bat);
	if (ret != 0) {
		fprintf(bat->err, _(E_MSG_NEWTHREADC "%d\n"), ret);
		pthread_cancel(playback_id);
		exit(EXIT_FAILURE);
	}

	/* wait for playback to complete */
	ret = thread_wait_completion(bat, playback_id, &thread_result_playback);
	if (ret != 0) {
		fprintf(bat->err, _(E_MSG_JOINTHREADP "%d\n"), ret);
		free(thread_result_playback);
		pthread_cancel(capture_id);
		exit(EXIT_FAILURE);
	}

	/* check playback status */
	if (*thread_result_playback != 0) {
		fprintf(bat->err, _(E_MSG_EXITTHREADP "%d\n"),
				*thread_result_playback);
		pthread_cancel(capture_id);
		exit(EXIT_FAILURE);
	} else {
		fprintf(bat->log, _("Playback completed.\n"));
	}

	/* now stop and wait for capture to finish */
	pthread_cancel(capture_id);
	ret = thread_wait_completion(bat, capture_id, &thread_result_capture);
	if (ret != 0) {
		fprintf(bat->err, _(E_MSG_JOINTHREADC "%d\n"), ret);
		free(thread_result_capture);
		exit(EXIT_FAILURE);
	}

	/* check capture status */
	if (*thread_result_capture != 0) {
		fprintf(bat->err, _(E_MSG_EXITTHREADC "%d\n"),
				*thread_result_capture);
		exit(EXIT_FAILURE);
	} else {
		fprintf(bat->log, _("Capture completed.\n"));
	}
}

/* single ended playback only test */
static void test_playback(struct bat *bat)
{
	pthread_t playback_id;
	int ret;
	int *thread_result;

	/* start playback */
	ret = pthread_create(&playback_id, NULL,
			(void *) bat->playback.fct, bat);
	if (ret != 0) {
		fprintf(bat->err, _(E_MSG_NEWTHREADP "%d\n"), ret);
		exit(EXIT_FAILURE);
	}

	/* wait for playback to complete */
	ret = thread_wait_completion(bat, playback_id, &thread_result);
	if (ret != 0) {
		fprintf(bat->err, _(E_MSG_JOINTHREADP "%d\n"), ret);
		free(thread_result);
		exit(EXIT_FAILURE);
	}

	/* check playback status */
	if (*thread_result != 0) {
		fprintf(bat->err, _(E_MSG_EXITTHREADP "%d\n"), *thread_result);
		exit(EXIT_FAILURE);
	} else {
		fprintf(bat->log, _("Playback completed.\n"));
	}
}

/* single ended capture only test */
static void test_capture(struct bat *bat)
{
	pthread_t capture_id;
	int ret;
	int *thread_result;

	/* start capture */
	ret = pthread_create(&capture_id, NULL, (void *) bat->capture.fct, bat);
	if (ret != 0) {
		fprintf(bat->err, _(E_MSG_NEWTHREADC "%d\n"), ret);
		exit(EXIT_FAILURE);
	}

	/* TODO: stop capture */

	/* wait for capture to complete */
	ret = thread_wait_completion(bat, capture_id, &thread_result);
	if (ret != 0) {
		fprintf(bat->err, _(E_MSG_JOINTHREADC "%d\n"), ret);
		free(thread_result);
		exit(EXIT_FAILURE);
	}

	/* check playback status */
	if (*thread_result != 0) {
		fprintf(bat->err, _(E_MSG_EXITTHREADC "%d\n"), *thread_result);
		exit(EXIT_FAILURE);
	} else {
		fprintf(bat->log, _("Capture completed.\n"));
	}
}

static void usage(struct bat *bat, char *argv[])
{
	fprintf(bat->log,
_("Usage:%s [Option]...\n"
"\n"
"-h, --help             help\n"
"-D                     sound card\n"
"-P                     playback pcm\n"
"-C                     capture pcm\n"
"-f                     input file\n"
"-s                     sample size\n"
"-c                     number of channels\n"
"-r                     sampling rate\n"
"-n                     frames to capture\n"
"-k                     sigma k\n"
"-F                     target frequency\n"
"-l                     internal loop, bypass hardware\n"
"-p                     total number of periods to play/capture\n"
"    --log              path of log file. if not set, logs be put to stdout,\n"
"			and errors be put to stderr.\n"
"    --saveplay         save playback content to target file, for debug\n"
), argv[0]);
}

static void set_defaults(struct bat *bat)
{
	memset(bat, 0, sizeof(struct bat));

	/* Set default values */
	bat->rate = 44100;
	bat->channels = 1;
	bat->frame_size = 2;
	bat->sample_size = 2;
	bat->convert_float_to_sample = convert_float_to_int16;
	bat->convert_sample_to_double = convert_int16_to_double;
	bat->sinf_func = sinf_sign;
	bat->frames = bat->rate * 2;
	bat->target_freq[0] = 997.0;
	bat->target_freq[1] = 997.0;
	bat->sigma_k = 3.0;
	bat->playback.device = NULL;
	bat->capture.device = NULL;
	bat->buf = NULL;
	bat->local = false;
	bat->playback.fct = &playback_alsa;
	bat->capture.fct = &record_alsa;
	bat->playback.single = false;
	bat->capture.single = false;
	bat->period_limit = false;
	bat->log = stdout;
	bat->err = stderr;
}

static void parse_arguments(struct bat *bat, int argc, char *argv[])
{
	int c, option_index;
	static const char short_options[] = "D:P:C:f:n:F:c:r:s:k:p:lth";
	static const struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"log", 1, 0, OPT_LOG},
		{"saveplay", 1, 0, OPT_SAVEPLAY},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, short_options, long_options,
					&option_index)) != -1) {
		switch (c) {
		case OPT_LOG:
			bat->logarg = optarg;
			break;
		case OPT_SAVEPLAY:
			bat->debugplay = optarg;
			break;
		case 'D':
			if (bat->playback.device == NULL)
				bat->playback.device = optarg;
			if (bat->capture.device == NULL)
				bat->capture.device = optarg;
			break;
		case 'P':
			if (bat->capture.single == true)
				bat->capture.single = false;
			else
				bat->playback.single = true;
			bat->playback.device = optarg;
			break;
		case 'C':
			if (bat->playback.single == true)
				bat->playback.single = false;
			else
				bat->capture.single = true;
			bat->capture.device = optarg;
			break;
		case 'f':
			bat->playback.file = optarg;
			break;
		case 'n':
			bat->narg = optarg;
			break;
		case 'F':
			get_sine_frequencies(bat, optarg);
			break;
		case 'c':
			bat->channels = atoi(optarg);
			break;
		case 'r':
			bat->rate = atoi(optarg);
			break;
		case 's':
			bat->sample_size = atoi(optarg);
			break;
		case 'k':
			bat->sigma_k = atof(optarg);
			break;
		case 'l':
			bat->local = true;
			break;
		case 'p':
			bat->periods_total = atoi(optarg);
			bat->period_limit = true;
			break;
		case 'h':
		default:
			usage(bat, argv);
			exit(EXIT_SUCCESS);
		}
	}
}

static int validate_options(struct bat *bat)
{
	int c;
	float freq_low, freq_high;

	/* check we have an input file for local mode */
	if ((bat->local == true) && (bat->capture.file == NULL)) {
		fprintf(bat->err, _(E_MSG_PARAMS
					"no input file for local testing\n"));
		return -EINVAL;
	}

	/* check supported channels */
	if (bat->channels > MAX_CHANNELS || bat->channels < MIN_CHANNELS) {
		fprintf(bat->err, _(E_MSG_PARAMS "%d channels not supported\n"),
				bat->channels);
		return -EINVAL;
	}

	/* check single ended is in either playback or capture - not both */
	if (bat->playback.single && bat->capture.single) {
		fprintf(bat->err, _(E_MSG_PARAMS
					"single ended mode is simplex\n"));
		return -EINVAL;
	}

	/* check sine wave frequency range */
	freq_low = DC_THRESHOLD;
	freq_high = bat->rate * RATE_FACTOR;
	for (c = 0; c < bat->channels; c++) {
		if (bat->target_freq[c] < freq_low
				|| bat->target_freq[c] > freq_high) {
			fprintf(bat->err, _(E_MSG_PARAMS
				"sine wave frequency out of range"));
			fprintf(bat->err, _("(%.1f, %.1f)\n"),
				freq_low, freq_high);
			return -EINVAL;
		}
	}

	return 0;
}

static int bat_init(struct bat *bat)
{
	int ret = 0;

	/* Determine log type */
	if (bat->logarg) {
		bat->log = NULL;
		bat->log = fopen(bat->logarg, "wb");
		if (bat->log == NULL) {
			fprintf(bat->err, _(E_MSG_OPENFILEC "%s %d\n"),
					bat->logarg, -errno);
			return -errno;
		}
		bat->err = bat->log;
	}

	/* Determine n */
	if (bat->narg) {
		ret = get_duration(bat);
		if (ret < 0) {
			fprintf(bat->err, _(E_MSG_PARAMS "duration: %s\n"),
					bat->narg);
			return ret;
		}
	}

	/* Determine capture file */
	if (bat->local)
		bat->capture.file = bat->playback.file;
	else
		bat->capture.file = TEMP_RECORD_FILE_NAME;

	if (bat->playback.file == NULL) {
		/* No input file so we will generate our own sine wave */
		if (bat->frames) {
			if (bat->playback.single) {
				/* Play nb of frames given by -n argument */
				bat->sinus_duration = bat->frames;
			} else {
				/* Play CAPTURE_DELAY msec +
				 * 150% of the nb of frames to be analysed */
				bat->sinus_duration = bat->rate *
						CAPTURE_DELAY / 1000;
				bat->sinus_duration +=
						(bat->frames + bat->frames / 2);
			}
		} else {
			/* Special case where we want to generate a sine wave
			 * endlessly without capturing */
			bat->sinus_duration = 0;
			bat->playback.single = true;
		}
	} else {
		bat->fp = fopen(bat->playback.file, "rb");
		if (bat->fp == NULL) {
			fprintf(bat->err, _(E_MSG_OPENFILEP "%s %d\n"),
					bat->playback.file, -errno);
			return -errno;
		}
		ret = read_wav_header(bat, bat->playback.file, bat->fp, false);
		fclose(bat->fp);
		if (ret != 0)
			return ret;
	}

	bat->frame_size = bat->sample_size * bat->channels;

	/* Set conversion functions */
	switch (bat->sample_size) {
	case 1:
		bat->convert_float_to_sample = convert_float_to_uint8;
		bat->convert_sample_to_double = convert_uint8_to_double;
		bat->sinf_func = sinf_unsign;
		break;
	case 2:
		bat->convert_float_to_sample = convert_float_to_int16;
		bat->convert_sample_to_double = convert_int16_to_double;
		bat->sinf_func = sinf_sign;
		break;
	case 3:
		bat->convert_float_to_sample = convert_float_to_int24;
		bat->convert_sample_to_double = convert_int24_to_double;
		bat->sinf_func = sinf_sign;
		break;
	case 4:
		bat->convert_float_to_sample = convert_float_to_int32;
		bat->convert_sample_to_double = convert_int32_to_double;
		bat->sinf_func = sinf_sign;
		break;
	default:
		fprintf(bat->err, _(E_MSG_PARAMS MSG_PCMFORMAT "size=%d\n"),
				bat->sample_size);
		return -EINVAL;
	}

	return ret;
}

int main(int argc, char *argv[])
{
	struct bat bat;
	int ret = 0;

	set_defaults(&bat);

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	textdomain(PACKAGE);
#endif

	fprintf(bat.log, _("%s version %s\n\n"), PACKAGE_NAME, PACKAGE_VERSION);

	parse_arguments(&bat, argc, argv);

	ret = bat_init(&bat);
	if (ret < 0)
		goto out;

	ret = validate_options(&bat);
	if (ret < 0)
		goto out;

	/* single line playback thread: playback only, no capture */
	if (bat.playback.single) {
		test_playback(&bat);
		goto out;
	}

	/* single line capture thread: capture only, no playback */
	if (bat.capture.single) {
		test_capture(&bat);
		goto analyze;
	}

	/* loopback thread: playback and capture in a loop */
	if (bat.local == false)
		test_loopback(&bat);

analyze:
	ret = analyze_capture(&bat);
out:
	fprintf(bat.log, _("\nReturn value is %d\n"), ret);
	if (bat.logarg)
		fclose(bat.log);

	return ret;
}
