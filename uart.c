/*************************************************************************
Title:    Interrupt UART library with receive/transmit circular buffers
Author:   Peter Fleury <pfleury@gmx.ch>   http://jump.to/fleury
File:     $Id: uart.c,v 1.5.2.6 2005/02/16 19:04:46 Peter Exp $
Software: AVR-GCC 3.3 
Hardware: any AVR with built-in UART, 
          tested on AT90S8515 at 4 Mhz and ATmega at 1Mhz

DESCRIPTION:
    An interrupt is generated when the UART has finished transmitting or
    receiving a byte. The interrupt handling routines use circular buffers
    for buffering received and transmitted data.
    
    The UART_RX_BUFFER_SIZE and UART_TX_BUFFER_SIZE variables define
    the buffer size in bytes. Note that these variables must be a 
    power of 2.
    
USAGE:
    Refere to the header file uart.h for a description of the routines. 
    See also example test_uart.c.

NOTES:
    Based on Atmel Application Note AVR306
                    
*************************************************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <portbits.h>
#include "uart.h"


/*
 *  constants and macros
 */

/* size of RX/TX buffers */
#define UART1_RX_BUFFER_MASK ( UART1_RX_BUFFER_SIZE - 1)
#define UART1_TX_BUFFER_MASK ( UART1_TX_BUFFER_SIZE - 1)

#if ( UART1_RX_BUFFER_SIZE & UART1_RX_BUFFER_MASK )
#error RX1 buffer size is not a power of 2
#endif
#if ( UART1_TX_BUFFER_SIZE & UART1_TX_BUFFER_MASK )
#error TX1 buffer size is not a power of 2
#endif

#if defined(__AVR_AT90S2313__) \
 || defined(__AVR_AT90S4414__) || defined(__AVR_AT90S4434__) \
 || defined(__AVR_AT90S8515__) || defined(__AVR_AT90S8535__) \
 || defined(__AVR_ATmega103__)
 /* old AVR classic or ATmega103 with one UART */
 #define AT90_UART
 #define UART0_RECEIVE_INTERRUPT   SIG_UART_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_UART_DATA
 #define UART0_STATUS   USR
 #define UART0_CONTROL  UCR
 #define UART0_DATA     UDR  
 #define UART0_UDRIE    UDRIE
#elif defined(__AVR_AT90S2333__) || defined(__AVR_AT90S4433__)
 /* old AVR classic with one UART */
 #define AT90_UART
 #define UART0_RECEIVE_INTERRUPT   SIG_UART_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_UART_DATA
 #define UART0_STATUS   UCSRA
 #define UART0_CONTROL  UCSRB
 #define UART0_DATA     UDR 
 #define UART0_UDRIE    UDRIE

#elif defined(__AVR_ATtiny2313__) 
 #define ATMEGA_USART
 #define UART0_RECEIVE_INTERRUPT   SIG_USART0_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_USART0_DATA
 #define UART0_STATUS   UCSRA
 #define UART0_CONTROL  UCSRB
 #define UART0_DATA     UDR
 #define UART0_UDRIE    UDRIE

#elif defined(__AVR_AT90PWM3__) 
 #define ATMEGA_USART
 #define UART0_RECEIVE_INTERRUPT	SIG_UART_RECV
 #define UART0_TRANSMIT_INTERRUPT	SIG_USART_DATA
 #define UART0_STATUS   UCSRA
 #define UART0_CONTROL  UCSRB
 #define UART0_DATA     UDR
 #define UART0_UDRIE    UDRIE

#elif defined(__AVR_ATmega48__) || defined(__AVR_ATmega88__) || defined(__AVR_ATmega168__) 
 #define ATMEGA_USART0
 #define RXCIE 			RXCIE0
 #define TXCIE 			TXCIE0
 #define RXEN 			RXEN0
 #define TXEN 			TXEN0
 #define UART0_RECEIVE_INTERRUPT	SIG_USART_RECV
 #define UART0_TRANSMIT_INTERRUPT	SIG_USART_DATA
 #define UART0_STATUS   UCSR0A
 #define UART0_CONTROL  UCSR0B
 #define UART0_DATA     UDR0
 #define UART0_UDRIE    UDRIE0


#elif  defined(__AVR_ATmega8__)  || defined(__AVR_ATmega16__) || defined(__AVR_ATmega32__) \
  || defined(__AVR_ATmega8515__) || defined(__AVR_ATmega8535__) \
  || defined(__AVR_ATmega323__) 
  /* ATmega with one USART */
 #define ATMEGA_USART
 #define UART0_RECEIVE_INTERRUPT   SIG_UART_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_UART_DATA
 #define UART0_STATUS   UCSRA
 #define UART0_CONTROL  UCSRB
 #define UART0_DATA     UDR
 #define UART0_UDRIE    UDRIE
#elif defined(__AVR_ATmega163__) 
  /* ATmega163 with one UART */
 #define ATMEGA_UART
 #define UART0_RECEIVE_INTERRUPT   SIG_UART_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_UART_DATA
 #define UART0_STATUS   UCSRA
 #define UART0_CONTROL  UCSRB
 #define UART0_DATA     UDR
 #define UART0_UDRIE    UDRIE
#elif defined(__AVR_ATmega162__)
 /* ATmega with two USART */
 #define ATMEGA_USART0
 #define ATMEGA_USART1
 #define UART0_RECEIVE_INTERRUPT   SIG_USART0_RECV
 #define UART1_RECEIVE_INTERRUPT   SIG_USART1_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_USART0_DATA
 #define UART1_TRANSMIT_INTERRUPT  SIG_USART1_DATA
 #define UART0_STATUS   UCSR0A
 #define UART0_CONTROL  UCSR0B
 #define UART0_DATA     UDR0
 #define UART0_UDRIE    UDRIE0
 #define UART1_STATUS   UCSR1A
 #define UART1_CONTROL  UCSR1B
 #define UART1_DATA     UDR1
 #define UART1_UDRIE    UDRIE1
#elif defined(__AVR_ATmega64__) || defined(__AVR_ATmega128__)
 /* ATmega with two USART */
 #define ATMEGA_USART0
 #define ATMEGA_USART1
 #define UART0_RECEIVE_INTERRUPT   SIG_UART0_RECV
 #define UART1_RECEIVE_INTERRUPT   SIG_UART1_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_UART0_DATA
 #define UART1_TRANSMIT_INTERRUPT  SIG_UART1_DATA
 #define UART0_STATUS   UCSR0A
 #define UART0_CONTROL  UCSR0B
 #define UART0_DATA     UDR0
 #define UART0_UDRIE    UDRIE0
 #define UART1_STATUS   UCSR1A
 #define UART1_CONTROL  UCSR1B
 #define UART1_DATA     UDR1
 #define UART1_UDRIE    UDRIE1
#elif defined(__AVR_ATmega164P__) || defined(__AVR_ATmega324P__)
 /* ATmega with two USART */
 #define RXCIE 			RXCIE0
 #define TXCIE 			TXCIE0
 #define RXEN 			RXEN0
 #define TXEN 			TXEN0
 #define ATMEGA_USART0
 #define ATMEGA_USART1
 #define UART0_RECEIVE_INTERRUPT   SIG_USART_RECV
 #define UART1_RECEIVE_INTERRUPT   SIG_USART1_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_USART_DATA
 #define UART1_TRANSMIT_INTERRUPT  SIG_USART1_DATA
 #define UART0_STATUS   UCSR0A
 #define UART0_CONTROL  UCSR0B
 #define UART0_DATA     UDR0
 #define UART0_UDRIE    UDRIE0
 #define UART1_STATUS   UCSR1A
 #define UART1_CONTROL  UCSR1B
 #define UART1_DATA     UDR1
 #define UART1_UDRIE    UDRIE1
#elif defined(__AVR_ATmega161__)
 /* ATmega with UART */
 #error "AVR ATmega161 currently not supported by this libaray !"
#elif defined(__AVR_ATmega169__)
 /* ATmega with one USART */
 #define ATMEGA_USART0
 #define UART0_RECEIVE_INTERRUPT   SIG_UART0_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_UART0_DATA
 #define UART0_STATUS   UCSR0A
 #define UART0_CONTROL  UCSR0B
 #define UART0_DATA     UDR0
 #define UART0_UDRIE    UDRIE0
#endif


#ifndef NO_UART_INT
/*
 *  module global variables
 */
#ifdef ENABLE_RX
static volatile unsigned char UART_RxBuf[UART_RX_BUFFER_SIZE];

#if (UART_RX_BUFFER_SIZE<=256)
	static volatile unsigned char UART_RxHead;
	static volatile unsigned char UART_RxTail;
#else
	static volatile unsigned short UART_RxHead;
	static volatile unsigned short UART_RxTail;
#endif
#ifndef SIMPLE_UART
static volatile unsigned char UART_LastRxError;
#endif

#ifdef RX_COUNT
#if (UART_RX_BUFFER_SIZE<256)
	static volatile unsigned char UART_RxCnt;
#else
	static volatile unsigned short UART_RxCnt;
#endif
#endif
#endif

#if defined (ENABLE_TX) && !defined(DISABLE_TXBUF)
static volatile unsigned char UART_TxBuf[UART_TX_BUFFER_SIZE];
static volatile unsigned char UART_TxHead;
static volatile unsigned char UART_TxTail;
#endif

#if defined( ATMEGA_USART1 )
static volatile unsigned char UART1_TxBuf[UART1_TX_BUFFER_SIZE];
static volatile unsigned char UART1_RxBuf[UART1_RX_BUFFER_SIZE];
static volatile unsigned char UART1_TxHead;
static volatile unsigned char UART1_TxTail;
static volatile unsigned char UART1_RxHead;
static volatile unsigned char UART1_RxTail;
static volatile unsigned char UART1_LastRxError;
#endif


#ifdef ENABLE_RX
SIGNAL(UART0_RECEIVE_INTERRUPT)
/*************************************************************************
Function: UART Receive Complete interrupt
Purpose:  called when the UART has received a character
**************************************************************************/
{
#if (UART_RX_BUFFER_SIZE<=256)
    unsigned char tmphead;
#else
    unsigned short tmphead;
#endif
    unsigned char data;

	/* read UART status register and UART data register */ 
	data = UART0_DATA;

    /* calculate buffer index */ 
    tmphead = ( UART_RxHead + 1);
	if (tmphead>=UART_RX_BUFFER_SIZE)
		tmphead=0;

	if ( tmphead == UART_RxTail )
	{	    /* error: receive buffer overflow */
#ifndef SIMPLE_UART
         UART_LastRxError = UART_BUFFER_OVERFLOW >> 8;
#endif
    }
	else
	{   /* store new index */
		UART_RxHead = tmphead;
        /* store received data in buffer */
		UART_RxBuf[tmphead] = data;
#ifdef RX_COUNT
		UART_RxCnt++;
#ifdef USE_CTS
		if (UART_RxCnt>((UART_RX_BUFFER_SIZE)/2))
			CTS=1;
#endif
#endif
    } 
}
#endif

#if defined (ENABLE_TX) && !defined(DISABLE_TXBUF)
SIGNAL(UART0_TRANSMIT_INTERRUPT)
/*************************************************************************
Function: UART Data Register Empty interrupt
Purpose:  called when the UART is ready to transmit the next byte
**************************************************************************/
{	unsigned char tmptail;
    if ( UART_TxHead != UART_TxTail) {
        /* calculate and store new buffer index */
        tmptail = (UART_TxTail + 1);
		if (tmptail>=UART_TX_BUFFER_SIZE)
			tmptail=0;

        UART_TxTail = tmptail;
        /* get one byte from buffer and write it to UART */
        UART0_DATA = UART_TxBuf[tmptail];  /* start transmission */
    }else{
        /* tx buffer empty, disable UDRE interrupt */
        UART0_CONTROL &= ~_BV(UART0_UDRIE);
    }
}
#endif

/*************************************************************************
Function: uart_init()
Purpose:  initialize UART and set baudrate
Input:    baudrate using macro UART_BAUD_SELECT()
Returns:  none
**************************************************************************/
void uart_init(unsigned int baudrate)
{
#ifdef ENABLE_RX
    UART_RxHead = 0;
    UART_RxTail = 0;
#ifdef USE_CTS
	CTS=0;
#endif
#ifdef RX_COUNT
	UART_RxCnt=0;
#endif
#endif

#if defined (ENABLE_TX) && !defined(DISABLE_TXBUF)
    UART_TxTail = 0;
    UART_TxHead = 0;
#endif
    
#if defined( AT90_UART )
    /* set baud rate */
    UBRR = (unsigned char)baudrate; 

    /* enable UART receiver and transmmitter and receive complete interrupt */
#ifdef ENABLE_RX
    UART0_CONTROL = _BV(RXCIE)|_BV(RXEN);
#endif
#ifdef ENABLE_TX
    UART0_CONTROL| = _BV(TXEN);
#endif

#elif defined (ATMEGA_USART)
    /* Set baud rate */
    UBRRH = (unsigned char)(baudrate>>8);
    UBRRL = (unsigned char) baudrate;

    /* Enable USART receiver and transmitter and receive complete interrupt */
#ifdef ENABLE_RX
    UART0_CONTROL = _BV(RXCIE)|_BV(RXEN);
#endif
#ifdef ENABLE_TX
    UART0_CONTROL |= _BV(TXEN);
#endif
    
    /* Set frame format: asynchronous, 8data, no parity, 1stop bit */
    #ifdef URSEL
    UCSRC = (1<<URSEL)|(3<<UCSZ0);
    #else
    UCSRC = (3<<UCSZ0);
    #endif 
    
#elif defined (ATMEGA_USART0 )
    /* Set baud rate */
    UBRR0H = (unsigned char)(baudrate>>8);
    UBRR0L = (unsigned char) baudrate;

    /* Enable USART receiver and transmitter and receive complete interrupt */
#ifdef ENABLE_RX
    UART0_CONTROL = _BV(RXCIE)|_BV(RXEN);
#endif
#ifdef ENABLE_TX
    UART0_CONTROL |= _BV(TXEN);
#endif
    
    /* Set frame format: asynchronous, 8data, no parity, 1stop bit */
    #ifdef URSEL0
    UCSR0C = (1<<URSEL0)|(3<<UCSZ00);
    #else
    UCSR0C = (3<<UCSZ00);
    #endif 

#elif defined ( ATMEGA_UART )
    /* set baud rate */
    UBRRHI = (unsigned char)(baudrate>>8);
    UBRR   = (unsigned char) baudrate;

    /* Enable UART receiver and transmitter and receive complete interrupt */
#ifdef ENABLE_RX
    UART0_CONTROL = _BV(RXCIE)|_BV(RXEN);
#endif
#ifdef ENABLE_TX
    UART0_CONTROL| = _BV(TXEN);
#endif

#endif

}/* uart_init */

#else			// uart init ohne interrupt
void uart_init(unsigned int baudrate)
{

#if defined( AT90_UART )
    /* set baud rate */
    UBRR = (unsigned char)baudrate; 

    /* enable UART receiver and transmmitter and receive complete interrupt */
#ifdef ENABLE_RX
    UART0_CONTROL = _BV(RXEN);
#endif
#ifdef ENABLE_TX
    UART0_CONTROL| = _BV(TXEN);
#endif

#elif defined (ATMEGA_USART)
    /* Set baud rate */
    UBRRH = (unsigned char)(baudrate>>8);
    UBRRL = (unsigned char) baudrate;

    /* Enable USART receiver and transmitter and receive complete interrupt */
#ifdef ENABLE_RX
    UART0_CONTROL = _BV(RXEN);
#endif
#ifdef ENABLE_TX
    UART0_CONTROL |= _BV(TXEN);
#endif
    
    /* Set frame format: asynchronous, 8data, no parity, 1stop bit */
    #ifdef URSEL
    UCSRC = (1<<URSEL)|(3<<UCSZ0);
    #else
    UCSRC = (3<<UCSZ0);
    #endif 
    
#elif defined (ATMEGA_USART0 )
    /* Set baud rate */
    UBRR0H = (unsigned char)(baudrate>>8);
    UBRR0L = (unsigned char) baudrate;

    /* Enable USART receiver and transmitter and receive complete interrupt */
#ifdef ENABLE_RX
    UART0_CONTROL = _BV(RXEN);
#endif
#ifdef ENABLE_TX
    UART0_CONTROL |= _BV(TXEN);
#endif
    
    /* Set frame format: asynchronous, 8data, no parity, 1stop bit */
    #ifdef URSEL0
    UCSR0C = (1<<URSEL0)|(3<<UCSZ00);
    #else
    UCSR0C = (3<<UCSZ00);
    #endif 

#elif defined ( ATMEGA_UART )
    /* set baud rate */
    UBRRHI = (unsigned char)(baudrate>>8);
    UBRR   = (unsigned char) baudrate;

    /* Enable UART receiver and transmitter and receive complete interrupt */
#ifdef ENABLE_RX
    UART0_CONTROL = _BV(RXEN);
#endif
#ifdef ENABLE_TX
    UART0_CONTROL| = _BV(TXEN);
#endif
#endif

}/* uart_init */
#endif

#ifdef NO_UART_INT
#ifdef ENABLE_RX
/*************************************************************************
Function: uart_getc()
Purpose:  return byte from ringbuffer  
Returns:  lower byte:  received byte from ringbuffer
          higher byte: last receive error
**************************************************************************/
#ifndef SIMPLE_UART
unsigned int uart_getc(void)
{	unsigned short data;

	if (UART0_STATUS&(1<<RXC))
	    data = UART0_DATA;
	else
		data=UART_NO_DATA;
    
    return data;

}/* uart_getc */
#endif


unsigned char uart_getchar(void)
{
	while (!(UART0_STATUS&(1<<RXC)));
	return UART0_DATA;
}

unsigned char uart_data(void)
{

	if(UART0_STATUS&(1<<RXC))
		return 1;
	else
		return 0;
}

#endif

/*************************************************************************
Function: uart_putc()
Purpose:  write byte to ringbuffer for transmitting via UART
Input:    byte to be transmitted
Returns:  none          
**************************************************************************/
#ifdef ENABLE_TX
void uart_putc(unsigned char data)
{
	while (!(UART0_STATUS&(1<<UDRE)));	//wait until Buffer empty
	UART0_DATA=data;

}/* uart_putc */


/*************************************************************************
Function: uart_puts()
Purpose:  transmit string to UART
Input:    string to be transmitted
Returns:  none          
**************************************************************************/

void uart_puts(const char *s )
{
    while (*s) 
      uart_putc(*s++);
}/* uart_puts */

/*************************************************************************
Function: uart_puts_p()
Purpose:  transmit string from program memory to UART
Input:    program memory string to be transmitted
Returns:  none
**************************************************************************/

void uart_puts_p(const char *progmem_s )
{	unsigned char c;
    while ( (c = pgm_read_byte(progmem_s++)) ) 
      uart_putc(c);
}/* uart_puts_p */
#endif


#else

#ifdef ENABLE_RX
#ifndef SIMPLE_UART
unsigned int uart_getc(void)
{    
#if (UART_RX_BUFFER_SIZE<=256)
    unsigned char tmptail;
#else
    unsigned short tmptail;
#endif
    unsigned char data;
    if ( UART_RxHead == UART_RxTail )
	{	return UART_NO_DATA;   /* no data available */
    }

#ifdef RX_COUNT
	cli();
	UART_RxCnt--;
#ifdef USE_CTS
	if (UART_RxCnt<((UART_RX_BUFFER_SIZE)/2))
		CTS=0;
#endif
	sei();
#endif
    /* calculate /store buffer index */
    tmptail = (UART_RxTail + 1);
	if (tmptail>=UART_RX_BUFFER_SIZE)
		tmptail=0;

    UART_RxTail = tmptail; 
    /* get data from receive buffer */
    data = UART_RxBuf[tmptail];
    return (UART_LastRxError << 8) + data;
}/* uart_getc */
#endif
#endif

/*************************************************************************
Function: uart_putc()
Purpose:  write byte to ringbuffer for transmitting via UART
Input:    byte to be transmitted
Returns:  none          
**************************************************************************/
#ifdef ENABLE_TX
void uart_putc(unsigned char data)
{
#ifdef DISABLE_TXBUF

	while (!(UART0_STATUS&(1<<UDRE)));	//wait until Buffer empty
	UART0_DATA=data;

#else
    unsigned char tmphead;
    tmphead  = (UART_TxHead + 1);
	if (tmphead>=UART_TX_BUFFER_SIZE)
		tmphead=0;
    while ( tmphead == UART_TxTail ); // wait for free space in buffer   
    UART_TxBuf[tmphead] = data;
    UART_TxHead = tmphead;
    UART0_CONTROL |= _BV(UART0_UDRIE);    // enable UDRE interrupt
#endif
}/* uart_putc */
#endif

/*************************************************************************
Function: uart_puts()
Purpose:  transmit string to UART
Input:    string to be transmitted
Returns:  none          
**************************************************************************/
#ifdef ENABLE_TX
void uart_puts(const char *s )
{
    while (*s) 
      uart_putc(*s++);

}/* uart_puts */

/*************************************************************************
Function: uart_puts_p()
Purpose:  transmit string from program memory to UART
Input:    program memory string to be transmitted
Returns:  none
**************************************************************************/

void uart_puts_p(const char *progmem_s )
{
	unsigned char c;
    while ( (c = pgm_read_byte(progmem_s++)) ) 
      uart_putc(c);

}/* uart_puts_p */
#endif
#ifdef ENABLE_RX
unsigned char uart_getchar(void)
{
#if (UART_RX_BUFFER_SIZE<=256)
    unsigned char tmptail;
#else
    unsigned short tmptail;
#endif
    while ( UART_RxHead == UART_RxTail ); 
    tmptail = (UART_RxTail + 1);     // calculate /store buffer index
	if (tmptail>=UART_RX_BUFFER_SIZE)
		tmptail=0;

    UART_RxTail = tmptail; 

#ifdef RX_COUNT
	cli();
	UART_RxCnt--;
#ifdef USE_CTS
	if (UART_RxCnt<((UART_RX_BUFFER_SIZE)/2))
		CTS=0;
#endif
	sei();
#endif
    return UART_RxBuf[tmptail];		// get data from receive buffer
}

#if ((defined RX_COUNT)&&(UART_RX_BUFFER_SIZE>=256))
unsigned short uart_data(void)
#else
unsigned char uart_data(void)
#endif
{
#ifdef RX_COUNT
	return UART_RxCnt;	
#else
	if(UART_RxHead == UART_RxTail)
		return 0;
	else
		return 1;
#endif
}
#endif
#endif

/*
 * these functions are only for ATmegas with two USART
 */
#if defined( ATMEGA_USART1 )

SIGNAL(UART1_RECEIVE_INTERRUPT)
/*************************************************************************
Function: UART1 Receive Complete interrupt
Purpose:  called when the UART1 has received a character
**************************************************************************/
{
    unsigned char tmphead;
    unsigned char data;
    unsigned char usr;
    unsigned char lastRxError;
 
    /* read UART status register and UART data register */ 
    usr  = UART1_STATUS;
    data = UART1_DATA;
    /* */
    lastRxError = (usr & (_BV(FE1)|_BV(DOR1)) ); 
    /* calculate buffer index */ 
    tmphead = ( UART1_RxHead + 1) & UART1_RX_BUFFER_MASK;
    if ( tmphead == UART1_RxTail ) {
        /* error: receive buffer overflow */
        lastRxError = UART_BUFFER_OVERFLOW >> 8;
    }else{
        /* store new index */
        UART1_RxHead = tmphead;
        /* store received data in buffer */
        UART1_RxBuf[tmphead] = data;
    }
    UART1_LastRxError = lastRxError;   
}

SIGNAL(UART1_TRANSMIT_INTERRUPT)
/*************************************************************************
Function: UART1 Data Register Empty interrupt
Purpose:  called when the UART1 is ready to transmit the next byte
**************************************************************************/
{
    unsigned char tmptail;
    if ( UART1_TxHead != UART1_TxTail) {
        /* calculate and store new buffer index */
        tmptail = (UART1_TxTail + 1) & UART1_TX_BUFFER_MASK;
        UART1_TxTail = tmptail;
        /* get one byte from buffer and write it to UART */
        UART1_DATA = UART1_TxBuf[tmptail];  /* start transmission */
    }else{
        /* tx buffer empty, disable UDRE interrupt */
        UART1_CONTROL &= ~_BV(UART1_UDRIE);
    }
}

/*************************************************************************
Function: uart1_init()
Purpose:  initialize UART1 and set baudrate
Input:    baudrate using macro UART_BAUD_SELECT()
Returns:  none
**************************************************************************/
void uart1_init(unsigned int baudrate)
{
    UART1_TxHead = 0;
    UART1_TxTail = 0;
    UART1_RxHead = 0;
    UART1_RxTail = 0;
    

    /* Set baud rate */
    UBRR1H = (unsigned char)(baudrate>>8);
    UBRR1L = (unsigned char) baudrate;

    /* Enable USART receiver and transmitter and receive complete interrupt */
    UART1_CONTROL = _BV(RXCIE1)|(1<<RXEN1)|(1<<TXEN1);
    
    /* Set frame format: asynchronous, 8data, no parity, 1stop bit */   
    #ifdef URSEL1
    UCSR1C = (1<<URSEL1)|(3<<UCSZ10);
    #else
    UCSR1C = (3<<UCSZ10);
    #endif 
}/* uart_init */


/*************************************************************************
Function: uart1_getc()
Purpose:  return byte from ringbuffer  
Returns:  lower byte:  received byte from ringbuffer
          higher byte: last receive error
**************************************************************************/
unsigned int uart1_getc(void)
{    
    unsigned char tmptail;
    unsigned char data;


    if ( UART1_RxHead == UART1_RxTail ) {
        return UART_NO_DATA;   /* no data available */
    }
    
    /* calculate /store buffer index */
    tmptail = (UART1_RxTail + 1) & UART1_RX_BUFFER_MASK;
    UART1_RxTail = tmptail; 
    
    /* get data from receive buffer */
    data = UART1_RxBuf[tmptail];
    
    return (UART1_LastRxError << 8) + data;

}/* uart1_getc */


/*************************************************************************
Function: uart1_putc()
Purpose:  write byte to ringbuffer for transmitting via UART
Input:    byte to be transmitted
Returns:  none          
**************************************************************************/
void uart1_putc(unsigned char data)
{
    unsigned char tmphead;

    
    tmphead  = (UART1_TxHead + 1) & UART1_TX_BUFFER_MASK;
    
    while ( tmphead == UART1_TxTail ){
        ;/* wait for free space in buffer */
    }
    
    UART1_TxBuf[tmphead] = data;
    UART1_TxHead = tmphead;

    /* enable UDRE interrupt */
    UART1_CONTROL    |= _BV(UART1_UDRIE);

}/* uart1_putc */


/*************************************************************************
Function: uart1_puts()
Purpose:  transmit string to UART1
Input:    string to be transmitted
Returns:  none          
**************************************************************************/
void uart1_puts(const char *s )
{
    while (*s) 
      uart1_putc(*s++);

}/* uart1_puts */


/*************************************************************************
Function: uart1_puts_p()
Purpose:  transmit string from program memory to UART1
Input:    program memory string to be transmitted
Returns:  none
**************************************************************************/
void uart1_puts_p(const char *progmem_s )
{
    register char c;
    
    while ( (c = pgm_read_byte(progmem_s++)) ) 
      uart1_putc(c);

}/* uart1_puts_p */


#endif




