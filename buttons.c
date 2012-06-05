#include "main.h"
#include "dac.h"
#include "buttons.h"
#include "pin_config.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

extern volatile uint8_t new_sound_id;
static uint8_t debounce_count = 0;
extern volatile uint8_t bank;

static void set_keys_mode1(void) {
	//inputs
	DDRC &= ~(CMASK);
	PORTC |= CMASK;
	//outputs
	DDRB |= BMASK;
	PORTB &= ~(BMASK);
	DDRD |= DMASK;
	PORTD &= ~(DMASK);
	_delay_us(SWITCH_TIME);
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
	_delay_us(SWITCH_TIME);
}

void keys_init(void) {
	set_keys_mode1();
	// set pin change interrupt mask
	PCMSK1 = CMASK;
	// clear pin change interrupt flag
	PCIFR |= _BV(PCIF1);
	// enable pin change interrupt
	PCICR |= _BV(PCIE1);
	// enable debounce timer interrupt
	TIMSK2 |= _BV(TOIE2);
}

inline void next_bank(void) {
	if (++bank == NUM_BANKS) bank = 0;
}

static uint8_t get_key(void) {
	uint8_t columns = (~PINC) & CMASK;
	set_keys_mode2();
	uint8_t rows = ((~PINB) & BMASK) + ((~PIND) & DMASK);
	set_keys_mode1();
	if ((rows == row4) && (columns == (col3 | col5))) return 36;
	uint8_t id;
	switch (rows) {
		case row0:
		id = 0;
		break;
		case row1:
		id = 6;
		break;
		case row2:
		id = 12;
		break;
		case row3:
		id = 18;
		break;
		case row4:
		id = 24;
		break;
		case row5:
		id = 30;
		break;
		default:
		return 255;
	}
	switch (columns) {
		case col0:
		id += 0;
		break;
		case col1:
		id += 1;
		break;
		case col2:
		id += 2;
		break;
		case col3:
		id += 3;
		break;
		case col4:
		id += 4;
		break;
		case col5:
		id += 5;
		break;
		default:
		return 255;
	}
	return id;
}

ISR(PCINT1_vect) {  // pin change
	// disable pin change interrupt
	PCICR &= ~(_BV(PCIE1));
	// start debounce timer, prescaler /256
	TCCR2B |= _BV(CS22) | _BV(CS21);
}

ISR(TIMER2_OVF_vect) {  // debounce timer
	if (debounce_count++ == DEBOUNCE_COUNT_MAX) {
		debounce_count = 0;
		if ((~PINC) & CMASK) {
			uint8_t id = get_key();
			switch (id) {
				case 32:
				next_bank();
				break;
				case 30:
				volume_up();
				break;
				case 31:
				volume_down();
				break;
				//case 255:
				//break;
				default:
				//if (buffer_ready) cancel_play();
				FLAG_SET(NEW_SOUND);
				new_sound_id = id;
			}
		}
		// stop and reset timer
		TCCR2B = 0;
		TCNT2 = 0;
		// clear pin change interrupt flag
		PCIFR |= _BV(PCIF1);
		// enable pin change interrupt
		PCICR |= _BV(PCIE1);
	}
}
