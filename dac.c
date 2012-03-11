#include "main.h"
#include "wavfile.h"
#include "pin_config.h"
#include "dac.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

static uint8_t sample_buffer[4];
static uint8_t sample_pos = 0;

static uint16_t audio_buffer[256];
static volatile uint8_t audio_rpos = 0;
static uint8_t audio_wpos = 0;
static uint8_t buffer_ready = 0;

static volatile uint8_t low_byte, sending;

extern WAVinfo_t wavinfo;

static inline void DAC_timer_start(void);
static inline void DAC_timer_stop(void);
static inline void DAC_output(uint16_t data);

#define SILENCE 0x800

void process_audio(uint8_t data_byte) {
	sample_buffer[sample_pos++] = data_byte;
	if (sample_pos==wavinfo.block_align) {
		if (buffer_ready) {
			// playback running, wait if buffer is full
			while (audio_rpos == audio_wpos);
		} else {
			// start playback as soon half of the buffer is filled
			if (audio_wpos == 128) {
				buffer_ready = 1;
				DAC_timer_start();
			}
		}
		if (wavinfo.bits_per_sample==8) {
			if(wavinfo.num_channels==1) {
				// 8 bit mono
				audio_buffer[audio_wpos++] = sample_buffer[0]<<4;
			} else {
				// 8 bit stereo
				audio_buffer[audio_wpos++] = (sample_buffer[0]<<3) + (sample_buffer[1]<<3);
			}
		} else {
			if(wavinfo.num_channels==1) {
				// 16 bit mono
				sample_buffer[1]+=1<<7;
				audio_buffer[audio_wpos++] = (sample_buffer[1]<<4) + (sample_buffer[0]>>4);
			} else {
				// 16 bit stereo
				sample_buffer[1]+=1<<7;
				sample_buffer[3]+=1<<7;
				audio_buffer[audio_wpos++] = ((sample_buffer[3]<<4) + (sample_buffer[2]>>4) + (sample_buffer[1]<<4) + (sample_buffer[0]>>4))>>1;
			}
		}
		sample_pos = 0;
		//if (audio_wpos==A_BUF_SIZE) audio_wpos = 0;
	}
}

void stop_audio(void) {
	// apply a linear fade out
	/*MÜLL!
	uint8_t f1 = audio_rpos + 1, f2 = 1, c;
	while (f2) {
		for (c=0; c<f2; c++) audio_buffer[f1] -= (audio_buffer[f1]>>8);
		f1++;
		f2++;
	}
	while (audio_rpos != f1); */

	DAC_timer_stop();
	_delay_us(30);   // wait for transmission to complete
	DAC_output(SILENCE);
	audio_rpos = 0;
	audio_wpos = 0;
	buffer_ready = 0;
}

static inline void DAC_timer_start(void) {
	//set timer1 output compare A value
	switch (wavinfo.sample_rate) {
		case 8000:
		OCR1A = 2000;
		break;
		case 11025:
		OCR1A = 1451;
		break;
		case 22050:
		OCR1A = 726;
		break;
		case 44100:
		OCR1A = 363;
		break;
	}
	TCCR1B |= _BV(CS10);		//start timer, no prescaling
}

static inline void DAC_timer_stop(void) {
	TCCR1B &= ~(_BV(CS10));		//stop timer
	TCNT1 = 0;					//reset timer
}

void DAC_init(void) {
	//empty audio buffer
	/* uint8_t b = 0;
	do audio_buffer[b++] = SILENCE;
	while (b != 0); */
	
	// set LOAD pin as output and go to idle state (high)
	DACDDR |= (1 << DACPIN_LOAD);
	DACLOAD(LEV_HIGH);
	// init SPI output
	DDRB |= _BV(DDB2) | _BV(DDB3) | _BV(DDB5);
	SPCR = _BV(SPIE) | _BV(SPE) | _BV(MSTR) | _BV(SPR0);
	SPSR = _BV(SPI2X);
	
	DAC_output(SILENCE);

	// init audio sample timer
	TCCR1B = _BV(WGM12);			//CTC mode
	TIMSK1 = _BV(OCIE1A);			//enable timer1 output compare A interrupt
	// note: timer is not running yet
}

void DAC_shutdown(void) {
	stop_audio();
	// set LOAD tri-state
	DACDDR &= ~(1 << DACPIN_LOAD);
	DACLOAD(LEV_LOW);
	// disable SPI
	SPCR = 0;
	DDRB &= ~(_BV(DDB2) | _BV(DDB3) | _BV(DDB5));
}

ISR(TIMER1_COMPA_vect) {
	// this routine is executed at the sampling rate
	// timer values are adjusted for a rate of 44.25 kHz
	DAC_output(audio_buffer[audio_rpos++]);
}

static inline void DAC_output(uint16_t data) {
	low_byte = data;
	sending = 1;
	SPDR = data>>8;
}

ISR(SPI_STC_vect) {
	// transmission completed
	if (sending) {
		// send second byte
		sending = 0;
		SPDR = low_byte;
	} else {
		// both bytes sent, trigger LOAD line
		DACLOAD(LEV_LOW);
		//_delay_us (0.15); // min LOAD pulse width (see LTC 1257 docs)
		DACLOAD(LEV_HIGH);
	}
}
