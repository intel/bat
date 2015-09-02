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

#define TEMP_RECORD_FILE_NAME		"/tmp/test.wav"

#define OPT_BASE			300
#define OPT_LOG				(OPT_BASE + 1)
#define OPT_SAVEPLAY			(OPT_BASE + 2)

#define COMPOSE(a, b, c, d)		((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define WAV_RIFF			COMPOSE('R', 'I', 'F', 'F')
#define WAV_WAVE			COMPOSE('W', 'A', 'V', 'E')
#define WAV_FMT				COMPOSE('f', 'm', 't', ' ')
#define WAV_DATA			COMPOSE('d', 'a', 't', 'a')
#define WAV_FORMAT_PCM			1	/* PCM WAVE file encoding */

#define MAX_CHANNELS			2
#define MIN_CHANNELS			1
#define MAX_PEAKS			10
#define MAX_FRAMES			(10 * 1024 * 1024)
/* Given in ms */
#define CAPTURE_DELAY			500
/* signal frequency should be less than samplerate * RATE_FACTOR */
#define RATE_FACTOR			0.4
/* valid range of samplerate: (1 - RATE_RANGE, 1 + RATE_RANGE) * samplerate */
#define RATE_RANGE			0.05
/* Given in us */
#define MAX_BUFFERTIME			500000
/* devide factor, was 4, changed to 8 to remove reduce capture overrun */
#define DIV_BUFFERTIME			8
/* margin to avoid sign inversion when generate sine wav */
#define RANGE_FACTOR			0.95

#define EBATBASE			1000
#define ENOPEAK				(EBATBASE + 1)
#define EONLYDC				(EBATBASE + 2)
#define EBADPEAK			(EBATBASE + 3)

#define DC_THRESHOLD			7.01

/* tolerance of detected peak = max (DELTA_HZ, DELTA_RATE * target_freq) */
#define DELTA_RATE			0.005
#define DELTA_HZ			1

#define FOUND_DC			(1<<1)
#define FOUND_WRONG_PEAK		(1<<0)

/* memory error message */
#define E_MSG_MALLOC			"Allocate memory buffer error: "
/* file error message */
#define E_MSG_OPENFILEP			"Can't open file for playback: "
#define E_MSG_OPENFILEC			"Can't open file for capture: "
#define E_MSG_READFILE			"Read file error: "
#define E_MSG_WRITEFILE			"Write file error: "
#define E_MSG_SEEKFILE			"Seek file error: "
#define E_MSG_FILECONTENT		"File content error: "
/* thread error message */
#define E_MSG_NEWTHREADP		"Can't create playback thread: "
#define E_MSG_NEWTHREADC		"Can't create capture thread: "
#define E_MSG_JOINTHREADP		"Can't join playback thread: "
#define E_MSG_JOINTHREADC		"Can't join capture thread: "
#define E_MSG_EXITTHREADP		"Exit playback thread fail: "
#define E_MSG_EXITTHREADC		"Exit capture thread fail: "
/* pcm device error message */
#define E_MSG_OPENPCMP			"Can't open PCM playback device: "
#define E_MSG_OPENPCMC			"Can't open PCM capture device: "
#define E_MSG_READPCM			"Read PCM device error: "
#define E_MSG_WRITEPCM			"Write PCM device error: "
#define E_MSG_NOPCMP			"No PCM device for playback: "
#define E_MSG_NOPCMC			"No PCM device for capture: "
#define E_MSG_SETDEV			"Set parameter to device error: "
#define E_MSG_GETDEV			"Get parameter from device error: "
/* parameter error message */
#define E_MSG_PARAMS			"Invalid parameter: "
/* error type string */
#define MSG_DEFAULT			"default params: "
#define MSG_HWPARAMS			"hw params: "
#define MSG_CHANNELS			"channel number: "
#define MSG_PCMFORMAT			"PCM format: "
#define MSG_ACCESS			"access type: "
#define MSG_SAMPLERATE			"sample rate: "
#define MSG_BUFFERTIME			"buffer time: "
#define MSG_BUFFERSIZE			"buffer size: "
#define MSG_PERIODTIME			"period time: "
#define MSG_PERIODSIZE			"period size: "
#define MSG_UNDERRUN			"underrun: "
#define MSG_OVERRUN			"overrun: "

struct wav_header {
	unsigned int magic; /* 'RIFF' */
	unsigned int length; /* file len */
	unsigned int type; /* 'WAVE' */
};

struct wav_chunk_header {
	unsigned int type; /* 'data' */
	unsigned int length; /* sample count */
};

struct wav_fmt {
	unsigned int magic; /* 'FMT '*/
	unsigned int fmt_size; /* 16 or 18 */
	unsigned short format; /* see WAV_FMT_* */
	unsigned short channels;
	unsigned int sample_rate; /* Frequency of sample */
	unsigned int bytes_p_second;
	unsigned short blocks_align; /* sample size; 1 or 2 bytes */
	unsigned short sample_length; /* 8, 12 or 16 bit */
};

struct chunk_fmt {
	unsigned short format; /* see WAV_FMT_* */
	unsigned short channels;
	unsigned int sample_rate; /* Frequency of sample */
	unsigned int bytes_p_second;
	unsigned short blocks_align; /* sample size; 1 or 2 bytes */
	unsigned short sample_length; /* 8, 12 or 16 bit */
};

struct wav_container {
	struct wav_header header;
	struct wav_fmt format;
	struct wav_chunk_header chunk;
};

struct bat;

struct pcm {
	char *device;
	char *file;
	bool single;
	void *(*fct)(struct bat *);
};

struct bat {
	unsigned int rate;	/* sampling rate */
	int channels;		/* nb of channels */
	int frames;		/* nb of frames */
	int frame_size;		/* size of frame */
	int sample_size;	/* size of sample */

	float sigma_k;		/* threshold for peak detection */
	float target_freq[MAX_CHANNELS];

	int sinus_duration;	/* number of frames for playback */
	char *narg;		/* argument string of duration */
	char *logarg;		/* path name of log file */
	char *debugplay;	/* path name to store playback signal */

	struct pcm playback;
	struct pcm capture;

	unsigned int periods_played;
	unsigned int periods_total;
	bool period_limit;

	FILE *fp;
	long wav_data_offset;

	FILE *log;
	FILE *err;

	double (*convert_sample_to_double)(void *, int);
	void (*convert_float_to_sample)(float, void *);
	float (*sinf_func)(float, int);

	void *buf;		/* PCM Buffer */

	bool local;		/* true for internal test */
};

struct analyze {
	void *buf;
	double *in;
	double *out;
	double *mag;
};

void close_file(FILE *);
void destroy_mem(void *);

void prepare_wav_info(struct wav_container *, struct bat *);
int read_wav_header(struct bat *, char *, FILE *, bool);
int write_wav_header(FILE *, struct wav_container *, struct bat *);

float sinf_sign(float, int);
float sinf_unsign(float, int);
void generate_sine_wave(struct bat *, int, void *, int);
