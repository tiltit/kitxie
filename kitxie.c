#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdlib.h>

#define LED_PORT				PORTD
#define LED1_PIN				PD4
#define LED2_PIN				PD5

#define CLK_PORT				PORTD
#define CLK_PIN					PD6
#define DATA_PORT				PORTB
#define DATA_PIN				PB0
#define LATCH_PORT				PORTB
#define LATCH_PIN				PB1

/**
 * External interupt pins :
 * PD2 INT0
 * PD3 INT1
 */
#define DCF77_PIN				PD2

#define ENCODER_PINA			PD3
#define ENCODER_PINB			PB4
#define ENCODER_BUTTON_PIN		PB5

#define SWITCH_A_PIN			PB2
#define SWITCH_B_PIN			PB3

#define SPEAKER_PIN				PD0

#define BAUD 115200
#define MYUBRR ( ( F_CPU / ( BAUD * 8UL ) ) - 1 )

/* ** CONSTANTS ** */
//const uint8_t digit_order[] = {3,4,8,7,5,9,6,2,1,0};
const uint8_t digit_order[] PROGMEM = {3,8,5,6,1,0,2,9,7,4};
//const uint8_t digit_order[] = {3,7,6,0,2,5,4,8,9,1};
const uint8_t melody[] PROGMEM = {0x2B, 0x50, 0x80};

/* ** GLOBALS ** */
volatile uint16_t time_count = 0;
volatile uint16_t time_mesure = 0;
volatile uint8_t dcf_seconds = 0;
volatile uint8_t seconds = 0;
volatile int8_t minutes = 0;
volatile int8_t hours = 0;
volatile uint8_t dcf_data[8];
//volatile uint16_t rotary_val;
volatile int8_t timer_minutes;
volatile int8_t timer_seconds;
int8_t switch_position;

void init(void) 
{
	/* Init io */
	/* Set up led and speaker outputs */
	DDRD |= (1 << LED1_PIN) | (1 << LED2_PIN) | (1 << SPEAKER_PIN);
	/* Set up outputs for the shift register */
	DDRD |= (1 << CLK_PIN);
	DDRB |= (1 << DATA_PIN) | (1 << LATCH_PIN);
	/* Turn on pullups for the button inputs */
	PORTB |= ~_BV(ENCODER_BUTTON_PIN) | ~_BV(SWITCH_A_PIN) | ~_BV(SWITCH_B_PIN);

	/* Setup external interupts */
	MCUCR |= (1<<ISC00) | (1 << ISC10);		/* Any logical change of int0 & int1 generates an interupt */
	GIMSK |= (1<<INT0) | (1<<INT1);			/* Enable INT0 and INT1 */


	/* Configure Timer 0 for 1ms interupt*/
	TCCR0A = (1<<WGM01); 				/* CTM Clear Timer on compare match mode */
	TCCR0B |= (1<<CS00) | (1<<CS01); 	/* Set the prescaler to 64 */
	/* ((16000000/8)/1000) = 250 */
	OCR0A = 249;
	/* Enable compare interupt */
	TIMSK |= (1<<OCIE0A);

	/* Configure timer 1 */
	TCCR1A &= ~(1<<WGM10);
	TCCR1A &= ~(1<<WGM11);
	TCCR1A |= (1<<COM1A0);
	TCCR1A &= !(1<<COM1A1);

	TCCR1B |= (1<<WGM12);
	//TCCR1B |= (1<<WGM13);
	TCCR1B &= ~(1<<WGM13);

	TCCR1B |= (1<<CS12);		/* Prescaler set to 256 */
	//OCR1A = 10;				/* Output Compare A register */
	//TIMSK |= (1<<OCIE1A);		/* Output Compare A Match Interrupt Enable */

}

void shiftout(uint16_t val)
{
	int8_t i;
	for(i=0;i!=16;++i) {
		if(val & (0x8000 >> i))
			DATA_PORT |= _BV(DATA_PIN);
		else
			DATA_PORT &= ~_BV(DATA_PIN);
		_delay_us(55);
		CLK_PORT |= _BV(CLK_PIN);
		_delay_us(55);	
		CLK_PORT &= ~_BV(CLK_PIN);
		_delay_us(10);
	}
	LATCH_PORT |= _BV(LATCH_PIN);
	_delay_us(55);
	LATCH_PORT &= ~_BV(LATCH_PIN);
	_delay_us(55);
}

void show_number(uint16_t display_value)
{
	static uint8_t digit;
	uint16_t out;
	uint8_t digit_value;

	digit++;
	digit %= 4;
	switch(digit) {
		case 0 : digit_value = (display_value/1000) % 10;break;
		case 1 : digit_value = (display_value/100) % 10; break;
		case 2 : digit_value = (display_value/10) % 10; break;
		case 3 : digit_value = display_value % 10; break;
	}
	out = (1 << (15 - digit));
	out |= (1 << digit_value);
	shiftout(out);
}

/**
 * Decodes the bits colected with the dcf77
 * module.
 */
void get_new_time()
{
	uint8_t i;
	bool dcf_unvalid = false;
	
	uint8_t new_minutes = 0;		
	uint8_t new_hours = 0;
	uint8_t new_seconds = 0;
	uint8_t minutes_parity=0, hours_parity=0, date_parity=0;

	/* Check if start of encoded (bit 20) time is true */
	if(dcf_data[2] & (1 << 4) ) {
		for(i=0;i!=59;++i) {

			/* Add up the parity bits */
			if(dcf_data[i/8] & (1 << (i % 8))) {
				if((i>20) && (i<28))
					minutes_parity++;
				if((i>28) && (i<35))
					hours_parity++;
				if((i>35) && (i<58))
					date_parity++;
					
				switch(i) {
					case 21: new_minutes+=1; break;
					case 22: new_minutes+=2; break;
					case 23: new_minutes+=4; break;
					case 24: new_minutes+=8; break;
					case 25: new_minutes+=10; break;
					case 26: new_minutes+=20; break;
					case 27: new_minutes+=40; break;
					case 28: dcf_unvalid = (++minutes_parity % 2);break;
	
					case 29: new_hours+=1;break;
					case 30: new_hours+=2;break;
					case 31: new_hours+=4;break;
					case 32: new_hours+=8;break;
					case 33: new_hours+=10;break;
					case 34: new_hours+=20;break;
					case 35: dcf_unvalid = (++hours_parity % 2);break;

					case 58: dcf_unvalid = (++date_parity % 2);break;
				}

			}
		}

		if((dcf_unvalid == false) && (new_minutes<60) && (new_hours<24)) {
			seconds = 0;
			minutes = new_minutes;
			hours = new_hours;
		}
	}

	/* Reset buffer */
	for(i=0;i!=8;++i) {
		dcf_data[i] = 0;
	}

}


/*********
*  Main  *
*********/
int main (void)
{
	uint16_t display_value = 0;
	int8_t alarm_anim = -9;
	uint8_t count = 0;
	uint8_t alarm_count = 0;
	uint8_t display_hours;
	uint8_t twelve_hour_mode = 0;
	

	init();
	sei();				/* Enable interupts */
	//print_dcf_input();
	while(1) {
		if( (PINB & _BV(SWITCH_A_PIN)) && (PINB & _BV(SWITCH_B_PIN))) {
			/* This is the middle position of the switch */
			/* Set the timer or the clock*/
			if((switch_position != 0) &&  !(PINB & _BV(ENCODER_BUTTON_PIN)) ) {
				switch_position = 2;	/* Set time mode */
				
			}
			else if (switch_position != 2) {
				switch_position = 0;
				display_value = (timer_minutes * 100) + timer_seconds;
			}

			if(switch_position == 2)
				display_value = (hours * 100) + minutes;

			TIMSK &= ~(1<<OCIE1A);
			
		} else {			
			if(!(PINB & _BV(SWITCH_A_PIN))) {
				/* This is the top position of the switch */
				/* Displays the time */
				
				if((switch_position!=-1) && !(PINB & _BV(ENCODER_BUTTON_PIN)) )
					twelve_hour_mode ^= (1);
				
				
				
				switch_position = -1;
				
				if(twelve_hour_mode) {
					display_hours = hours%12;
					display_hours = display_hours < 1 ? 12 : display_hours;
				} else {
					display_hours = hours;
				}
				display_value = (display_hours * 100) + minutes;

				TIMSK &= ~(1<<OCIE1A);
				
			} else {
				/* Bottom position of the switch */
				/* Displays timer countdown */
				switch_position = 1;
				if( (timer_minutes > 0) || (timer_seconds > 0) ) {
					display_value = (timer_minutes * 100) + timer_seconds;
					TIMSK &= ~(1<<OCIE1A);
					alarm_count = 255;
				} else {

					/* digit_order animation */
					if( (++count % 24) == 0) {

						if(++alarm_anim == 10)
							alarm_anim = -8;
						display_value %= 1000;
						display_value *= 10;
						display_value += pgm_read_byte(digit_order+abs(alarm_anim));
					}
					if (count % 32 == 0) {

						if(alarm_count != 0) {

								TIMSK &= ~(1<<OCIE1A);
								if( pgm_read_byte(melody) & (1 << alarm_count % 8) ) {
									OCR1A = 25;
									TIMSK |= (1<<OCIE1A);
								}
								if( pgm_read_byte(melody+1) & (1 << alarm_count % 8) ) {
									OCR1A = 60;
									TIMSK |= (1<<OCIE1A);
								}
								if( pgm_read_byte(melody+2) & (1 << alarm_count % 8) ) {
									OCR1A = 100;
									TIMSK |= (1<<OCIE1A);
								}
								TCNT1 = 0;
								
								if(alarm_count % 16 < 8)
									TIMSK &= ~(1<<OCIE1A);

								alarm_count--;

						} else {
							TIMSK &= ~(1<<OCIE1A);
							PORTD &= ~_BV(SPEAKER_PIN);
						}

					}
				}
			}
		}
		show_number(display_value);
		_delay_us(3000);
	}
	return 0;
}

/* Interupt trigered by the dcf77 pulse */
ISR (INT0_vect)
{
	uint16_t mesure;
	mesure = time_mesure;
	time_mesure=0;
	
	if (!(PIND & _BV(DCF77_PIN))) {
		if(mesure>1500) {
			if(dcf_seconds == 59) {
				get_new_time();
			}
			dcf_seconds =0;
		}
		PORTD |= _BV(LED2_PIN);
	} else {
		PORTD &= ~_BV(LED2_PIN);
		
		if (mesure >= 50) {
			if( mesure > 140 ) {
				dcf_data[dcf_seconds / 8] |= (1 << (dcf_seconds % 8));
			}
			dcf_seconds++;
		}
	}
}

void inc(volatile int8_t *i, int8_t limit)
{
	if(++(*i) > limit)
		*i = 0;
}

void dec(volatile int8_t *i, int8_t limit)
{
	if(--(*i) < 0)
		*i = limit; 
}

/* External Interupt for the Rotary Encoder */
ISR (INT1_vect)
{
	bool direction;
	if(!(PIND & _BV(ENCODER_PINA))) {
		if( !(PINB & _BV(ENCODER_PINB)) ) {
			direction = true;
		} else {
			direction = false;
		}
	} else {
		if( !(PINB & _BV(ENCODER_PINB)) ) {
			direction = false;
		} else {
			direction = true;
		}
	}


	if(switch_position == 0) {	
		if(PINB & _BV(ENCODER_BUTTON_PIN)) { 
			if(direction) {
				inc(&timer_minutes, 99);
			} else {
				dec(&timer_minutes , 99);
			}
		} else {
			if(direction) {
				inc(&timer_seconds, 59);
			} else {
				dec(&timer_seconds, 59);
			}
		}
	}

	if(switch_position == 2) {
		if(PINB & _BV(ENCODER_BUTTON_PIN)) {
			if(direction) {
				inc(&hours, 23);
			} else {
				dec(&hours, 23);
			}
		} else {
			if(direction) {
				inc(&minutes, 59);
			} else {
				dec(&minutes, 59);
			}
		}
	}
}

/**
 * Timer used to count seconds
 * and mesure the dcf77 pulses
 */
ISR (TIMER0_COMPA_vect)  // timer0 overflow interrupt
{
	time_count++;
	time_mesure++;
	if(time_count%1000 == 0) {
		time_count=0;
		PORTD ^= _BV(LED1_PIN);

		if(++seconds >= 60) {
			seconds = 0;
			if(++minutes>=60) {
				minutes=0;
				if(++hours>=24)
					hours = 0;
			}
		}
		/* Timer Countdown */
		if((switch_position == 1) && ( (timer_seconds > 0) || (timer_minutes > 0) )) {
			if(--timer_seconds < 0) {
				if(--timer_minutes < 0) {
					timer_seconds = 0;
					timer_minutes = 0;
					return;
				}
				timer_seconds = 59;
			}
		}
	}
}

ISR(TIMER1_COMPA_vect)
{
	/* Just toggle a pin to make some noise */
	PORTD ^= _BV(SPEAKER_PIN);
}

