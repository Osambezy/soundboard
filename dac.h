#ifndef _DAC_H_
#define _DAC_H_

#include <stdint.h>

void DAC_init(void);
void DAC_shutdown(void);
void process_audio(uint8_t data_byte);
void stop_audio(void);
void cancel_play(void);

void volume_up (void);
void volume_down (void);

void sound_gut(void);
void sound_osch(void);

#endif
