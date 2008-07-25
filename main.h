#define CHANNEL			1			// Sende/Empfangskanal (0-3) (nur gültig wenn kein DIP Schalter verwendet wird)
#define RF_BAUDRATE		20000		// Baudrate des RFM12 (nur gültig wenn kein DIP Schalter verwendet wird)
#define UART_BAUDRATE	19200		// Baudrate des UARTs (nur gültig wenn kein DIP Schalter verwendet wird)
#define PROTOKOLL_V2

#define COMMAND_SET_RELAIS 0

#define MY_ADDRESS 0x0A
//#define DIP_KEYBOARD
#define ADDRESS_DDR DDRC
#define ADDRESS_PIN PINC
#define ADDRESS_PORT PORTC

extern unsigned char myAddress;
extern unsigned char destination;
