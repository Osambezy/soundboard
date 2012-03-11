/*
 * Low level access routines to the LTC1257 DAC device.
 */

#ifndef _DAC_H_
#define _DAC_H_

#include <inttypes.h>

void DAC_init(void);
void DAC_shutdown(void);
void process_audio(uint8_t data_byte);
void stop_audio(void);

#endif
