#include "main.h"
#include "pin_config.h"
#include "wavfile.h"
#include "dac.h"
#include "diskio.h"
#include "pff.h"
#include "buttons.h"
#include "hibernate.h"
#include "filenames.c"
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>

volatile uint8_t new_sound = 0, new_sound_id = 0;
uint8_t special_mode = 0;
uint16_t credits_counter = 0;
#define CREDITS_COUNTER_MAX 1000
WAVinfo_t wavinfo;
FATFS fs;

// unknown interrupt
//ISR(BADISR_vect) {
//}

void sound_gut(void) {
	wavinfo.bits_per_sample = 8;
	wavinfo.sample_rate = 44100;
	wavinfo.block_align = 1;
	wavinfo.num_channels = 1;
	for (uint8_t i = 0; i<200; i++) {
		for (uint8_t j = 0; j<100; j++) process_audio(0x70);
		for (uint8_t j = 0; j<100; j++) process_audio(0x90);
	}
	stop_audio();
}

void sound_osch(void) {
	wavinfo.bits_per_sample = 8;
	wavinfo.sample_rate = 44100;
	wavinfo.block_align = 1;
	wavinfo.num_channels = 1;
	for (uint8_t i = 0; i<255; i++) {
		for (uint8_t j = 0; j<11; j++) process_audio(0x70);
		for (uint8_t j = 0; j<19; j++) process_audio(0x90);
	}
	stop_audio();
}

int main(void) {
	FRESULT res;
	WORD br;

	sei(); // Globally enable interrupts

	hibernate_init();
	DAC_init();
	keys_init();

	if (pf_mount(&fs)) { sound_osch(); }
	else { sound_gut(); }

	while(1) {
		cli(); // disable interrupts to avoid race condition with sleep function
		if (new_sound) {
			hibernate_timer_stop();
			new_sound = 0;
			sei();
			switch (special_mode) {
				case 1:
				if (new_sound_id == 18) {
					special_mode = 2;
					goto sound_ende;
				} else if (new_sound_id == 24) {
					special_mode = 4;
					goto sound_ende;
				} else special_mode = 0;
				break;
				case 2:
				special_mode = 3;
				break;
				case 4:
				special_mode = 5;
				break;
				default:
				special_mode = 0;
			}
			if (new_sound_id == 36) {
				special_mode = 1;
				goto sound_ende;
			}
			char* filename;
			if (++credits_counter > CREDITS_COUNTER_MAX) {
				credits_counter = 0;
				filename = "image.hex";
			} else {
				filename = filenames(new_sound_id);
				if (filename == NULL) goto sound_ende;
			}
			uint8_t tries = 3;
			while (pf_open(filename) && pf_open("error1.wav")) {
				if ((tries--) == 0) goto sound_ende;
				_delay_ms(10);
				pf_mount(&fs);
			}
			if (parse_wav_header()) {
				if (pf_open("error2.wav") || parse_wav_header()) goto sound_ende;
			}
			do {
				#define read_length 16384
				if (wavinfo.data_length > read_length) {
					res = pf_read(0, read_length, &br);
					wavinfo.data_length -= read_length;
				} else {
					res = pf_read(0, wavinfo.data_length, &br);
					break;
				}
			} while (res==0 && br==read_length && wavinfo.data_length>0 && !new_sound);
			stop_audio();
			sound_ende:
			hibernate_timer_init();
		} else {
			sleep_enable();
			sei();
			sleep_cpu();
			sleep_disable();
		}
		hibernate_check();
	}
	
	return 0;
}
