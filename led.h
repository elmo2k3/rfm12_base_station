#define LED_PORT PORTD
#define LED_DDRD DDRD
#define LED_BLUE (1<<PD5)
#define LED_GREEN (1<<PD6)
#define LED_RED (1<<PD7)

extern uint8_t Red,Green,Blue;
extern void led_init(void);
