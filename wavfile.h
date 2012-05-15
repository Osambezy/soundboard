#ifndef _WAVFILE_H_
#define _WAVFILE_H_

#include <stdint.h>

typedef struct {
    uint32_t	chunk_id;
    uint32_t	chunk_size;
} WAVChunk_t;

typedef struct {
    uint16_t	audio_format;
    uint16_t	num_channels;
    uint32_t	sample_rate;
    uint32_t	byte_rate;
    uint16_t	block_align;
    uint16_t	bits_per_sample;
} WAVfmtChunkData_t;

typedef struct {
	uint8_t		num_channels;
	uint16_t	sample_rate;
	uint8_t		bits_per_sample;
	uint8_t		block_align;
	uint32_t	data_length;
} WAVinfo_t;

uint8_t parse_wav_header(void);

#endif
