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
#include <stdbool.h>
#include <errno.h>

#include "aconfig.h"
#include "gettext.h"

#include "common.h"
#include "alsa.h"

int retval_play;
int retval_record;

void close_file(FILE *file)
{
	if (file != NULL)
		fclose(file);
}

void destroy_mem(void *block)
{
	free(block);
}

int read_wav_header(struct bat *bat, char *file, FILE *fp, bool skip)
{
	struct wav_header riff_wave_header;
	struct wav_chunk_header chunk_header;
	struct chunk_fmt chunk_fmt;
	int more_chunks = 1;
	size_t ret;

	ret = fread(&riff_wave_header, sizeof(riff_wave_header), 1, fp);
	if (ret != 1) {
		fprintf(bat->err, _(E_MSG_READFILE "header of %s:%zd\n"),
				file, ret);
		return -EIO;
	}
	if ((riff_wave_header.magic != WAV_RIFF)
			|| (riff_wave_header.type != WAV_WAVE)) {
		fprintf(bat->err, _(E_MSG_FILECONTENT
					"%s is not a riff/wave file\n"),
				file);
		return -EINVAL;
	}

	do {
		ret = fread(&chunk_header, sizeof(chunk_header), 1, fp);
		if (ret != 1) {
			fprintf(bat->err, _(E_MSG_READFILE "chunk of %s:%zd\n"),
					file, ret);
			return -EIO;
		}

		switch (chunk_header.type) {
		case WAV_FMT:
			ret = fread(&chunk_fmt, sizeof(chunk_fmt), 1, fp);
			if (ret != 1) {
				fprintf(bat->err, _(E_MSG_READFILE));
				fprintf(bat->err, _("chunk fmt of %s:%zd\n"),
						file, ret);
				return -EIO;
			}
			/* If the format header is larger, skip the rest */
			if (chunk_header.length > sizeof(chunk_fmt)) {
				ret = fseek(fp,
					chunk_header.length - sizeof(chunk_fmt),
					SEEK_CUR);
				if (ret == -1) {
					fprintf(bat->err, _(E_MSG_SEEKFILE));
					fprintf(bat->err, _("chunk fmt of"));
					fprintf(bat->err, _(" %s:%zd\n"),
							file, ret);
					return -EINVAL;
				}
			}
			if (skip == false) {
				bat->channels = chunk_fmt.channels;
				bat->rate = chunk_fmt.sample_rate;
				bat->sample_size = chunk_fmt.sample_length / 8;
				switch (bat->sample_size) {
				case 1:
				case 2:
				case 3:
				case 4:
					break;
				default:
					fprintf(bat->err, _(E_MSG_PARAMS
								MSG_PCMFORMAT
								"size=%d\n"),
							bat->sample_size);
					return -EINVAL;
				}
				bat->frame_size = chunk_fmt.blocks_align;
			}

			break;
		case WAV_DATA:
			if (skip == false) {
				/*  The number of analysed captured frames is
					arbitrarily set to half of the number of
					frames of the wav file or the number of
					frames of the wav file when doing direct
					analysis (-l) */
				bat->frames =
						chunk_header.length
						/ bat->frame_size;
				if (!bat->local)
					bat->frames /= 2;
			}
			/* Stop looking for chunks */
			more_chunks = 0;
			break;
		default:
			/* Unknown chunk, skip bytes */
			ret = fseek(fp, chunk_header.length, SEEK_CUR);
			if (ret == -1) {
				fprintf(bat->err, _(E_MSG_SEEKFILE));
				fprintf(bat->err, _("unknown chunk of"));
				fprintf(bat->err, _(" %s:%zd\n"), file, ret);
				return -EINVAL;
			}
		}
	} while (more_chunks);

	bat->wav_data_offset = ftell(fp);
	if (bat->wav_data_offset == -1) {
		fprintf(bat->err, _("Obtain wav data offset error: %d\n"),
				-errno);
		return -errno;
	} else if (bat->wav_data_offset == 0) {
		fprintf(bat->err, _("Invalid wav data offset: 0\n"));
		return -EINVAL;
	}

	return 0;
}

void prepare_wav_info(struct wav_container *wav, struct bat *bat)
{
	wav->header.magic = WAV_RIFF;
	wav->header.type = WAV_WAVE;
	wav->format.magic = WAV_FMT;
	wav->format.fmt_size = 16;
	wav->format.format = WAV_FORMAT_PCM;
	wav->format.channels = bat->channels;
	wav->format.sample_rate = bat->rate;
	wav->format.sample_length = bat->sample_size * 8;
	wav->format.blocks_align = bat->channels * bat->sample_size;
	wav->format.bytes_p_second = wav->format.blocks_align * bat->rate;
	/* Default set time length to 10 seconds */
	wav->chunk.length = bat->frames * bat->frame_size;
	wav->chunk.type = WAV_DATA;
	wav->header.length = (wav->chunk.length) + sizeof(wav->chunk)
			+ sizeof(wav->format) + sizeof(wav->header) - 8;
}

int write_wav_header(FILE *fp, struct wav_container *wav, struct bat *bat)
{
	int err = 0;

	err = fwrite(&wav->header, 1, sizeof(wav->header), fp);
	if (err != sizeof(wav->header)) {
		fprintf(bat->err, _(E_MSG_WRITEFILE "header %d\n"), err);
		return -EIO;
	}
	err = fwrite(&wav->format, 1, sizeof(wav->format), fp);
	if (err != sizeof(wav->format)) {
		fprintf(bat->err, _(E_MSG_WRITEFILE "format %d\n"), err);
		return -EIO;
	}
	err = fwrite(&wav->chunk, 1, sizeof(wav->chunk), fp);
	if (err != sizeof(wav->chunk)) {
		fprintf(bat->err, _(E_MSG_WRITEFILE "chunk %d\n"), err);
		return -EIO;
	}

	return 0;
}
