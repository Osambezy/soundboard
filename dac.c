#include "main.h"
#include "wavfile.h"
#include "pin_config.h"
#include "dac.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

static uint8_t sample_buffer[4];
static uint8_t sample_pos;

static uint16_t audio_buffer[256];
static volatile uint8_t audio_rpos;
static uint8_t audio_wpos;

static volatile uint8_t volume = 3;

#define low_byte GPIOR1

extern WAVinfo_t wavinfo;
extern uint8_t special_mode;

static void DAC_timer_start(void);
static void DAC_timer_stop(void);
static inline void DAC_output(uint16_t data);
static void DAC_silence(void);

#define SILENCE 0x800

void process_audio(uint8_t data_byte) {
	static uint8_t double_speed = 0;

	sample_buffer[sample_pos++] = data_byte;
	
	if (sample_pos==wavinfo.block_align) {
		sample_pos = 0;
		if (!FLAG_CHECK(PLAYING)) return;
		if (special_mode == 5) {
			if (double_speed) {
				double_speed = 0;
				return;
			} else double_speed = 1;
		}
		if (FLAG_CHECK(BUFFER_READY)) {
			// playback running, wait if buffer is full
			while (audio_rpos == audio_wpos);
		} else {
			// start playback as soon half of the buffer is filled
			if (audio_wpos == 128) {
				FLAG_SET(BUFFER_READY);
				DAC_timer_start();
			}
		}
		uint16_t sample;
		if (wavinfo.bits_per_sample==8) {
			if(wavinfo.num_channels==1) {
				// 8 bit mono
				sample = sample_buffer[0]<<4;
			} else {
				// 8 bit stereo
				sample = (sample_buffer[0]<<3) + (sample_buffer[1]<<3);
			}
		} else {
			if(wavinfo.num_channels==1) {
				// 16 bit mono
				uint8_t s1 = sample_buffer[1]+(1<<7);
				sample = (s1<<4) + (sample_buffer[0]>>4);
			} else {
				// 16 bit stereo
				uint8_t s1 = sample_buffer[1]+(1<<7);
				uint8_t s3 = sample_buffer[1]+(1<<7);
				sample = ((s3<<4) + (sample_buffer[2]>>4) + (s1<<4) + (sample_buffer[0]>>4))>>1;
			}
		}
		switch (volume) {
			case 2:
			sample >>= 1;
			sample += 0x400;
			break;
			case 1:
			sample >>= 2;
			sample += 0x600;
			break;
			case 0:
			sample >>= 3;
			sample += 0x700;
			break;
			default:
			break;
		}
		audio_buffer[audio_wpos++] = sample;
	}
}

void stop_audio(void) {
	DAC_timer_stop();
	DAC_silence();
	audio_rpos = 0;
	audio_wpos = 0;
	FLAG_CLEAR(BUFFER_READY);
	sample_pos = 0;
	FLAG_SET(PLAYING);
}

void cancel_play(void) {
	// interrupts the current sound when a new button is pressed
	DAC_timer_stop();
	FLAG_CLEAR(PLAYING);
}

static void DAC_timer_start(void) {
	//set timer1 output compare A value
	if (special_mode == 3 || special_mode == 5) {
		// Gaudi mode
		switch (wavinfo.sample_rate) {
			case 8000:
			OCR1A = (F_CPU / 6000);
			break;
			case 11025:
			OCR1A = (F_CPU / 8000);
			break;
			case 22050:
			OCR1A = (F_CPU / 16000);
			break;
			case 44100:
			default:
			OCR1A = (F_CPU / 32000);
			break;
			case 48000:
			OCR1A = (F_CPU / 35000);
		}
	} else {
		switch (wavinfo.sample_rate) {
			case 8000:
			OCR1A = (F_CPU / 8000);
			break;
			case 11025:
			OCR1A = (F_CPU / 11025);
			break;
			case 22050:
			OCR1A = (F_CPU / 22050);
			break;
			case 44100:
			default:
			OCR1A = (F_CPU / 44100);
			break;
			case 48000:
			OCR1A = (F_CPU / 48000);
		}
	}
	TCCR1B |= _BV(CS10);		//start timer, no prescaling
}

static void DAC_timer_stop(void) {
	TCCR1B &= ~(_BV(CS10));		//stop timer
	TCNT1 = 0;			//reset timer
}

void DAC_init(void) {
	sample_pos = 0;
	audio_rpos = 0;
	audio_wpos = 0;
	FLAG_CLEAR(BUFFER_READY);
	FLAG_SET(PLAYING);
	
	// set LOAD pin as output and go to idle state (high)
	DACDDR |= (1 << DACPIN_LOAD);
	DACLOAD(LEV_HIGH);
	// init USART in SPI mode
	UBRR0 = 0;
	/* Setting the XCK port pin as output, enables master mode. */
	DDRD |= (1<<PD4);
	/* Set MSPI mode of operation and SPI data mode 0. */
	UCSR0C = (1<<UMSEL01)|(1<<UMSEL00)|(0<<UCPHA0)|(0<<UCPOL0);
	/* Enable transmitter and transmit complete interrupt. */
	UCSR0B = _BV(TXEN0) | _BV(TXCIE0);
	/* Set baud rate. */
	/* IMPORTANT: The Baud Rate must be set after the transmitter is enabled */
	UBRR0 = 5;
	
	DAC_silence();

	// init audio sample timer
	TCCR1B = _BV(WGM12);			//CTC mode
	TCNT1 = 0;						//reset
	TIMSK1 = _BV(OCIE1A);			//enable timer1 output compare A interrupt
	// note: timer is not running yet
}

void DAC_shutdown(void) {
	stop_audio();
	// set LOAD tri-state
	DACDDR &= ~(1 << DACPIN_LOAD);
	DACLOAD(LEV_LOW);

	//disable usart spi
	UCSR0B = 0;
	UCSR0C = 0;
	//set XCK pin tri-state
	DDRD &= ~(_BV(PD4));
}

static void trigger_load(void) {
	DACLOAD(LEV_LOW);
	//_delay_us (0.15); // min LOAD pulse width (see LTC 1257 docs)
	DACLOAD(LEV_HIGH);
}

static void DAC_silence(void) {
	_delay_us(30);
	DAC_output(SILENCE);
	_delay_us(30);
	trigger_load();
}

ISR(TIMER1_COMPA_vect) {
	// this routine is executed at the sampling rate
	DAC_output(audio_buffer[audio_rpos++]);
}

static inline void DAC_output(uint16_t data) {
	trigger_load();
	
	low_byte = data;
	FLAG_SET(SENDING);
	UDR0 = data>>8;
}

ISR(USART_TX_vect) {
	// transmission completed
	if (FLAG_CHECK(SENDING)) {
		// send second byte
		FLAG_CLEAR(SENDING);
		UDR0 = low_byte;
	}
}

void volume_up (void) {
	if (volume < 3) volume++;
}
void volume_down (void) {
	if (volume > 0) volume--;
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
