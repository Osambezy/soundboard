#include "buttons.h"
#include "pin_config.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

extern volatile uint8_t new_sound, new_sound_id;
static volatile uint8_t debounce_count = 0;

static void set_keys_mode1(void) {
	//inputs
	DDRC &= ~(CMASK);
	PORTC |= CMASK;
	//outputs
	DDRB |= BMASK;
	PORTB &= ~(BMASK);
	DDRD |= DMASK;
	PORTD &= ~(DMASK);
}
static void set_keys_mode2(void) {
	//inputs
	DDRB &= ~(BMASK);
	PORTB |= BMASK;
	DDRD &= ~(DMASK);
	PORTD |= DMASK;
	//outputs
	DDRC |= CMASK;
	PORTC &= ~(CMASK);
}

void keys_init(void) {
	set_keys_mode1();
	// set pin change interrupt mask
	PCMSK1 = CMASK;
	// enable pin change interrupt
	PCICR |= _BV(PCIE1);
}

static uint8_t get_key(void) {
	uint8_t columns = (~PINC) & CMASK;
	set_keys_mode2();
	_delay_us(10);
	uint8_t rows = ((~PINB) & BMASK) + ((~PIND) & DMASK);
	set_keys_mode1();
	if (columns & _BV(4)) {
		if (rows & _BV(0)) return 1;
		else if (rows & _BV(7)) return 4;
	} else if (columns & _BV(5)) {
		if (rows & _BV(0)) return 2;
		else if (rows & _BV(7)) return 3;
	}
	return 0;
}

ISR(PCINT1_vect){
	PCICR &= ~(_BV(PCIE1));		// disable pin change interrupt
	_delay_us(10);
	if ((~PINC) & CMASK) {
		new_sound = 1;
		new_sound_id = get_key();
	}
	TCCR2B |= _BV(CS22) | _BV(CS21) | _BV(CS20); // prescaler /1024 (16.3ms until OVF)
	TIMSK2 |= _BV(TOIE2);		// enable debounce timer interrupt
}

ISR(TIMER2_OVF_vect) {
	debounce_count++;
	if (debounce_count == 10) {
		debounce_count = 0;
		// stop and reset timer, disable interrupt
		TCCR2B = 0;
		TCNT2 = 0;
		TIMSK2 &= ~(_BV(TOIE2));
		// clear pin change interrupt flag
		PCIFR |= _BV(PCIF1);
		// enable pin change interrupt
		PCICR |= _BV(PCIE1);
	}
}
