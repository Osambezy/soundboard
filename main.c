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

inline void MOSFET_init() {
	MOSFETDDR |= _BV(MOSFET);
}
inline void MOSFET_on() {
	MOSFETPORT |= _BV(MOSFET);
}
inline void MOSFET_off() {
	MOSFETPORT &= ~(_BV(MOSFET));
}

static void hibernate_timer_init(void) {
	hibernate_count = 0;
	TCNT0 = 0;
	TCCR0B |= _BV(CS02) | _BV(CS00);	// prescaler /1024
	TIMSK0 |= _BV(TOIE0);				// enable overflow interrupt
}
static void hibernate(void) {
	// stop and reset timer, disable interrupt
	TCCR0B = 0;
	TIMSK0 &= ~(_BV(TOIE0));
	hibernating = 1;
	DAC_shutdown();
	disk_shutdown();
	MOSFET_off();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}
static void wakeup(void) {
	MOSFET_on();
	hibernate_timer_init();
	hibernating = 0;
	set_sleep_mode(SLEEP_MODE_IDLE);
	_delay_ms(10);
	DAC_init();
	pf_mount(&fs);
}
static void hibernate_reset(void) {
	hibernate_count = 0;
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
	pf_mount(&fs);

	while(1) {
		//TODO: rewrite this loop!
		
		cli(); // disable interrupts to avoid race condition with sleep function
		if (new_sound) {
			sei();
			if (hibernating) wakeup();
			else hibernate_reset();
			new_sound = 0;
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
		} else {
			sleep_enable();
			sei();
			sleep_cpu();
			sleep_disable();
		}
		if (hibernate_count > HIBER_COUNT_MAX) { //TODO: make atomic!
			hibernate();
		}
	}
	
	return 0;
}
