
/*	Author: lab
 *  Partner(s) Name: 
 *	Lab Section:
 *	Assignment: PORTB = tmpBT1;Lab #  Exercise #
 *	Exercise Description: [optional - include for your own benefit]
 *
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#endif


void transmit_data(unsigned char data, unsigned char cs) {
    int i;
    for (i = 0; i < 8 ; ++i) {
   	 // Sets SRCLR to 1 allowing data to be set
   	 // Also clears SRCLK in preparation of sending data
	 if(cs == 0) PORTC = 0x08;
	 else if (cs == 1) PORTC = 0x20;
   	 // set SER = next bit of data to be sent.
   	 PORTC |= ((data >> i) & 0x01);
   	 // set SRCLK = 1. Rising edge shifts next bit of data into the shift register
   	 PORTC |= 0x02;  
    }
    // set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
    if(cs == 0) PORTC |= 0x04;
    else if(cs == 1) PORTC |= 0x10;
    // clears all lines in preparation of a new transmission
    PORTC = 0x00;
}

unsigned long _avr_timer_M = 1; //start count from here, down to 0. Dft 1ms
unsigned long _avr_timer_cntcurr = 0; //Current internal count of 1ms ticks

void A2D_init() {
      ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	// ADEN: Enables analog-to-digital conversion
	// ADSC: Starts analog-to-digital conversion
	// ADATE: Enables auto-triggering, allowing for constant
	//	    analog to digital conversions.
}

void Set_A2D_Pin(unsigned char pinNum) {
	ADMUX = (pinNum <= 0x07) ? pinNum : ADMUX;
	// Allow channel to stabilize
	static unsigned char i = 0;
	for ( i=0; i<15; i++ ) { asm("nop"); } 
}

void TimerOn(){
	//AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B; //bit 3 = 0: CTC mode (clear timer on compare)
	//AVR output compare register OCR1A
	OCR1A = 125; // Timer interrupt will be generated when TCNT1 == OCR1A
	//AVR timer interrupt mask register
	TIMSK1 = 0x02; //bit1: OCIE1A -- enables compare match interrupt
	//Init avr counter
	TCNT1 = 0;

	_avr_timer_cntcurr = _avr_timer_M;
	//TimerISR will be called every _avr_timer_cntcurr ms
	
	//Enable global interrupts 
	SREG |= 0x80; //0x80: 1000000
}

void TimerOff(){
	TCCR1B = 0x00; //bit3bit1bit0 = 000: timer off
}

ISR(TIMER1_COMPA_vect){
	_avr_timer_cntcurr--;
	if (_avr_timer_cntcurr == 0) {
			TimerISR();
			_avr_timer_cntcurr = _avr_timer_M;
			}
}

void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

typedef struct task {
  int state; // Current state of the task
  unsigned long period; // Rate at which the task should tick
  unsigned long elapsedTime; // Time since task's previous tick
  int (*TickFct)(int); // Function to call for task's tick
} task;

task tasks[3];

const unsigned char tasksNum = 3;

const unsigned long tasksPeriodGCD = 1;
const unsigned long periodSample = 100;
const unsigned long periodDisplay = 1;
const unsigned long periodBall = 300;

enum DSPLY_States {DS_start, DS_game, DS_score, DS_blank, DS_firework};
int DSPLY_Tick(int state);


enum BALL_Tick {BALL_start, BALL_run};
int BALL_Tick(int state);

enum JS_States {sample};
int JS_Tick(int state);

int JSV_Tick(int state);

void TimerISR() {
  unsigned char i;
  for (i = 0; i < tasksNum; ++i) { // Heart of the scheduler code
     if ( tasks[i].elapsedTime >= tasks[i].period ) { // Ready
        tasks[i].state = tasks[i].TickFct(tasks[i].state);
        tasks[i].elapsedTime = 0;
     }
     tasks[i].elapsedTime += tasksPeriodGCD;
  }
}

unsigned char g_pattern;
unsigned char g_row;

/* Score digits
 *    |   |   |   |   |   |   |   |   |   |
 *  000   1 222 333 4 4 555 666 777 888 999
 *  0 0   1   2   3 4 4 5   6     7 8 8 9 9
 *  0 0   1 222 333 444 555 666   7 888 999
 *  0 0   1 2     3   4   5 6 6   7 8 8   9
 *  000   1 222 333   4 555 666   7 888   9
 */
const unsigned char score0[10] = {7,1,7,7,5,7,7,7,7,7};
const unsigned char score1[10] = {5,1,1,1,5,4,4,1,5,5};
const unsigned char score2[10] = {5,1,7,7,7,7,7,1,7,7};
const unsigned char score3[10] = {5,1,4,1,1,1,5,1,5,1};
const unsigned char score4[10] = {7,1,7,7,1,7,7,1,7,1};

unsigned char lScore = 0;
unsigned char rScore = 4;
unsigned char ballX = 3;
unsigned char ballY = 3;
unsigned char lPadY = 3;
unsigned char rPadY = 0;
unsigned char ballH = 0;
unsigned char ballV = 0;

int main() {
 
  DDRB = 0x07; PORTB = 0x00;
  DDRD = 0xFF; PORTD = 0x00;
  DDRC = 0xFF; PORTC = 0x00;
  unsigned char i=0;
  A2D_init();
  tasks[i].state = DS_start;
  tasks[i].period = periodDisplay;
  tasks[i].elapsedTime = tasks[i].period;
  tasks[i].TickFct = &DSPLY_Tick;
  ++i;
  tasks[i].state = sample;
  tasks[i].period = periodSample;
  tasks[i].elapsedTime = tasks[i].period;
  tasks[i].TickFct = &JS_Tick;
  ++i;
  tasks[i].state = BALL_start;
  tasks[i].period = periodBall;
  tasks[i].elapsedTime = tasks[i].period;
  tasks[i].TickFct = &BALL_Tick;
  //TimerSet(tasksPeriodGCD);

  TimerOn();
  
  while(1) {
  }
  return 0;
}

int DSPLY_Tick(int state) {
	unsigned char dispBuf[5] = {0, 0, 0, 0, 0};
	static unsigned char curRow = 0;
	unsigned char nxtRow = 0;
	// unsigned char i;
	// Transition 
	switch (state) {
		case DS_start:
			  state = DS_game;
			  break;
		case DS_game:
			  break;
		case DS_score:
			  break;
		case DS_blank:
			  break;
		case DS_firework:
			  break;
		default:
			  break;
	}
	// Action 
	switch (state) {
		case DS_start:
			  break;
		case DS_game:
			  // Ball display
			  dispBuf[ballY] = 0x01 << ballX;
			  //dispBuf[3] = 0x01 << 3;
			  // left paddle display
			  dispBuf[lPadY] = dispBuf[lPadY] | (0x01 << 7);
			  nxtRow = lPadY +1;
			  dispBuf[nxtRow] = dispBuf[nxtRow] | (0x01 << 7);
			  // right paddle display
			  dispBuf[rPadY] = dispBuf[rPadY] | 0x01;
			  nxtRow = rPadY +1;
			  dispBuf[nxtRow] = dispBuf[nxtRow] | 0x01;
			  break;
		case DS_score:
			  dispBuf[0] = (score0[lScore]<<5) | (score0[rScore]); 
			  dispBuf[1] = (score1[lScore]<<5) | (score1[rScore]); 
			  dispBuf[2] = (score2[lScore]<<5) | (score2[rScore]); 
			  dispBuf[3] = (score3[lScore]<<5) | (score3[rScore]); 
			  dispBuf[4] = (score4[lScore]<<5) | (score4[rScore]); 
			  break;
		case DS_blank:
			  break;
		case DS_firework:
			  break;
		default:
			  break;
	}
	//transmit_data(g_pattern, 0);
	//transmit_data(~g_row, 1);
	transmit_data(0xFF, 1);
	transmit_data(dispBuf[curRow], 0);
	transmit_data(~(0x01 << curRow), 1);
	// for ( i=0; i<15; i++ ) { asm("nop"); } 
	//  Next row
	if (curRow < 4) curRow++;
	else            curRow = 0;
	// Return state
	return state;
}

int JS_Tick(int state) {
	static unsigned char pattern = 0x10;	// LED pattern - 0: LED off; 1: LED on
	short deviation;   	                // Joy stick position deviation from neutral
	//adc 
	Set_A2D_Pin(1);
	unsigned short input = ADC;
	
	// Actions
	switch (state) {
		case sample:
			// update dot in horizental direction
			if(input > 600){
				//if(pattern > 0x01) pattern = pattern >> 1;
				// else pattern = 0x80; 
				if(rPadY > 0) rPadY = rPadY - 1;
				else rPadY = 0x00; 
			}
			else if(input < 400){
				//if(pattern < 0x80) pattern = pattern << 1;
				// else pattern = 0x01;
				if(rPadY < 3) rPadY = rPadY + 1;
				else rPadY = 0x03; 
			}	
			/*
			// update update rate
			deviation = input - 512;
			if (deviation < 0) deviation = -deviation;
			if (deviation > 450)
				tasks[1].period = 100;
			else if (deviation > 300)
				tasks[1].period = 250;
			else if (deviation > 150)
				tasks[1].period = 500;
			else
				tasks[1].period = 1000;
			*/
			break;
		default:
			break;
	}
	g_pattern = pattern;
	return state;
}

int BALL_Tick(int state) {
	switch (state) {
		case BALL_start:
			  state = BALL_run;
			  break;
		case BALL_run:
			  break;
		default:
			  break;
	}
	// Action 
	switch (state) {
		case BALL_start:
			  break;
		case BALL_run:
			  if(ballH == 0){
				  if(ballX != 0) ballX--;
				  else{ ballH = 1; ballX++;}
			  }
			  else{
				  if(ballX != 7) ballX++;
				  else{ ballH = 0; ballX--;}
			  }
			  if(ballV == 0){
				  if(ballY != 0) ballY--;
				  else{ ballV = 1; ballY++;}
			  }
			  else{
				  if(ballY != 4) ballY++;
				  else{ ballV = 0; ballY--;}
			  }
			  break;
		default:
			  break;
	}
	// Return state
	return state;
}















int JSV_Tick(int state) {
	static unsigned char row = 0x04;  	// Row(s) displaying pattern. 
	short deviation;   	                // Joy stick position deviation from neutral
	//adc 
	Set_A2D_Pin(1);
	unsigned short input = ADC;
	
	// Actions
	switch (state) {
		case sample:
			// update dot in horizental direction
			if(input > 562){
				if(row > 0x01) row = row >> 1;
				// else row = 0x80; 
			}
			else if(input < 462){
				if(row < 0x10) row = row << 1;
				// else row = 0x01;
			}	
			// update update rate
			deviation = input - 512;
			if (deviation < 0) deviation = -deviation;
			if (deviation > 450)
				tasks[2].period = 100;
			else if (deviation > 300)
				tasks[2].period = 250;
			else if (deviation > 150)
				tasks[2].period = 500;
			else
				tasks[2].period = 1000;
			
			break;
		default:
			break;
	}
	g_row = row;
	return state;
}

