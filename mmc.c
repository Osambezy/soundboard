/*-------------------------------------------------------------------------*/
/* PFF - Low level disk control module for AVR            (C)ChaN, 2010    */
/*-------------------------------------------------------------------------*/
/*     -- modified for GAP Sound Board --                                  */
/*-------------------------------------------------------------------------*/

#include "main.h"
#include "pin_config.h"
#include "dac.h"
#include "pff.h"
#include "diskio.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/*-------------------------------------------------------------------------*/
/* Platform dependent macros and functions needed to be modified           */
/*-------------------------------------------------------------------------*/

#define SELECT()	CSPORT &= ~_BV(CS)		/* CS = L */
#define	DESELECT()	CSPORT |=  _BV(CS)		/* CS = H */
#define	MMC_SEL		!(CSPORT &  _BV(CS))	/* CS status (true:CS == L) */

#define	FORWARD(d)	process_audio(d)


/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/

/* Definitions for MMC/SDC command */
#define CMD0	(0x40+0)	/* GO_IDLE_STATE */
#define CMD1	(0x40+1)	/* SEND_OP_COND (MMC) */
#define	ACMD41	(0xC0+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(0x40+8)	/* SEND_IF_COND */
#define CMD16	(0x40+16)	/* SET_BLOCKLEN */
#define CMD17	(0x40+17)	/* READ_SINGLE_BLOCK */
#define CMD24	(0x40+24)	/* WRITE_BLOCK */
#define CMD55	(0x40+55)	/* APP_CMD */
#define CMD58	(0x40+58)	/* READ_OCR */


/* Card type flags (CardType) */
#define CT_MMC				0x01	/* MMC ver 3 */
#define CT_SD1				0x02	/* SD ver 1 */
#define CT_SD2				0x04	/* SD ver 2 */
#define CT_BLOCK			0x08	/* Block addressing */


static
BYTE CardType;

static inline void spi_card_init(void) {
	//set CS pin as output
	CSDDR |= _BV(CS);
	
	// init SPI
	DDRB |= _BV(DDB3) | _BV(DDB5);
	SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR1) | _BV(SPR0);
}

void disk_shutdown(void) {
	// disable SPI
	SPCR = 0;
	SPSR = 0;
	DDRB &= ~(_BV(DDB3) | _BV(DDB5));
	
	//set CS pin tri-state
	CSDDR &= ~(_BV(CS));
}

static inline void spi_card_vollgas(void) {
	//SPCR &= ~(_BV(SPR1));
	SPCR &= ~(_BV(SPR1) | _BV(SPR0));
	SPSR = _BV(SPI2X);
}

static void spi_card_send(BYTE data) {
	// put data into buffer, sends the data
	SPDR = data;
	// wait for transmission to complete
	while(!(SPSR & (1<<SPIF)));
	// clear receive buffer
	SPDR;
}
static void spi_card_rcv_async(void) {
	// send dummy byte to start clock
	SPDR = 0xFF;
}
static BYTE spi_card_rcv_async_fetch(void) {
	// wait for data to be received
	while(!(SPSR & (1<<SPIF)));
	// get and return received data from buffer
	return SPDR;
}
static BYTE spi_card_rcv(void) {
	spi_card_rcv_async();
	return spi_card_rcv_async_fetch();
}

/*-----------------------------------------------------------------------*/
/* Send a command packet to MMC                                          */
/*-----------------------------------------------------------------------*/

static
BYTE send_cmd (
	BYTE cmd,		/* 1st byte (Start + Index) */
	DWORD arg		/* Argument (32 bits) */
)
{
	BYTE n, res;


	if (cmd & 0x80) {	/* ACMD<n> is the command sequense of CMD55-CMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) return res;
	}

	/* Select the card */
	DESELECT();
	spi_card_rcv();
	SELECT();
	spi_card_rcv();

	/* Send a command packet */
	spi_card_send(cmd);						/* Start + Command index */
	spi_card_send((BYTE)(arg >> 24));		/* Argument[31..24] */
	spi_card_send((BYTE)(arg >> 16));		/* Argument[23..16] */
	spi_card_send((BYTE)(arg >> 8));			/* Argument[15..8] */
	spi_card_send((BYTE)arg);				/* Argument[7..0] */
	n = 0x01;							/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;			/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
	spi_card_send(n);

	/* Receive a command response */
	n = 10;								/* Wait for a valid response in timeout of 10 attempts */
	do {
		res = spi_card_rcv();
	} while ((res & 0x80) && --n);

	return res;			/* Return with the response value */
}




/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (void)
{
	BYTE n, cmd, ty, ocr[4];
	UINT tmr;

#if _USE_WRITE
	if (CardType && MMC_SEL) disk_writep(0, 0);	/* Finalize write process if it is in progress */
#endif
	spi_card_init();		/* Initialize ports to control MMC */
	DESELECT();
	for (n = 10; n; n--) spi_card_rcv();	/* 80 dummy clocks with CS=H */
	SELECT();
	for (n = 2; n; n--) spi_card_rcv();

	ty = 0;
	if (send_cmd(CMD0, 0) == 0x01) {			/* Enter Idle state */
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDv2 */
			for (n = 0; n < 4; n++) ocr[n] = spi_card_rcv();		/* Get trailing return value of R7 resp */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {			/* The card can work at vdd range of 2.7-3.6V */
				for (tmr = 10000; tmr && send_cmd(ACMD41, 1UL << 30); tmr--) _delay_us(100);	/* Wait for leaving idle state (ACMD41 with HCS bit) */
				if (tmr && send_cmd(CMD58, 0) == 0) {		/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++) ocr[n] = spi_card_rcv();
					ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* SDv2 (HC or SC) */
				}
			}
		} else {							/* SDv1 or MMCv3 */
		    for (tmr = 2000; tmr && (send_cmd(ACMD41, 0)>1); tmr--) _delay_us(100);
			if (tmr) 	{
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 */
			}
			for (tmr = 1000; tmr && send_cmd(cmd, 0); tmr--) _delay_us(1000);	/* Wait for leaving idle state */
			if (!tmr || send_cmd(CMD16, 512) != 0)			/* Set R/W block length to 512 */
				ty = 0;
		}
	}
	CardType = ty;
	DESELECT();
	spi_card_rcv();
	spi_card_vollgas();

	return ty ? 0 : STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read partial sector                                                   */
/*-----------------------------------------------------------------------*/

DRESULT disk_readp (
	BYTE *buff,		/* Pointer to the read buffer (NULL:Read bytes are forwarded to the stream) */
	DWORD lba,		/* Sector number (LBA) */
	WORD ofs,		/* Byte offset to read from (0..511) */
	WORD cnt		/* Number of bytes to read (ofs + cnt mus be <= 512) */
)
{
	DRESULT res;
	BYTE rc;
	WORD bc;


	if (!(CardType & CT_BLOCK)) lba *= 512;		/* Convert to byte address if needed */

	res = RES_ERROR;
	if (send_cmd(CMD17, lba) == 0) {		/* READ_SINGLE_BLOCK */

		bc = 40000;
		do {							/* Wait for data packet */
			rc = spi_card_rcv();
		} while (rc == 0xFF && --bc);

		if (rc == 0xFE) {				/* A data packet arrived */
			bc = 514 - ofs - cnt;

			/* Skip leading bytes */
			if (ofs) {
				do spi_card_rcv(); while (--ofs);
			}

			/* Receive a part of the sector */
			if (buff) {	/* Store data to the memory */
				do {
					*buff++ = spi_card_rcv();
				} while (--cnt);
			} else {	/* Forward data to the outgoing stream (depends on the project) */
				spi_card_rcv_async();
				BYTE received;
				do {
					received = spi_card_rcv_async_fetch();
					cnt--;
					if (cnt) spi_card_rcv_async();
					FORWARD(received);
				} while (cnt);
				/*do {
					FORWARD(spi_card_rcv());
				} while (--cnt);*/
			}

			/* Skip trailing bytes and CRC */
			do spi_card_rcv(); while (--bc);

			res = RES_OK;
		}
	}

	DESELECT();
	spi_card_rcv();

	return res;
}



/*-----------------------------------------------------------------------*/
/* Write partial sector                                                  */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_writep (
	const BYTE *buff,	/* Pointer to the bytes to be written (NULL:Initiate/Finalize sector write) */
	DWORD sa			/* Number of bytes to send, Sector number (LBA) or zero */
)
{
	DRESULT res;
	WORD bc;
	static WORD wc;

	res = RES_ERROR;

	if (buff) {		/* Send data bytes */
		bc = (WORD)sa;
		while (bc && wc) {		/* Send data bytes to the card */
			spi_card_send(*buff++);
			wc--; bc--;
		}
		res = RES_OK;
	} else {
		if (sa) {	/* Initiate sector write process */
			if (!(CardType & CT_BLOCK)) sa *= 512;	/* Convert to byte address if needed */
			if (send_cmd(CMD24, sa) == 0) {			/* WRITE_SINGLE_BLOCK */
				spi_card_send(0xFF); spi_card_send(0xFE);		/* Data block header */
				wc = 512;							/* Set byte counter */
				res = RES_OK;
			}
		} else {	/* Finalize sector write process */
			bc = wc + 2;
			while (bc--) spi_card_send(0);	/* Fill left bytes and CRC with zeros */
			if ((spi_card_rcv() & 0x1F) == 0x05) {	/* Receive data resp and wait for end of write process in timeout of 500ms */
				for (bc = 5000; spi_card_rcv() != 0xFF && bc; bc--) _delay_us(100);	/* Wait ready */
				if (bc) res = RES_OK;
			}
			DESELECT();
			spi_card_rcv();
		}
	}

	return res;
}
#endif
