#include "main.h"
#include "pin_config.h"
#include "wavfile.h"
#include "dac.h"
#include "diskio.h"
#include "pff.h"
#include "buttons.h"
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdint.h>

volatile uint8_t new_sound = 0, new_sound_id=0;

WAVinfo_t wavinfo;

static FATFS fs;
static volatile uint32_t hibernate_count = 0;
static uint8_t hibernating = 0;

ISR(TIMER0_OVF_vect) {
	hibernate_count++;
}

ISR(BADISR_vect) {
} // unknown interrupt

void MOSFET_init() {
	MOSFETDDR |= _BV(MOSFET);
}
void MOSFET_on() {
	MOSFETPORT |= _BV(MOSFET);
	_delay_ms(200);
}
void MOSFET_off() {
	MOSFETPORT &= ~(_BV(MOSFET));
}

static void hibernate_timer_init(void) {
	hibernate_count = 0;
	TCNT0 = 0;
	TCCR0B |= _BV(CS02) | _BV(CS00);	// prescaler /1024
	TIMSK0 |= _BV(TOIE0);				// enable overflow interrupt
}
static void hibernate_timer_stop(void) {
	hibernate_count = 0;
	TCCR0B &= ~(_BV(CS02) | _BV(CS01) | _BV(CS00)); // stop timer
	TIFR0 |= _BV(TOV0); // clear pending interrupt
}

static void hibernate(void) {
	hibernating = 1;
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	DAC_shutdown();
	disk_shutdown();
	MOSFET_off();
}
static void wakeup(void) {
	hibernating = 0;
	set_sleep_mode(SLEEP_MODE_IDLE);
	MOSFET_on();
	DAC_init();
	pf_mount(&fs);
}
static void hibernate_reset(void) {
	hibernate_count = 0;
}

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

	MOSFET_init();
	MOSFET_on();
	DAC_init();
	keys_init();
	hibernate_timer_init();

	if (pf_mount(&fs)) { sound_osch(); }
	else { sound_gut(); }

	while(1) {
		cli(); // disable interrupts to avoid race condition with sleep function
		if (new_sound) {
			hibernate_timer_stop();
			new_sound = 0;
			sei();
			switch (new_sound_id) {
			case 0:
				continue;
			case 1:
				pf_open("1.wav");
				break;
			case 2:
				pf_open("2.wav");
				break;
			case 3:
				pf_open("3.wav");
				break;
			case 4:
				pf_open("4.wav");
				break;
			}
			if (parse_wav_header()) {
				// maybe not mounted?
				if (pf_mount(&fs)) continue;
				else { // try again
					if (parse_wav_header()) continue;
				}
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
			hibernate_timer_init();
		} else {
			sleep_enable();
			sei();
			sleep_cpu();
			sleep_disable();
		}
		cli();
		if (hibernating) {
			wakeup();
		} else {
			if (hibernate_count > HIBER_COUNT_MAX) {
				hibernate_timer_stop();
				hibernate();
			}
		}
		sei();
	}
	
	return 0;
}
