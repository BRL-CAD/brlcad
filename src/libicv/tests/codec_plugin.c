/* C O D E C _ P L U G I N . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "icv/codec.h"

#ifdef BU_MIME_TYPES_H
#  error "Runtime image plugins must not depend on generated MIME enums"
#endif

static const unsigned char codec_signature[] = {'M', 'I', 'M', 'E', 'I', 'C', 'V', '\n'};
static const unsigned char alternate_signature[] = {'M', 'I', 'M', 'E', 'I', 'C', '2', '\n'};
static const unsigned char sequence_signature[] = {'M', 'I', 'M', 'E', 'S', 'E', 'Q', '\n'};

static const unsigned char *
codec_signature_for(const char *type)
{
    if (BU_STR_EQUAL(type, "image/vnd.example.codec"))
	return codec_signature;
    if (BU_STR_EQUAL(type, "image/vnd.example.codec2"))
	return alternate_signature;
    return NULL;
}

static icv_image_t *
codec_read_memory(const unsigned char *buffer, size_t size, size_t UNUSED(width), size_t UNUSED(height), const char *type, const struct bu_mime_traits *UNUSED(traits), const struct bu_mime_traits *UNUSED(observations))
{
    const unsigned char *signature = codec_signature_for(type);
    if (!signature || size < sizeof(codec_signature) || memcmp(buffer, signature, sizeof(codec_signature)))
	return NULL;
    return icv_read_mem_with_context(NULL, buffer + sizeof(codec_signature), size - sizeof(codec_signature), "image/vnd.brlcad.pix", NULL, 1, 1, NULL);
}

static icv_image_t *
codec_read_stream(FILE *fp, size_t width, size_t height, const char *type, const struct bu_mime_traits *UNUSED(traits), const struct bu_mime_traits *UNUSED(observations))
{
    unsigned char buffer[sizeof(codec_signature) + 3];
    if (fread(buffer, 1, sizeof(buffer), fp) != sizeof(buffer))
	return NULL;
    return codec_read_memory(buffer, sizeof(buffer), width, height, type, NULL, NULL);
}

static int
codec_write_memory(icv_image_t *image, unsigned char **buffer, size_t *size, const char *type, const struct bu_mime_traits *UNUSED(traits))
{
    const unsigned char *signature = codec_signature_for(type);
    if (!signature)
	return BRLCAD_ERROR;
    unsigned char *pixels = NULL;
    size_t pixel_size = 0;
    if (icv_write_mem_with_context(NULL, image, &pixels, &pixel_size, "image/vnd.brlcad.pix", NULL, NULL))
	return BRLCAD_ERROR;
    *size = sizeof(codec_signature) + pixel_size;
    *buffer = (unsigned char *)bu_malloc(*size, "fixture image");
    memcpy(*buffer, signature, sizeof(codec_signature));
    memcpy(*buffer + sizeof(codec_signature), pixels, pixel_size);
    bu_free(pixels, "fixture pixels");
    return 0;
}

static int
codec_write_stream(icv_image_t *image, FILE *fp, const char *type, const struct bu_mime_traits *UNUSED(traits))
{
    unsigned char *buffer = NULL;
    size_t size = 0;
    int status = codec_write_memory(image, &buffer, &size, type, NULL);
    if (!status && fwrite(buffer, 1, size, fp) != size)
	status = BRLCAD_ERROR;
    bu_free(buffer, "fixture image");
    return status;
}

static enum bu_mime_probe_status
codec_probe(const struct bu_mime_probe_input *input, const char *type)
{
    const unsigned char *signature = codec_signature_for(type);
    if (!signature)
	return BU_MIME_PROBE_ERROR;
    size_t available = input->size < sizeof(codec_signature) ? input->size : sizeof(codec_signature);
    if (available && memcmp(input->data, signature, available))
	return BU_MIME_PROBE_NO_MATCH;
    if (input->size < sizeof(codec_signature))
	return input->complete ? BU_MIME_PROBE_NO_MATCH : BU_MIME_PROBE_INCOMPLETE;
    return BU_MIME_PROBE_SIGNATURE;
}

static enum icv_metadata_status
codec_metadata(const struct bu_mime_probe_input *input, const char *type, struct icv_image_metadata *info, const struct bu_mime_traits *UNUSED(traits), const struct bu_mime_traits *UNUSED(observations))
{
    if (codec_probe(input, type) != BU_MIME_PROBE_SIGNATURE)
	return input->complete ? ICV_METADATA_ERROR : ICV_METADATA_INCOMPLETE;
    info->width = info->height = 1;
    info->channels = 3;
    info->color_space = ICV_METADATA_COLOR_RGB;
    return ICV_METADATA_MATCH;
}

static int
sequence_traits_match(const struct bu_mime_traits *requested, const struct bu_mime_traits *observations)
{
    const struct bu_mime_trait encoding = BU_MIME_TRAIT_STRING("encoding", "binary");
    const struct bu_mime_traits observed = {&encoding, 1};
    return bu_mime_traits_match(&observed, observations, requested);
}

static enum bu_mime_probe_status
sequence_probe(const struct bu_mime_probe_input *input, const char *UNUSED(type))
{
    const size_t size = input->size < sizeof(sequence_signature) ? input->size : sizeof(sequence_signature);
    if (size && memcmp(input->data, sequence_signature, size)) return BU_MIME_PROBE_NO_MATCH;
    if (size < sizeof(sequence_signature)) return input->complete ? BU_MIME_PROBE_NO_MATCH : BU_MIME_PROBE_INCOMPLETE;
    return BU_MIME_PROBE_SIGNATURE;
}

static icv_anim_t *
sequence_read(struct icv_context *context, const char *filename, const char *UNUSED(type), const struct bu_mime_traits *traits, const struct bu_mime_traits *observations)
{
    if (!sequence_traits_match(traits, observations)) return NULL;
    FILE *file = fopen(filename, "rb");
    if (!file) return NULL;
    unsigned char signature[sizeof(sequence_signature)];
    icv_anim_t *animation = NULL;
    if (fread(signature, 1, sizeof(signature), file) == sizeof(signature) && !memcmp(signature, sequence_signature, sizeof(signature))) {
	int count = fgetc(file);
	if (count > 0) animation = icv_anim_create(ICV_ANIM_UNKNOWN, 1, 1, 10);
	for (int i = 0; animation && i < count; ++i) {
	    unsigned char rgb[3];
	    icv_image_t *image = NULL;
	    if (fread(rgb, 1, sizeof(rgb), file) == sizeof(rgb))
		image = icv_read_mem_with_context(context, rgb, sizeof(rgb), "image/vnd.brlcad.pix", NULL, 1, 1, NULL);
	    int valid = image && !icv_anim_add_frame(animation, image);
	    icv_destroy(image);
	    if (!valid) {icv_anim_destroy(animation); animation = NULL;}
	}
    }
    int error = ferror(file);
    if (fclose(file) || error) {icv_anim_destroy(animation); animation = NULL;}
    return animation;
}

static int
sequence_write(struct icv_context *context, const icv_anim_t *animation, const char *filename,
    const char *UNUSED(type), const struct bu_mime_traits *traits)
{
    const size_t count = icv_anim_num_frames(animation);
    if (!count || count > UINT8_MAX || !sequence_traits_match(traits, NULL)) return BRLCAD_ERROR;
    FILE *file = fopen(filename, "wb");
    if (!file) return BRLCAD_ERROR;
    int valid = fwrite(sequence_signature, 1, sizeof(sequence_signature), file) == sizeof(sequence_signature) &&
	fputc((int)count, file) != EOF;
    for (size_t i = 0; valid && i < count; ++i) {
	icv_image_t *image = icv_anim_get_frame(animation, i);
	unsigned char *rgb = NULL;
	size_t size = 0;
	valid = image && image->width == 1 && image->height == 1 &&
	    !icv_write_mem_with_context(context, image, &rgb, &size, "image/vnd.brlcad.pix", NULL, NULL) &&
	    size == 3 && fwrite(rgb, 1, size, file) == size;
	bu_free(rgb, "fixture sequence frame");
	icv_destroy(image);
    }
    if (ferror(file)) valid = 0;
    if (fclose(file)) valid = 0;
    return valid ? 0 : BRLCAD_ERROR;
}

COMPILER_DLLEXPORT const struct icv_plugin *
icv_plugin_info(void)
{
    static const char *extensions[] = {"codecimg"};
    static const char *aliases[] = {"image/x-example-codec2"};
    static const char *sequence_extensions[] = {"codecseq"};
    static const char *sequence_aliases[] = {"video/x-example-sequence"};
    static const struct bu_mime_definition definitions[] = {{
	BU_MIME_ABI_VERSION, sizeof(struct bu_mime_definition),
	"image/vnd.example.codec", "Fixture image", NULL, 0, extensions, 1, NULL, 0
    }, {BU_MIME_ABI_VERSION, sizeof(struct bu_mime_definition),
	"image/vnd.example.codec2", "Second fixture image", aliases, 1, NULL, 0, NULL, 0},
	{BU_MIME_ABI_VERSION, sizeof(struct bu_mime_definition), "video/vnd.example.sequence", "Fixture sequence",
	    sequence_aliases, 1, sequence_extensions, 1, NULL, 0}};
    static const struct bu_mime_capability types[] = {
        BU_MIME_CAPABILITY("image/vnd.example.codec", NULL, 0, 0),
        BU_MIME_CAPABILITY("image/vnd.example.codec2", NULL, 0, 0),
    };
    static const struct bu_mime_trait binary = BU_MIME_TRAIT_STRING("encoding", "binary");
    static const struct bu_mime_capability sequence_format = {"video/vnd.example.sequence", &binary, 1,
	ICV_CODEC_READ_SEQUENCE | ICV_CODEC_WRITE_SEQUENCE, BU_MIME_DEFER_INPUT_VALIDATION};
    static const struct icv_codec codecs[] = {{
	"Fixture image codec", types, 2, BU_MIME_DEFAULT_PRIORITY,
	codec_read_stream, codec_read_memory, codec_write_stream, codec_write_memory, codec_probe, 0, codec_metadata, NULL, NULL, NULL
    }, {"Fixture sequence codec", &sequence_format, 1, BU_MIME_DEFAULT_PRIORITY,
	NULL, NULL, NULL, NULL, sequence_probe, 0, NULL, NULL, sequence_read, sequence_write}};
    static const struct icv_plugin plugin = {
	ICV_PLUGIN_ABI_VERSION, sizeof(struct icv_plugin), definitions, sizeof(definitions) / sizeof(definitions[0]),
	codecs, sizeof(codecs) / sizeof(codecs[0])
    };
    return &plugin;
}
