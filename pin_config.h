#ifndef _PINCONFIG_
#define _PINCONFIG_

// key matrix rows
#define BMASK (_BV(0))
#define DMASK (_BV(2) | _BV(3) | _BV(5) | _BV(6) | _BV(7))
#define row0	_BV(0)
#define row1	_BV(7)
#define row2	_BV(6)
#define row3	_BV(5)
#define row4	_BV(2)
#define row5	_BV(3)

// key matrix columns
#define CMASK (_BV(0) | _BV(1) | _BV(2) | _BV(3) | _BV(4) | _BV(5)) 
#define col0	_BV(0)
#define col1	_BV(1)
#define col2	_BV(2)
#define col3	_BV(3)
#define col4	_BV(4)
#define col5	_BV(5)

#define MOSFETDDR		DDRD
#define	MOSFETPORT		PORTD
#define MOSFET			0

#define CSPORT		PORTB
#define CSDDR		DDRB
#define CS			2

#define DACPORT		PORTB
#define DACDDR		DDRB
#define DACPIN_LOAD	1

#define DACLOAD(LEVEL)	DACPORT = (LEVEL) ? (DACPORT | _BV(DACPIN_LOAD)):(DACPORT & ~_BV(DACPIN_LOAD))
 
#define LEV_LOW		0
#define LEV_HIGH	1


#endif
