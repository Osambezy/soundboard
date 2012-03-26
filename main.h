#ifndef _MAIN_H_
#define _MAIN_H_

#define F_CPU 16.000000E6

// time since last keypress, when device goes into hibernate mode
#define HIBER_TIME		10		// seconds

// time between key change interrupt and reading of keys
#define DEBOUNCE_TIME	50		// milliseconds

// delay for switching row/column mode (matrix multiplexing)
// should be increased to handle higher capacitances of the key matrix
#define SWITCH_TIME		10		// microseconds

#define HIBER_COUNT_MAX ((uint32_t) (HIBER_TIME * F_CPU / 256 / 1024))
#define DEBOUNCE_COUNT_MAX ((uint8_t) (DEBOUNCE_TIME * F_CPU / 256 / 256 / 1000))

#endif /* _MAIN_H_ */
