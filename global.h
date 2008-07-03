extern void delay (unsigned int us);

#ifndef cbi
#define cbi(sfr, bit)     (_SFR_BYTE(sfr) &= ~_BV(bit)) 
#endif
#ifndef sbi
#define sbi(sfr, bit)     (_SFR_BYTE(sfr) |= _BV(bit))  
#endif

typedef union conver_ {
	unsigned long dw;
	unsigned int w[2];
	unsigned char b[4];
} CONVERTDW;

typedef union conver2_ {
	unsigned int w;
	unsigned char b[2];
} CONVERTW;


