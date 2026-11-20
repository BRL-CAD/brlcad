/* S E Q U E N C E _ P R O F I L E S . C P P
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
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/str.h"
#include "icv/codec.h"
#include "icv/colorspace.h"
#include "vmath.h"

namespace {
using bytes = std::vector<unsigned char>;
using context_ptr = std::unique_ptr<icv_context, decltype(&icv_context_destroy)>;
using animation_ptr = std::unique_ptr<icv_anim_t, decltype(&icv_anim_destroy)>;
using image_ptr = std::unique_ptr<icv_image_t, decltype(&icv_destroy)>;
size_t jpeg_reads = 0, jpeg_writes = 0;

void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

struct temporary {
    char path[MAXPATHLEN];
    temporary() {FILE *file = bu_temp_file(path, sizeof(path)); require(file && !fclose(file), "cannot create sequence path");}
    ~temporary() {bu_file_delete(path);}
    void write(const bytes &data) const
    {
	FILE *file = fopen(path, "wb");
	require(file && fwrite(data.data(), 1, data.size(), file) == data.size() && !fclose(file), "cannot save sequence fixture");
    }
    bytes read() const
    {
	std::ifstream file(path, std::ios::binary);
	require(file.is_open(), "cannot read sequence fixture");
	bytes data{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	require(!file.bad(), "sequence fixture read failed");
	return data;
    }
};

icv_image_t *jpeg_read(const unsigned char *data, size_t size, size_t width, size_t height, const char *type, const bu_mime_traits *traits, const bu_mime_traits *)
{
    ++jpeg_reads;
    return icv_read_mem_variant(NULL, data, size, type, NULL, traits, width, height, NULL);
}

int jpeg_write(icv_image_t *image, unsigned char **data, size_t *size, const char *type, const bu_mime_traits *traits)
{
    ++jpeg_writes;
    return icv_write_mem_variant(NULL, image, data, size, type, NULL, traits, NULL);
}

icv_metadata_status avi_note_metadata(const bu_mime_probe_input *, const char *, icv_image_metadata *, const bu_mime_traits *, const bu_mime_traits *)
{
    return ICV_METADATA_UNKNOWN;
}

int avi_note(const bu_mime_probe_input *input, const char *, bu_mime_traits *traits)
{
    constexpr size_t riff_header_size = 12;
    if (input->size < riff_header_size || memcmp(input->data, "RIFF", sizeof(uint32_t)) ||
	memcmp(input->data + 2 * sizeof(uint32_t), "AVI ", sizeof(uint32_t))) return 1;
    const char *members[] = {"riff", "avi"};
    return bu_mime_traits_add(traits, "avi.header-present", "true") &&
	bu_mime_traits_add_set(traits, "fixture.avi-signatures", members, sizeof(members) / sizeof(members[0]));
}

animation_ptr make_animation()
{
    animation_ptr animation(icv_anim_create(ICV_ANIM_UNKNOWN, 1, 1, 12), icv_anim_destroy);
    require(animation != nullptr, "cannot create sequence");
    for (size_t i = 0; i < 2; ++i) {
	image_ptr image(icv_create(1, 1, ICV_COLOR_SPACE_RGB), icv_destroy);
	require(image != nullptr, "cannot create frame");
	image->data[0] = i ? 0 : 1;
	image->data[1] = i ? 1 : 0;
	image->data[2] = 0;
	require(!icv_anim_add_frame(animation.get(), image.get()), "cannot add frame");
    }
    return animation;
}

void check_frames(const icv_anim_t *animation)
{
    require(animation && icv_anim_num_frames(animation) == 2, "sequence changed its frame count");
    constexpr double jpeg_tolerance = 0.03;
    for (size_t i = 0; i < 2; ++i) {
	image_ptr image(icv_anim_get_frame(animation, i), icv_destroy);
	require(image && image->width == 1 && image->height == 1 && image->channels == 3 &&
	    NEAR_EQUAL(image->data[0], i ? 0 : 1, jpeg_tolerance) &&
	    NEAR_EQUAL(image->data[1], i ? 1 : 0, jpeg_tolerance) && NEAR_ZERO(image->data[2], jpeg_tolerance),
	    "sequence changed its frame pixels");
    }
}

void put_word(bytes &data, size_t at, uint32_t value)
{
    for (size_t i = 0; i < sizeof(value); ++i) data[at + i] = static_cast<unsigned char>(value >> (8 * i));
}

uint32_t get_word(const bytes &data, size_t at)
{
    require(at + sizeof(uint32_t) <= data.size(), "incomplete AVI fixture word");
    uint32_t value = 0;
    for (size_t i = 0; i < sizeof(value); ++i) value |= uint32_t(data[at + i]) << (8 * i);
    return value;
}

bytes pad_avi_header(const bytes &data)
{
    constexpr size_t riff_header_size = 12, chunk_header_size = 8;
    const size_t junk_size = BU_MIME_DEFAULT_SCAN_LIMIT + 1;
    bytes padded(data.begin(), data.begin() + riff_header_size);
    padded.insert(padded.end(), {'J','U','N','K',0,0,0,0});
    put_word(padded, riff_header_size + sizeof(uint32_t), static_cast<uint32_t>(junk_size));
    padded.resize(padded.size() + junk_size + (junk_size & 1), 0);
    padded.insert(padded.end(), data.begin() + riff_header_size, data.end());
    put_word(padded, sizeof(uint32_t), static_cast<uint32_t>(padded.size() - chunk_header_size));
    return padded;
}

bytes extra_avi_stream(const bytes &data, bool audio_first)
{
    constexpr size_t riff_header_size = 12, chunk_header_size = 8, list_type_size = 4;
    const size_t header_size_at = riff_header_size + sizeof(uint32_t);
    const size_t header_end = riff_header_size + chunk_header_size + get_word(data, header_size_at);
    const size_t children = riff_header_size + chunk_header_size + list_type_size;
    for (size_t at = children; at + chunk_header_size + list_type_size <= header_end;) {
	const size_t size = get_word(data, at + sizeof(uint32_t));
	const size_t end = at + chunk_header_size + size + (size & 1);
	require(end <= header_end, "incomplete AVI fixture list");
	if (!memcmp(data.data() + at, "LIST", list_type_size) && !memcmp(data.data() + at + chunk_header_size, "strl", list_type_size)) {
	    bytes stream(data.begin() + at, data.begin() + end);
	    if (audio_first) {
		const unsigned char video[] = {'v','i','d','s'};
		const auto tag = std::search(stream.begin(), stream.end(), std::begin(video), std::end(video));
		require(tag != stream.end(), "AVI fixture has no stream type");
		std::copy_n("auds", sizeof(video), tag);
	    }
	    // Header-only rejection fixtures do not establish conversion of extra streams.
	    bytes changed(data.begin(), data.begin() + at);
	    changed.insert(changed.end(), stream.begin(), stream.end());
	    changed.insert(changed.end(), data.begin() + at, data.end());
	    put_word(changed, header_size_at, get_word(data, header_size_at) + static_cast<uint32_t>(stream.size()));
	    put_word(changed, sizeof(uint32_t), static_cast<uint32_t>(changed.size() - chunk_header_size));
	    require(!memcmp(changed.data() + children, "avih", list_type_size), "AVI fixture has no main header");
	    constexpr size_t main_header_streams_word = 6;
	    put_word(changed, children + chunk_header_size + main_header_streams_word * sizeof(uint32_t), 2);
	    return changed;
	}
	at = end;
    }
    throw std::runtime_error("AVI fixture has no stream list");
}

void check_builtin(icv_context *context, const icv_anim_t *source, const char *type)
{
    temporary file;
    require(!icv_anim_write_variant(context, source, file.path, type, NULL, NULL, NULL), "sequence output failed");
    animation_ptr decoded(icv_anim_read_variant(context, file.path, NULL, NULL, NULL, NULL), icv_anim_destroy);
    check_frames(decoded.get());
    require(BU_STR_EQUAL(icv_anim_mime_type(decoded.get()), type), "sequence lost its source identity");
    const bu_mime_trait unknown = BU_MIME_TRAIT_STRING("fixture.unestablished", "assertion");
    const bu_mime_traits request = {&unknown, 1};
    animation_ptr rejected(icv_anim_read_variant(context, file.path, type, NULL, &request, NULL), icv_anim_destroy);
    require(!rejected, "sequence accepted an unestablished input assertion");
    const bytes original = file.read();
    require(icv_anim_write_variant(context, source, file.path, type, NULL, &request, NULL) != 0 && file.read() == original,
	"rejected sequence output changed an existing destination");
    const bu_mime_trait ascii = BU_MIME_TRAIT_STRING("encoding", "ascii");
    const bu_mime_traits ascii_request = {&ascii, 1};
    require(icv_anim_write_variant(context, source, file.path, type, NULL, &ascii_request, NULL) != 0 && file.read() == original,
	"sequence output ignored its fixed wire encoding");
    if (BU_STR_EQUAL(type, "image/apng")) return;

    const char *members[] = {"avi", "riff"};
    const bu_mime_trait annotations[] = {BU_MIME_TRAIT_STRING("avi.header-present", "true"),
	BU_MIME_TRAIT_SET("fixture.avi-signatures", members, sizeof(members) / sizeof(members[0]))};
    const bu_mime_traits annotated = {annotations, sizeof(annotations) / sizeof(annotations[0])};
    decoded.reset(icv_anim_read_variant(context, file.path, type, NULL, &annotated, NULL));
    check_frames(decoded.get());
    context_ptr isolated(icv_context_create(NULL), icv_context_destroy);
    rejected.reset(icv_anim_read_variant(isolated.get(), file.path, type, NULL, &annotated, NULL));
    require(!rejected, "sequence observations escaped their context");


    for (bool audio_first : {false, true}) {
	file.write(pad_avi_header(extra_avi_stream(original, audio_first)));
	const size_t before = jpeg_reads;
	rejected.reset(icv_anim_read_variant(context, file.path, type, NULL, NULL, NULL));
	require(!rejected && jpeg_reads == before, "AVI entered a frame decoder for an unsupported stream arrangement");
    }

    const unsigned char handler[] = {'v','i','d','s','M','J','P','G'};
    bytes changed = original;
    const auto at = std::search(changed.begin(), changed.end(), std::begin(handler), std::end(handler));
    require(at != changed.end(), "AVI fixture has no stream handler");
    const size_t offset = static_cast<size_t>(at - changed.begin()) + sizeof(uint32_t);
    std::copy_n("DIB ", sizeof(uint32_t), changed.begin() + offset);
    file.write(changed);
    const size_t before = jpeg_reads;
    rejected.reset(icv_anim_read_variant(context, file.path, type, NULL, NULL, NULL));
    require(!rejected && jpeg_reads == before, "AVI entered a frame decoder for a different stream handler");

    // Retain actual MJPEG payloads while exercising the accepted lowercase declaration.
    changed = original;
    std::copy_n("mjpg", sizeof(uint32_t), changed.begin() + offset);
    const bytes padded = pad_avi_header(changed);
    file.write(padded);
    bu_mime_result *detected = NULL;
    require(icv_detect_file(context, file.path, &detected, NULL) == BU_MIME_MATCH, "long AVI header lost its identity");
    const bool deferred = detected->incomplete && detected->count &&
	!bu_mime_traits_get(&detected->candidates[0].traits, "avi.video-handler");
    bu_mime_result_free(detected);
    require(deferred, "bounded AVI inspection invented a stream handler");
    const bu_mime_trait lower = BU_MIME_TRAIT_STRING("avi.video-handler", "mjpg");
    const bu_mime_traits lower_request = {&lower, 1};
    decoded.reset(icv_anim_read_variant(context, file.path, type, NULL, &lower_request, NULL));
    check_frames(decoded.get());
}

void check_plugin(icv_context *context, const icv_anim_t *source, const char *plugin)
{
    require(icv_load_plugin(context, plugin, NULL) != 0, "cannot load sequence plugin");
    const char *type = "video/vnd.example.sequence";
    require(icv_supports(context, type, ICV_CODEC_READ_SEQUENCE | ICV_CODEC_WRITE_SEQUENCE) &&
	!icv_supports(context, type, ICV_CODEC_READ | ICV_CODEC_METADATA), "sequence plugin advertised still-image operations");
    temporary file;
    require(!icv_anim_write_variant(context, source, file.path, "video/x-example-sequence", NULL, NULL, NULL), "plugin sequence output failed");
    animation_ptr decoded(icv_anim_read_variant(context, file.path, NULL, NULL, NULL, NULL), icv_anim_destroy);
    check_frames(decoded.get());
    require(BU_STR_EQUAL(icv_anim_mime_type(decoded.get()), type), "plugin sequence retained an alias");
    const bu_mime_trait unknown = BU_MIME_TRAIT_STRING("fixture.unestablished", "assertion");
    const bu_mime_traits request = {&unknown, 1};
    animation_ptr rejected(icv_anim_read_variant(context, file.path, type, NULL, &request, NULL), icv_anim_destroy);
    require(!rejected, "plugin sequence ignored a full-reader assertion");
    icv_codec_info *snapshot = NULL;
    size_t count = 0;
    {
	context_ptr owner(icv_context_create(NULL), icv_context_destroy);
	require(owner && icv_load_plugin(owner.get(), plugin, NULL), "cannot create independent sequence context");
	decoded.reset(icv_anim_read_variant(owner.get(), file.path, NULL, NULL, NULL, NULL));
	require(icv_list_codecs(owner.get(), type, &snapshot, &count) && count == 1, "cannot snapshot sequence codec");
    }
    check_frames(decoded.get());
    require(BU_STR_EQUAL(icv_anim_mime_type(decoded.get()), type) && BU_STR_EQUAL(snapshot[0].mime_type, type),
	"sequence data borrowed its destroyed context's metadata");
    icv_codec_list_free(snapshot, count);
}

icv_anim_t *throw_read(icv_context *, const char *, const char *, const bu_mime_traits *, const bu_mime_traits *)
{
    throw std::runtime_error("fixture decoder exception");
}

int throw_write(icv_context *, const icv_anim_t *, const char *, const char *, const bu_mime_traits *)
{
    throw BRLCAD_ERROR;
}

void check_exceptions(icv_context *context, const icv_anim_t *source)
{
    const char *type = "video/vnd.example.throw";
    const bu_mime_definition definition = {BU_MIME_ABI_VERSION, sizeof(bu_mime_definition), type, NULL, NULL, 0, NULL, 0, NULL, 0};
    const bu_mime_capability format = BU_MIME_CAPABILITY(type, NULL, 0, 0);
    const icv_codec codec = {"Throwing sequence", &format, 1, BU_MIME_DEFAULT_PRIORITY,
	NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, throw_read, throw_write};
    const icv_plugin plugin = {ICV_PLUGIN_ABI_VERSION, sizeof(icv_plugin), &definition, 1, &codec, 1};
    require(icv_register_plugin(context, &plugin, NULL) != 0, "cannot register exception fixture");
    temporary file;
    const bytes marker = {'k','e','e','p'};
    file.write(marker);
    bu_vls messages = BU_VLS_INIT_ZERO;
    animation_ptr decoded(icv_anim_read_variant(context, file.path, type, NULL, NULL, &messages), icv_anim_destroy);
    require(!decoded && strstr(bu_vls_cstr(&messages), "fixture decoder exception"), "sequence decoder exception escaped its API");
    bu_vls_trunc(&messages, 0);
    require(icv_anim_write_variant(context, source, file.path, type, NULL, NULL, &messages) != 0 &&
	strstr(bu_vls_cstr(&messages), "exception") && file.read() == marker, "sequence encoder exception escaped its API");
    bu_vls_free(&messages);
    require(!icv_anim_write_variant(context, source, file.path, "image/apng", NULL, NULL, NULL), "sequence context failed after an exception");
    decoded.reset(icv_anim_read_variant(context, file.path, NULL, NULL, NULL, NULL));
    check_frames(decoded.get());
}
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    try {
	require(argc == 2, "expected the codec plugin path");
	context_ptr context(icv_context_create(NULL), icv_context_destroy);
	require(context != nullptr, "cannot create sequence context");
	const bu_mime_capability jpeg = BU_MIME_CAPABILITY("image/jpeg", NULL, 0, 0);
	const icv_codec codec = {"Context JPEG", &jpeg, 1, BU_MIME_MAX_PRIORITY,
	    NULL, jpeg_read, NULL, jpeg_write, NULL, 0, NULL, NULL, NULL, NULL};
	const icv_plugin plugin = {ICV_PLUGIN_ABI_VERSION, sizeof(icv_plugin), NULL, 0, &codec, 1};
	require(icv_register_plugin(context.get(), &plugin, NULL) != 0, "cannot register contextual JPEG codec");
	const bu_mime_capability avi = BU_MIME_CAPABILITY("video/x-msvideo", NULL, 0, ICV_CODEC_METADATA);
	const icv_codec annotation = {"AVI header annotation", &avi, 1, BU_MIME_DEFAULT_PRIORITY,
	    NULL, NULL, NULL, NULL, NULL, 0, avi_note_metadata, avi_note, NULL, NULL};
	const icv_plugin metadata_plugin = {ICV_PLUGIN_ABI_VERSION, sizeof(icv_plugin), NULL, 0, &annotation, 1};
	require(icv_register_plugin(context.get(), &metadata_plugin, NULL) != 0, "cannot register independent header observations");
	require(icv_supports(context.get(), "image/apng", ICV_CODEC_READ_SEQUENCE | ICV_CODEC_WRITE_SEQUENCE) &&
	    !icv_supports(context.get(), "image/png", ICV_CODEC_READ_SEQUENCE), "sequence operation discovery is incorrect");
	bu_vls selected = BU_VLS_INIT_ZERO;
	require(icv_resolve_variant(context.get(), ICV_CODEC_WRITE_SEQUENCE, "output.PNG", NULL, NULL, NULL, &selected, NULL, NULL, NULL) &&
	    BU_STR_EQUAL(bu_vls_cstr(&selected), "image/apng"), "PNG sequence output did not resolve to APNG");
	require(icv_resolve_variant(context.get(), ICV_CODEC_WRITE, "output.PNG", NULL, NULL, NULL, &selected, NULL, NULL, NULL) &&
	    BU_STR_EQUAL(bu_vls_cstr(&selected), "image/png"), "still PNG output became ambiguous");
	bu_vls_free(&selected);
	auto animation = make_animation();
	check_builtin(context.get(), animation.get(), "image/apng");
	check_builtin(context.get(), animation.get(), "video/x-msvideo");
	require(jpeg_reads >= 4 && jpeg_writes == 2, "AVI did not reuse its caller's frame codec context");
	check_plugin(context.get(), animation.get(), argv[1]);
	check_exceptions(context.get(), animation.get());
    } catch (const std::exception &error) {
	bu_log("Sequence profile test: %s\n", error.what());
	return 1;
    }
    return 0;
}
