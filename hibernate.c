#include "main.h"
#include "hibernate.h"
#include "pin_config.h"
#include "dac.h"
#include "diskio.h"
#include "pff.h"
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

static volatile uint32_t hibernate_count = 0;
static uint8_t hibernating = 0;

extern FATFS fs;

ISR(TIMER0_OVF_vect) {
	hibernate_count++;
}

static inline void MOSFET_init() {
	MOSFETDDR |= _BV(MOSFET);
}
static inline void MOSFET_on() {
	MOSFETPORT |= _BV(MOSFET);
	_delay_ms(200);
}
static inline void MOSFET_off() {
	MOSFETPORT &= ~(_BV(MOSFET));
}

void hibernate_timer_init(void) {
	hibernate_count = 0;
	TCNT0 = 0;
	TCCR0B |= _BV(CS02) | _BV(CS00);	// prescaler /1024
	TIMSK0 |= _BV(TOIE0);				// enable overflow interrupt
}
void hibernate_timer_stop(void) {
	TCCR0B &= ~(_BV(CS02) | _BV(CS01) | _BV(CS00)); // stop timer
	TIFR0 |= _BV(TOV0); // clear pending interrupt
	hibernate_count = 0;
}

static void hibernate(void) {
	hibernate_timer_stop();
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
	hibernate_timer_init();
}

void hibernate_init(void) {
	MOSFET_init();
	MOSFET_on();
	hibernate_timer_init();
}

void hibernate_check(void) {
	TIMSK0 &= ~(_BV(TOIE0)); //disable timer interrupt
	if (hibernating) {
		wakeup();
	} else {
		if (hibernate_count > HIBER_COUNT_MAX) {
			hibernate();
		}
	}
	TIMSK0 |= _BV(TOIE0); //enable timer interrupt
}
