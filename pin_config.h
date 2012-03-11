#ifndef _PINCONFIG_
#define _PINCONFIG_

// key matrix rows
#define BMASK (_BV(PB0))
#define DMASK (_BV(PD2) | _BV(PD3) | _BV(PD5) | _BV(PD6) | _BV(PD7))
// key matrix columns
#define CMASK (_BV(PC1) | _BV(PC2) | _BV(PC3) | _BV(PC4) | _BV(PC5)) 

#define MOSFETDDR		DDRB
#define	MOSFETPORT		PORTB
#define MOSFET			PB2

#define CSPORT		PORTC
#define CSDDR		DDRC
#define CS			PC0

#define DACPORT		PORTB
#define DACDDR		DDRB
#define DACPIN_LOAD	PB1

#define DACLOAD(LEVEL)	DACPORT = (LEVEL) ? (DACPORT | _BV(DACPIN_LOAD)):(DACPORT & ~_BV(DACPIN_LOAD))
 
#define LEV_LOW		0
#define LEV_HIGH	1


#endif
