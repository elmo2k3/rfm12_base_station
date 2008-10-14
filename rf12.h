
extern unsigned short rf12_trans(unsigned short wert);					// transfer 1 word to/from module
extern void rf12_init(void);											// initialize module
extern void rf12_setfreq(unsigned short freq);							// set center frequency
extern void rf12_setbaud(unsigned short baud);							// set baudrate
extern void rf12_setpower(unsigned char power, unsigned char mod);		// set transmission settings
extern void rf12_setbandwidth(unsigned char bandwidth, unsigned char gain, unsigned char drssi);	// set receiver settings
extern void rf12_config(unsigned short baudrate, unsigned char channel, unsigned char power, unsigned char environment);	// config module

extern unsigned char rf12_data(void);									// data in receive buffer ?
extern unsigned char rf12_getchar(void);								// get one byte from receive buffer
extern void rf12_putc(unsigned char datum);								// put one byte into transmit buffer
extern unsigned char rf12_busy(void);									// transmit buffer full ?

extern void rf12_rxmode(void);
extern void rf12_stoprx(void);

unsigned extern volatile char flags;

extern volatile unsigned char rf12_RxHead;

extern void tx_packet(unsigned char retrans);

extern  unsigned char tx_status;

#define RF12FREQ(freq)	((unsigned short)((freq-430.0)/0.0025))			// macro for calculating frequency value out of frequency in MHz

#define QUIET		1
#define NORMAL		2
#define NOISY		3

#define RECEIVED_OK			1		// Daten erfolgreich empfangen
#define RECEIVED_FAIL		2		// Daten fehlerhaft empfangen -> bitte nochmal senden
#define DATAINBUFFER		4		// Empfänger möchte Daten senden

extern volatile unsigned char delaycnt;
extern void rf12_txdata(unsigned char *data, unsigned char number, unsigned char status, unsigned char id, unsigned char toAddress);

