/*
 * Copyright (c) 2025 Tobias Stoeckmann <tobias@stoeckmann.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <limits.h>
#include <pixman.h>
#include <stdint.h>
#include <stdlib.h>

#include <avif/avif.h>

#include "functions.h"

static pixman_image_t *
do_load_avif(FILE *fp, uint32_t **pixels)
{
	avifRGBImage rgb;
	avifDecoder *decoder;
	avifResult result;
	pixman_image_t *img;
	uint32_t width, height;
	uint8_t *fb;
	size_t bytesRead, len;
	long flen;

	decoder = avifDecoderCreate();
	if (decoder == NULL) {
		debug("failed to allocate AVIF decoder\n");
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	flen = ftell(fp);
	if (flen < 0) {
		debug("truncated AVIF file");
		avifDecoderDestroy(decoder);
		return NULL;
	}
	fseek(fp, 0, SEEK_SET);

	fb = malloc(flen);
	if (fb == NULL) {
		debug("failed to allocate AVIF file buffer\n");
		avifDecoderDestroy(decoder);
		return NULL;
	}
	bytesRead = fread(fb, sizeof(*fb), flen, fp);
	if (bytesRead != flen) {
		debug("failed to read entire AVIF file\n");
		avifDecoderDestroy(decoder);
		return NULL;
	}

	result = avifDecoderSetIOMemory(decoder, fb, flen);
	if (result != AVIF_RESULT_OK) {
		debug("failed to set IO on AVIF decoder\n");
		avifDecoderDestroy(decoder);
		free(fb);
		return NULL;
	}
	result = avifDecoderParse(decoder);
	if (result != AVIF_RESULT_OK) {
		debug("failed to decode AVIF image\n");
		avifDecoderDestroy(decoder);
		free(fb);
		return NULL;
	}

	width = decoder->image->width;
	height = decoder->image->height;
	if (decoder->image->depth != 8) {
		debug("AVIF bit depth must be 8 to be supported");
		avifDecoderDestroy(decoder);
		free(fb);
		return NULL;
	}

	avifDecoderNextImage(decoder);
	avifRGBImageSetDefaults(&rgb, decoder->image);

	SAFE_MUL3(len, width, height, sizeof(**pixels));
	*pixels = xmalloc(len);

	rgb.pixels = (unsigned char *)(*pixels);
	rgb.rowBytes = width * 4;
	rgb.format = AVIF_RGB_FORMAT_BGRA;

	result = avifImageYUVToRGB(decoder->image, &rgb);
	avifDecoderDestroy(decoder);
	free(fb);
	if (result != AVIF_RESULT_OK) {
		debug("conversion from YUV failed on AVIF image: %s\n",
		    avifResultToString(result));
		return NULL;
	}

	img = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, *pixels,
	    width * sizeof(uint32_t));
	if (img == NULL)
		errx(1, "failed to create pixman image");

	return img;
}

pixman_image_t *
load_avif(FILE *fp)
{
	pixman_image_t *img;
	uint32_t *pixels;

	pixels = NULL;
	img = do_load_avif(fp, &pixels);
	if (img == NULL)
		free(pixels);
	return img;
}
