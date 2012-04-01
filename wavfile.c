#include "wavfile.h"
#include "pff.h"

extern WAVinfo_t wavinfo;

static uint8_t seek(uint32_t offset) {
	uint8_t dummy[8];
	WORD br;
	while (offset>8) {
		if (pf_read(&dummy, 8, &br)) return 1;
		if (br != 8) return 1;
		offset -= 8;
	}
	if (pf_read(&dummy, offset, &br)) return 1;
	if (br != offset) return 1;
	return 0;
}

uint8_t parse_wav_header(void) {
	WAVChunk_t WAVChunk;
	WAVfmtChunkData_t WAVfmt;
	WORD br;
	
	pf_read(&WAVChunk, sizeof(WAVChunk_t), &br);
	if (br != sizeof(WAVChunk_t)) return 1;
	if (WAVChunk.chunk_id != 0x46464952) return 1;  //  "RIFF"
	pf_read(&WAVChunk, 4, &br);
	if (br != 4) return 1;
	if (WAVChunk.chunk_id != 0x45564157) return 1;  //  "WAVE"

	while(1) {
		if (pf_read(&WAVChunk, sizeof(WAVChunk_t), &br)) return 1;
		if (br != sizeof(WAVChunk_t)) return 1;
		switch (WAVChunk.chunk_id) {
		case 0x20746d66:  // "fmt "
			if (WAVChunk.chunk_size < sizeof(WAVfmtChunkData_t)) return 1;
			if (pf_read(&WAVfmt, sizeof(WAVfmtChunkData_t), &br)) return 1;
			if (br != sizeof(WAVfmtChunkData_t)) return 1;
			if (WAVfmt.audio_format != 1 || 
				(WAVfmt.num_channels != 1 && WAVfmt.num_channels != 2) ||
				(WAVfmt.sample_rate != 48000 && WAVfmt.sample_rate != 44100 && WAVfmt.sample_rate != 22050 && WAVfmt.sample_rate != 11025 && WAVfmt.sample_rate != 8000) ||
				(WAVfmt.bits_per_sample != 8 && WAVfmt.bits_per_sample != 16) ||
				(WAVfmt.num_channels == 2 && WAVfmt.sample_rate == 48000 && WAVfmt.bits_per_sample == 16) ||
				WAVfmt.block_align > 4) return 2;
			wavinfo.num_channels = WAVfmt.num_channels;
			wavinfo.sample_rate = WAVfmt.sample_rate;
			wavinfo.bits_per_sample = WAVfmt.bits_per_sample;
			wavinfo.block_align = WAVfmt.block_align;
			if (WAVChunk.chunk_size > sizeof(WAVfmtChunkData_t)) {
				if (seek(WAVChunk.chunk_size - sizeof(WAVfmtChunkData_t))) return 1;
			}
			break;
		case 0x61746164:  // "data"
			wavinfo.data_length = WAVChunk.chunk_size;
			return 0;
		default:
			if (seek(WAVChunk.chunk_size)) return 1;
		}
	}
}
