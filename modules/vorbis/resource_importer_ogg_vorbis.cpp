/*************************************************************************/
/*  resource_importer_ogg_vorbis.cpp                                     */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "resource_importer_ogg_vorbis.h"

#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "scene/resources/texture.h"
#include "thirdparty/libogg/ogg/ogg.h"
#include "thirdparty/libvorbis/vorbis/codec.h"

#ifdef TOOLS_ENABLED
#include "editor/import/audio_stream_import_settings.h"
#endif

String ResourceImporterOGGVorbis::get_importer_name() const {
	return "oggvorbisstr";
}

String ResourceImporterOGGVorbis::get_visible_name() const {
	return "oggvorbisstr";
}

void ResourceImporterOGGVorbis::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("ogg");
}

String ResourceImporterOGGVorbis::get_save_extension() const {
	return "oggvorbisstr";
}

String ResourceImporterOGGVorbis::get_resource_type() const {
	return "AudioStreamOGGVorbis";
}

bool ResourceImporterOGGVorbis::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

int ResourceImporterOGGVorbis::get_preset_count() const {
	return 0;
}

String ResourceImporterOGGVorbis::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterOGGVorbis::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "loop"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "loop_offset"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "bpm", PROPERTY_HINT_RANGE, "0,400,0.01,or_greater"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "beat_count", PROPERTY_HINT_RANGE, "0,512,or_greater"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "bar_beats", PROPERTY_HINT_RANGE, "2,32,or_greater"), 4));
}

Ref<AudioStreamOGGVorbis> ResourceImporterOGGVorbis::import_ogg_vorbis(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), Ref<AudioStreamOGGVorbis>(), "Cannot open file '" + p_path + "'.");

	uint64_t len = f->get_length();

	Vector<uint8_t> file_data;
	file_data.resize(len);
	uint8_t *w = file_data.ptrw();

	f->get_buffer(w, len);

	Ref<AudioStreamOGGVorbis> ogg_vorbis_stream;
	ogg_vorbis_stream.instantiate();

	Ref<OGGPacketSequence> ogg_packet_sequence;
	ogg_packet_sequence.instantiate();

	ogg_stream_state stream_state;
	ogg_sync_state sync_state;
	ogg_page page;
	ogg_packet packet;
	bool initialized_stream = false;

	ogg_sync_init(&sync_state);
	int err;
	size_t cursor = 0;
	size_t packet_count = 0;
	bool done = false;
	while (!done) {
		ERR_FAIL_COND_V_MSG((err = ogg_sync_check(&sync_state)), Ref<AudioStreamOGGVorbis>(), "Ogg sync error " + itos(err));
		while (ogg_sync_pageout(&sync_state, &page) != 1) {
			if (cursor >= len) {
				done = true;
				break;
			}
			ERR_FAIL_COND_V_MSG((err = ogg_sync_check(&sync_state)), Ref<AudioStreamOGGVorbis>(), "Ogg sync error " + itos(err));
			char *sync_buf = ogg_sync_buffer(&sync_state, OGG_SYNC_BUFFER_SIZE);
			ERR_FAIL_COND_V_MSG((err = ogg_sync_check(&sync_state)), Ref<AudioStreamOGGVorbis>(), "Ogg sync error " + itos(err));
			ERR_FAIL_COND_V(cursor > len, Ref<AudioStreamOGGVorbis>());
			size_t copy_size = len - cursor;
			if (copy_size > OGG_SYNC_BUFFER_SIZE) {
				copy_size = OGG_SYNC_BUFFER_SIZE;
			}
			memcpy(sync_buf, &file_data[cursor], copy_size);
			ogg_sync_wrote(&sync_state, copy_size);
			cursor += copy_size;
			ERR_FAIL_COND_V_MSG((err = ogg_sync_check(&sync_state)), Ref<AudioStreamOGGVorbis>(), "Ogg sync error " + itos(err));
		}
		if (done) {
			break;
		}
		ERR_FAIL_COND_V_MSG((err = ogg_sync_check(&sync_state)), Ref<AudioStreamOGGVorbis>(), "Ogg sync error " + itos(err));

		// Have a page now.
		if (!initialized_stream) {
			if (ogg_stream_init(&stream_state, ogg_page_serialno(&page))) {
				ERR_FAIL_V_MSG(Ref<AudioStreamOGGVorbis>(), "Failed allocating memory for OGG Vorbis stream.");
			}
			initialized_stream = true;
		}
		ogg_stream_pagein(&stream_state, &page);
		ERR_FAIL_COND_V_MSG((err = ogg_stream_check(&stream_state)), Ref<AudioStreamOGGVorbis>(), "Ogg stream error " + itos(err));
		int desync_iters = 0;

		Vector<Vector<uint8_t>> packet_data;
		int64_t granule_pos = 0;

		while (true) {
			err = ogg_stream_packetout(&stream_state, &packet);
			if (err == -1) {
				// According to the docs this is usually recoverable, but don't sit here spinning forever.
				desync_iters++;
				ERR_FAIL_COND_V_MSG(desync_iters > 100, Ref<AudioStreamOGGVorbis>(), "Packet sync issue during ogg import");
				continue;
			} else if (err == 0) {
				// Not enough data to fully reconstruct a packet. Go on to the next page.
				break;
			}
			if (packet_count == 0 && vorbis_synthesis_idheader(&packet) == 0) {
				print_verbose("Found a non-vorbis-header packet in a header position");
				// Clearly this logical stream is not a vorbis stream, so destroy it and try again with the next page.
				if (initialized_stream) {
					ogg_stream_clear(&stream_state);
					initialized_stream = false;
				}
				break;
			}
			granule_pos = packet.granulepos;

			PackedByteArray data;
			data.resize(packet.bytes);
			memcpy(data.ptrw(), packet.packet, packet.bytes);
			packet_data.push_back(data);
			packet_count++;
		}
		if (initialized_stream) {
			ogg_packet_sequence->push_page(granule_pos, packet_data);
		}
	}
	if (initialized_stream) {
		ogg_stream_clear(&stream_state);
	}
	ogg_sync_clear(&sync_state);

	if (ogg_packet_sequence->get_packet_granule_positions().is_empty()) {
		ERR_FAIL_V_MSG(Ref<AudioStreamOGGVorbis>(), "OGG Vorbis decoding failed. Check that your data is a valid OGG Vorbis audio stream.");
	}

	ogg_vorbis_stream->set_packet_sequence(ogg_packet_sequence);

	return ogg_vorbis_stream;
}

#ifdef TOOLS_ENABLED

bool ResourceImporterOGGVorbis::has_advanced_options() const {
	return true;
}

void ResourceImporterOGGVorbis::show_advanced_options(const String &p_path) {
	Ref<AudioStreamOGGVorbis> ogg_stream = import_ogg_vorbis(p_path);
	if (ogg_stream.is_valid()) {
		AudioStreamImportSettings::get_singleton()->edit(p_path, "oggvorbisstr", ogg_stream);
	}
}
#endif

Error ResourceImporterOGGVorbis::import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	bool loop = p_options["loop"];
	float loop_offset = p_options["loop_offset"];
	double bpm = p_options["bpm"];
	int beat_count = p_options["beat_count"];
	int bar_beats = p_options["bar_beats"];

	Ref<AudioStreamOGGVorbis> ogg_vorbis_stream = import_ogg_vorbis(p_source_file);
	if (ogg_vorbis_stream.is_null()) {
		return ERR_CANT_OPEN;
	}

	ogg_vorbis_stream->set_loop(loop);
	ogg_vorbis_stream->set_loop_offset(loop_offset);
	ogg_vorbis_stream->set_bpm(bpm);
	ogg_vorbis_stream->set_beat_count(beat_count);
	ogg_vorbis_stream->set_bar_beats(bar_beats);

	return ResourceSaver::save(p_save_path + ".oggvorbisstr", ogg_vorbis_stream);
}

ResourceImporterOGGVorbis::ResourceImporterOGGVorbis() {
}
