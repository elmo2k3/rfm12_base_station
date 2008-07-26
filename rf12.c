#include "main.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/crc16.h>
#include <portbits.h>
#include "global.h"
#include "rf12.h"


#include <util/delay.h>


#define ANSWER_TIMEOUT	10			// Maximale Wartezeit auf die Bestätigung der Daten in ms (max 500)
#define RETRY			1			// Maximale Anzahl an Sendeversuchen
#define MAX_BUF			128			// Maximale Paketgröße
#define RX_BUF			128			// Empfangs FIFO
#define TX_TIMEOUT		2			// Maximale Wartezeit auf Daten in ms (max 500)

//#define LED_TX		PORTC_1
//#define LED_RX		PORTC_3
//#define LED_ERR		PORTC_0


#define RF_PORT	PORTB
#define RF_DDR	DDRB
#define RF_PIN	PINB

#define SDI		3
#define SCK		5
#define CS		2
#define SDO		4
//		FFIT	PD3

#define WAITFORACK		1

volatile unsigned char delaycnt;
unsigned char txbuf[MAX_BUF],rxbuf[RX_BUF];
volatile unsigned char rf12_RxHead;
volatile unsigned char rf12_RxTail;
unsigned char tx_cnt, tx_id, tx_status, retrans_cnt;
unsigned volatile char flags;

void tx_packet(unsigned char retrans);

unsigned short rf12_trans(unsigned short wert)
{	CONVERTW val;
	val.w=wert;
	cbi(RF_PORT, CS);
	SPDR = val.b[1];
	while(!(SPSR & (1<<SPIF)));
	val.b[1]=SPDR;
	SPDR = val.b[0];
	while(!(SPSR & (1<<SPIF)));
	val.b[0]=SPDR;
	sbi(RF_PORT, CS);
	return val.w;
}

void rf12_init(void)
{
	RF_PORT=(1<<CS);
	RF_DDR&=~(1<<SDO);
	RF_DDR|=(1<<SDI)|(1<<SCK)|(1<<CS);
	DDRD&=~4;
	PORTD|=4;
#if F_CPU > 10000000
	SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0);
	SPSR = (1<<SPI2X);
#else
	SPCR=(1<<SPE)|(1<<MSTR);
#endif
	TCCR1A=0;
	TCCR1B=(1<<WGM12)|1;
	OCR1A=((F_CPU+2500)/500)-1;
	TIMSK|=(1<<OCIE1A);

	for (unsigned char i=0; i<20; i++)
		_delay_ms(10);					// wait until POR done
	rf12_trans(0xC0E0);					// AVR CLK: 10MHz
	rf12_trans(0x80D7);					// Enable FIFO
	rf12_trans(0xC2AB);					// Data Filter: internal
	rf12_trans(0xCA81);					// Set FIFO mode
	rf12_trans(0xE000);					// disable wakeuptimer
	rf12_trans(0xC800);					// disable low duty cycle
	rf12_trans(0xC4F7);					// AFC settings: autotuning: -10kHz...+7,5kHz

	rf12_RxHead=0;
	rf12_RxTail=0;

	rf12_rxmode();
}

void rf12_config(unsigned short baudrate, unsigned char channel, unsigned char power, unsigned char environment)
{
	rf12_setfreq(RF12FREQ(433.4)+13*channel); // Sende/Empfangsfrequenz auf 433,4MHz + channel * 325kHz einstellen
   	rf12_setpower(0, 5);					// 6mW Ausgangangsleistung, 90kHz Frequenzshift
   	rf12_setbandwidth(4, environment, 0);	// 200kHz Bandbreite, Verstärkung je nach Umgebungsbedingungen, DRSSI threshold: -103dBm (-environment*6dB)
	rf12_setbaud(baudrate);					// Baudrate
}

void rf12_rxmode(void)
{
	rf12_trans(0x82C8);					// RX on
	rf12_trans(0xCA81);					// set FIFO mode
	_delay_ms(.8);
	rf12_trans(0xCA83);					// enable FIFO: sync word search
	rf12_trans(0);
	//MCUCR |= (1<<ISC01)|(1<<ISC00);
	GIFR=(1<<INTF0);
	GICR|=(1<<INT0);					// Low Level Interrupt für FFIT (über Inverter angeschlossen !)
}

void rf12_stoprx(void)
{
	GICR&=~(1<<INT0);					// Interrupt aus
	rf12_trans(0x8208);					// RX off
	_delay_ms(1);
}

void rf12_setbandwidth(unsigned char bandwidth, unsigned char gain, unsigned char drssi)
{
	rf12_trans(0x9500|((bandwidth&7)<<5)|((gain&3)<<3)|(drssi&7));
}

void rf12_setfreq(unsigned short freq)
{	if (freq<96)						// 430,2400MHz
		freq=96;
	else if (freq>3903)					// 439,7575MHz
		freq=3903;
	rf12_trans(0xA000|freq);
}

void rf12_setbaud(unsigned short baud)
{
	if (baud<664)
		baud=664;
	if (baud<5400)						// Baudrate= 344827,58621/(R+1)/(1+CS*7)
		rf12_trans(0xC680|((43104/baud)-1));	// R=(344828/8)/Baud-1
	else
		rf12_trans(0xC600|((344828UL/baud)-1));	// R=344828/Baud-1
}

void rf12_setpower(unsigned char power, unsigned char mod)
{	
	rf12_trans(0x9800|(power&7)|((mod&15)<<4));
}

static inline void rf12_ready(void)
{
	cbi(RF_PORT, CS);
	asm("nop");
	asm("nop");
	while (!(RF_PIN&(1<<SDO)));			// wait until FIFO ready
}

void rf12_txbyte(unsigned char val)
{
	rf12_ready();
	rf12_trans(0xB800|val);
	if ((val==0x00)||(val==0xFF))		// Stuffbyte einfügen um ausreichend Pegelwechsel zu haben
	{	rf12_ready();
		rf12_trans(0xB8AA);
	}
}

unsigned char rf12_rxbyte(void)
{	unsigned char val;
	val =rf12_trans(0xB000);
	if ((val==0x00)||(val==0xFF))		// Stuffbyte wieder entfernen
	{	rf12_ready();
		rf12_trans(0xB000);
	}
	return val;
}
#ifdef PROTOKOLL_V2
void rf12_txdata(unsigned char *data, unsigned char number, unsigned char status, unsigned char id, unsigned char toAddress)
#else
void rf12_txdata(unsigned char *data, unsigned char number, unsigned char status, unsigned char id)
#endif
{	unsigned char i, crc;
	//LED_TX=1;
	rf12_trans(0x8238);					// TX on
	rf12_ready();
	rf12_trans(0xB8AA);					// Sync Data
	rf12_ready();
	rf12_trans(0xB8AA);
	rf12_ready();
	rf12_trans(0xB8AA);
	rf12_ready();
	rf12_trans(0xB82D);
	rf12_ready();
	rf12_trans(0xB8D4);
	#ifdef PROTOKOLL_V2
	crc=_crc_ibutton_update (0, toAddress);
	rf12_txbyte(toAddress);				// Empfänger
	crc=_crc_ibutton_update (crc, MY_ADDRESS);
	rf12_txbyte(MY_ADDRESS);				// Sender
	crc=_crc_ibutton_update (crc, status);
	rf12_txbyte(status);				// Status
	#else
	crc=_crc_ibutton_update (0, status);
	rf12_txbyte(status);				// Status
	#endif
	crc=_crc_ibutton_update (crc, number);
	rf12_txbyte(number);				// Anzahl der zu sendenden Bytes übertragen
	if (number)							// nur Status ? Dann keine Daten senden
	{	crc=_crc_ibutton_update (crc, id);
		rf12_txbyte(id);				// Paket ID
		for (i=0; i<number; i++)
		{	rf12_txbyte(*data);
			crc=_crc_ibutton_update (crc, *data);
			data++;
		}
	}
	rf12_txbyte(crc);					// Checksumme hinterher senden
	rf12_txbyte(0);						// dummy data
	rf12_trans(0x8208);					// TX off
	//LED_TX=0;
}


ISR(SIG_INTERRUPT0)
{	
	static unsigned char bytecnt, status, number, id, crc,rf_data[MAX_BUF];
	static unsigned char rx_lastid=255, toAddress, fromAddress;
#ifdef PROTOKOLL_V2
	if (bytecnt==0)
	{	//LED_RX=1;
		toAddress=rf12_rxbyte();			// Empfängeradresse
		crc=_crc_ibutton_update (0, toAddress);
		bytecnt=1;
	}
	else if (bytecnt==1)
	{	
		fromAddress=rf12_rxbyte();			// Empfängeradresse
		crc=_crc_ibutton_update (crc, fromAddress);
		bytecnt=2;
	}
	else if (bytecnt==2)
	{	
		status=rf12_rxbyte();			// Status
		crc=_crc_ibutton_update (crc, status);
		bytecnt=3;
	}
	else if (bytecnt==3)
	{	number=rf12_rxbyte();			// Anzahl der zu empfangenden Bytes
		crc=_crc_ibutton_update (crc, number);
		if (number>MAX_BUF)
			number=MAX_BUF;
		if (number)
			bytecnt=4;
		else
			bytecnt=255;
	}
	else if (bytecnt==4)
	{	id=rf12_rxbyte();				// Paketnummer
		crc=_crc_ibutton_update (crc, id);
		bytecnt=5;
	}
	else if (bytecnt<255)
	{	rf_data[bytecnt-5]=rf12_rxbyte();
		crc=_crc_ibutton_update (crc, rf_data[bytecnt-5]);
		bytecnt++;
		if ((bytecnt-5)>=number)		// alle Bytes empfangen ?
			bytecnt=255;
	}
	else
	{	unsigned char crcref;
		crcref=rf12_rxbyte();			// CRC empfangen
		rf12_trans(0xCA81);				// restart syncword detection:
		rf12_trans(0xCA83);				// enable FIFO
		//LED_RX=0;
		if (crcref==crc && toAddress==MY_ADDRESS)				// Wenn CRC OK, Daten übernehmen
		{	if (status&RECEIVED_OK)		// Empfangsbestätigung ?
			{	flags&=~WAITFORACK;		// -> "Warten auf Bestätigung"-Flag löschen
				tx_cnt=0;				// -> Daten als gesendet markieren
				tx_id++;
				//LED_ERR=0;
			}
			if (number)
			{	
				destination=fromAddress;
				tx_status=RECEIVED_OK;			// zu sendender Status
				tx_packet(0);					// Empfangsbestätigung senden
				retrans_cnt=RETRY;				// Retry Counter neu starten
				if (id!=rx_lastid)				// Handelt es sich um neue Daten ?
				{	rx_lastid=id;				// Aktuelle ID speichern
					unsigned char i, tmphead;
					tmphead = rf12_RxHead;
					for(i=0; i<number; i++)	// Komplettes Paket in den Empfangspuffer kopieren
					{	tmphead++;
		    			if (tmphead>=RX_BUF)
		    				tmphead=0;
		    			if (tmphead == rf12_RxTail)	// receive buffer overflow !!!
		    			{	break;				// restlichen Daten ignorieren
		    			}
		    			else
		    			{	rxbuf[tmphead] = rf_data[i]; // Daten in Empfangspuffer kopieren
		    			}	
					}
					rf12_RxHead = tmphead;
				}
			}
		}
		bytecnt=0;
	}
#else
	if (bytecnt==0)
	{	//LED_RX=1;
		status=rf12_rxbyte();			// Status
		crc=_crc_ibutton_update (0, status);
		bytecnt=1;
	}
	else if (bytecnt==1)
	{	number=rf12_rxbyte();			// Anzahl der zu empfangenden Bytes
		crc=_crc_ibutton_update (crc, number);
		if (number>MAX_BUF)
			number=MAX_BUF;
		if (number)
			bytecnt=2;
		else
			bytecnt=255;
	}
	else if (bytecnt==2)
	{	id=rf12_rxbyte();				// Paketnummer
		crc=_crc_ibutton_update (crc, id);
		bytecnt=3;
	}
	else if (bytecnt<255)
	{	rf_data[bytecnt-3]=rf12_rxbyte();
		crc=_crc_ibutton_update (crc, rf_data[bytecnt-3]);
		bytecnt++;
		if ((bytecnt-3)>=number)		// alle Bytes empfangen ?
			bytecnt=255;
	}
	else
	{	unsigned char crcref;
		crcref=rf12_rxbyte();			// CRC empfangen
		rf12_trans(0xCA81);				// restart syncword detection:
		rf12_trans(0xCA83);				// enable FIFO
		//LED_RX=0;
		if (crcref==crc)				// Wenn CRC OK, Daten übernehmen
		{	if (status&RECEIVED_OK)		// Empfangsbestätigung ?
			{	flags&=~WAITFORACK;		// -> "Warten auf Bestätigung"-Flag löschen
				tx_cnt=0;				// -> Daten als gesendet markieren
				tx_id++;
				//LED_ERR=0;
			}
			if (number)
			{	tx_status=RECEIVED_OK;			// zu sendender Status
				tx_packet(0);					// Empfangsbestätigung senden
				retrans_cnt=RETRY;				// Retry Counter neu starten
				if (id!=rx_lastid)				// Handelt es sich um neue Daten ?
				{	rx_lastid=id;				// Aktuelle ID speichern
					unsigned char i, tmphead;
					tmphead = rf12_RxHead;
					for(i=0; i<number; i++)	// Komplettes Paket in den Empfangspuffer kopieren
					{	tmphead++;
		    			if (tmphead>=RX_BUF)
		    				tmphead=0;
		    			if (tmphead == rf12_RxTail)	// receive buffer overflow !!!
		    			{	break;				// restlichen Daten ignorieren
		    			}
		    			else
		    			{	rxbuf[tmphead] = rf_data[i]; // Daten in Empfangspuffer kopieren
		    			}	
					}
					rf12_RxHead = tmphead;
				}
			}
		}
		bytecnt=0;
	}
#endif
}

ISR(SIG_OUTPUT_COMPARE1A)							// 500Hz Interrupt
{	
	if ((tx_cnt)||(flags&WAITFORACK))				// Nur zählen wenn gebraucht
	{	if (!--delaycnt)
    	{	if (flags&WAITFORACK)					// Timeout beim Warten auf Empfangsbestätigung
			{	if (retrans_cnt)
	    		{	retrans_cnt--;
	              	tx_packet(1);					// retransmit
	    		}
	    		else								// Versuche abgelaufen
	    		{	//LED_ERR=1;						// -> Fehler LED an
	    			tx_cnt=0;						// -> Daten verwerfen
	    			tx_id++;
	    			flags&=~WAITFORACK;				// Daten als OK markieren
				}
    		}
			else if (tx_cnt)
        	{	tx_status=0;					// zu sendender Status
        		tx_packet(0);					// erstmaliger Transfer
        	}
    	}
	}
}

void tx_packet(unsigned char retrans)
{
	rf12_stoprx();									// auf TX umschalten
	if ((!retrans)&&((flags&WAITFORACK)||(tx_cnt==0))) // es wird noch auf eine Antwort vom Paket gewartet oder es sind keine Daten zu senden
	{   	
#ifdef PROTOKOLL_V2 
		rf12_txdata(txbuf, 0, tx_status, 0, destination);		// -> kein komplettes neues Paket, sondern nur Status senden
		#else
		rf12_txdata(txbuf, 0, tx_status, 0);
#endif
	}
	else
	{	
#ifdef PROTOKOLL_V2
		rf12_txdata(txbuf, tx_cnt, tx_status, tx_id, destination); // komplettes Paket senden
#else
		rf12_txdata(txbuf, tx_cnt, tx_status, tx_id);
#endif
		flags|=WAITFORACK;							// auf ACK warten
		delaycnt=ANSWER_TIMEOUT/2;					// Timeout Counter neu starten
		if (!retrans)								// erstmalige Übertragung ?
			retrans_cnt=RETRY;						// -> Retry Counter neu starten
	}
	rf12_rxmode();									// wieder auf RX umschalten
}

unsigned char rf12_data(void)
{
	if(rf12_RxHead == rf12_RxTail)
		return 0;
	else
		return 1;
}

unsigned char rf12_getchar(void)
{    unsigned short tmptail;
    while (rf12_RxHead == rf12_RxTail); 
    tmptail = (rf12_RxTail + 1);     // calculate buffer index
	if (tmptail>=RX_BUF)
		tmptail=0;
	rf12_RxTail = tmptail; 
    return rxbuf[tmptail];		// get data from receive buffer
}

unsigned char rf12_busy(void)
{
	if(flags&WAITFORACK)
		return 1;
	else
		return 0;
}

void rf12_putc(unsigned char datum)
{
	while(flags&WAITFORACK);			// Puffer erst dann füllen, wenn die alten Daten aus dem Puffer wirklich angekommen sind
	if (!tx_cnt)
		delaycnt=TX_TIMEOUT/2;			// Nach dem ersten Byte: timeout starten
	txbuf[tx_cnt++]=datum;
	if (tx_cnt>=MAX_BUF) 				// Puffer voll -> senden
	{	tx_status=0;					// zu sendender Status
		tx_packet(0);					// erstmaliger Transfer
	}
}

