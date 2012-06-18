#ifndef _MAIN_H_
#define _MAIN_H_

#define F_CPU 20.000000E6

// time since last keypress, when device goes into hibernate mode
#define HIBER_TIME		15		// seconds (max. 858)

// time between key change interrupt and reading of keys
#define DEBOUNCE_TIME	20		// milliseconds (max. 835)

// delay for switching row/column mode (matrix multiplexing)
// should be increased to handle higher capacitances of the key matrix
#define SWITCH_TIME		10		// microseconds

#define CREDITS_COUNTER_MAX 500

#define HIBER_COUNT_MAX ((uint16_t) (HIBER_TIME * F_CPU / 256 / 1024))
#define DEBOUNCE_COUNT_MAX ((uint8_t) (DEBOUNCE_TIME * F_CPU / 256 / 256 / 1000))

#define FLAG_SET(x) (GPIOR0 |= (x))
#define FLAG_CLEAR(x) (GPIOR0 &= (~x))
#define FLAG_CHECK(x) (GPIOR0 & (x))

#define SENDING 0x01
#define PLAYING 0x02
#define BUFFER_READY 0x04
#define NEW_SOUND 0x08
#define HIBERNATING 0x10
#define WAKING_UP 0x20

#endif /* _MAIN_H_ */
