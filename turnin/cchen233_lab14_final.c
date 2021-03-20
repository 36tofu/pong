
/* Author:Christopher Chen
 * Partner(s) Name (if applicable):  
 * Lab Section:21
 * Assignment: Lab #14  Exercise #
 * Exercise Description: [optional - include for your own benefit]
 *
 * I acknowledge all content contained herein, excluding template or example
 * code, is my own original work.
 *
 *  Demo Link(Advancements 1-3): https://youtu.be/C9YKyU7oWKk
 *  Advancement 4: https://youtu.be/KUze--aHlmk
 *
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#include <stdlib.h>
#endif

void set_PWM(double frequency){
	static double current_frequency;

	if (frequency != current_frequency) {
		if (!frequency) { TCCR3B &= 0x08; }
		else {TCCR3B |= 0x03; }

		if (frequency < 0.954) { OCR3A = 0xFFFF; }

		else if (frequency > 31250) { OCR3A = 0x0000; }

		else { OCR3A = (short)(8000000 / (128 * frequency)) - 1; }

		TCNT3 = 0;
		current_frequency = frequency; 
	}
}

void PWM_on(){
	TCCR3A = (1 << COM3A0);

	TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);

	set_PWM(0);
}

void PWM_off(){
	TCCR3A = 0x00;
	TCCR3B = 0x00;
}

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

task tasks[5];

const unsigned char tasksNum = 5;

const unsigned long tasksPeriodGCD = 1;
const unsigned long periodSample = 40;
const unsigned long periodDisplay = 1;
const unsigned long periodBall = 350;
const unsigned long periodLP = 80;
const unsigned long periodSound= 150;

enum DSPLY_States {DS_start, DS_game, DS_score, DS_blank, DS_firework, DS_winner, DS_winner2, DS_ballSel, DS_plrs};
int DSPLY_Tick(int state);


enum BALL_Tick {BALL_start, BALL_run, BALL_stop};
int BALL_Tick(int state);

enum JS_States {sample};
int JS_Tick(int state);

enum LP_States {sampleLP};
int LP_Tick(int state);

enum FRQ_States { FRQ_SMStart, FRQ_s0, FRQ_1, FRQ_2};
int TickFct_FRQ(int state);


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
unsigned char rScore = 0;
unsigned char ballX = 3;
unsigned char ballY = 3;
unsigned char rPadY = 1;
unsigned char lPadY = 1;
unsigned char ballH = 0;
unsigned char ballV = 0;
unsigned char missR = 0;
unsigned char missL = 0;
unsigned char ballRestart = 0;
unsigned char paddleMove = 0;
unsigned char numPlayers = 1;
unsigned char sound = 0;

int main() {
  DDRB = 0x47; PORTB = 0x00;
  DDRD = 0x00; PORTD = 0xFF;
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
  ++i;
  tasks[i].state = sampleLP;
  tasks[i].period = periodLP;
  tasks[i].elapsedTime = tasks[i].period;
  tasks[i].TickFct = &LP_Tick;
  ++i;
  tasks[i].state = FRQ_SMStart;
  tasks[i].period = periodSound;
  tasks[i].elapsedTime = tasks[i].period;
  tasks[i].TickFct = &TickFct_FRQ;
  //TimerSet(tasksPeriodGCD);

  TimerOn();
  PWM_on();
  
  while(1) {
  }
  return 0;
}

int DSPLY_Tick(int state) {
	static unsigned char maxBalls = 3;
	unsigned char dispBuf[5] = {0, 0, 0, 0, 0};
	static unsigned char curRow = 0;
	unsigned char nxtRow = 0;
	static unsigned char periodCounter = 500;
	static unsigned char flashCounter = 3;
	unsigned char tmpD = (~PIND & 0x1F);
	PORTB = lScore;
	

	// unsigned char i;
	// Transition 
	switch (state) {
		case DS_start:
			  state = DS_ballSel;
			  break;
		case DS_ballSel:
			  if(periodCounter > 0)
				 periodCounter--;
			  else
				 periodCounter = 350; 
			  if(periodCounter == 10){
				  if(tmpD == 0x02 && maxBalls < 9)
				  	maxBalls++;
				  else if(tmpD == 0x01 && maxBalls > 1)
				  	maxBalls--;
				  else if(tmpD == 0x10){
					  state = DS_plrs;
					  periodCounter = 2000;
				  }
			  }
		case DS_plrs:
			  if(periodCounter > 0)
				 periodCounter--;
			  else
				 periodCounter = 1000; 
			  if(periodCounter == 10){
				  if(tmpD == 0x02 && numPlayers == 1)
				  	numPlayers++;
				  else if(tmpD == 0x01 && numPlayers == 2)
				  	numPlayers--;
				  else if(tmpD == 0x04){
					  missL = 0;
					  missR = 0;
					  lScore = 0;
					  rScore = 0;
					  state = DS_game;
				  }
			  }
			  break;
		case DS_game:
			  if(missL == 1 || missR == 1){
			  	if(missL == 1) {rScore++; missL = 0;}
			  	else if(missR == 1) {lScore++; missR = 0;}
				state = DS_score;
				periodCounter = 1000;
			  }
			  break;
		case DS_score:
			if(tmpD == 0x04){
				if(lScore == maxBalls || rScore == maxBalls){
					periodCounter = 1000;
					state = DS_winner;
				}
				else
					state = DS_game;
			}

			  break;
		case DS_winner:
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
		case DS_ballSel:
			  dispBuf[0] = (0xE0 | (score0[maxBalls])); 
			  dispBuf[1] = (0xA0 | (score1[maxBalls])); 
			  dispBuf[2] = (0xE0 | (score2[maxBalls])); 
			  dispBuf[3] = (0xA0 | (score3[maxBalls])); 
			  dispBuf[4] = (0xE0 | (score4[maxBalls])); 
			  break;
		case DS_plrs:
			  dispBuf[0] = (0xE0 | (score0[numPlayers])); 
			  dispBuf[1] = (0xA0 | (score1[numPlayers])); 
			  dispBuf[2] = (0xE0 | (score2[numPlayers])); 
			  dispBuf[3] = (0x80 | (score3[numPlayers])); 
			  dispBuf[4] = (0x80 | (score4[numPlayers])); 
			  break;
		case DS_game:
			  // Ball display
			  if(missR == 0 && missL == 0)
			  	dispBuf[ballY] = 0x01 << ballX;
			  
			  //dispBuf[3] = 0x01 << 3;
			  // left paddle display
			  dispBuf[lPadY-1] = dispBuf[lPadY-1] | (0x01 << 7);
			  dispBuf[lPadY] = dispBuf[lPadY] | (0x01 << 7);
			  dispBuf[lPadY+1] = dispBuf[lPadY+1] | (0x01 << 7);
			  // right paddle display
			  dispBuf[rPadY-1] = dispBuf[rPadY-1] | 0x01;
			  dispBuf[rPadY] = dispBuf[rPadY] | 0x01;
			  dispBuf[rPadY+1] = dispBuf[rPadY+1] | 0x01;
			  break;
		case DS_score:
			  dispBuf[0] = (score0[lScore]<<5) | (score0[rScore]); 
			  dispBuf[1] = (score1[lScore]<<5) | (score1[rScore]); 
			  dispBuf[2] = (score2[lScore]<<5) | (score2[rScore]); 
			  dispBuf[3] = (score3[lScore]<<5) | (score3[rScore]); 
			  dispBuf[4] = (score4[lScore]<<5) | (score4[rScore]); 
			  break;
		case DS_winner:
			  for(unsigned char i = 0; i < 5; i++){
				  if(rScore == maxBalls)
				  	dispBuf[i] = 0x0F;
				  else
					dispBuf[i] = 0xF0;
				}
			 if(tmpD == 0x08){//play same game
				missL = 0;
				missR = 0;
				rScore = 0;
				lScore = 0;
				state = DS_ballSel;
			 }
			 /*else if(tmpD == 4){
				rScore = 0;
				lScore = 0;
				state = DS_ballSel;
				
			 }
			 */
			  if(periodCounter > 0)	  
			  	periodCounter--;
			  else{
				  periodCounter = 500;
				  state = DS_winner2;
			  }
			  break;
		case DS_winner2:
			  for(unsigned char i = 0; i < 5; i++){
				  	dispBuf[i] = 0x00;
				}
			 /*
			 if(tmpD == 0x08){
				rScore = 0;
				lScore = 0;
				state = DS_game;
			 }
		 */	 
			  if(periodCounter > 0)	  
			  	periodCounter--;
			  else{
				  periodCounter = 500;
				  state = DS_winner;
			  }
			  break;
		case DS_blank:
			  break;
		case DS_firework:
			  break;
		default:
			  break;
	}
	transmit_data(0xFF, 1);
	transmit_data(dispBuf[curRow], 0);
	transmit_data(~(0x01 << curRow), 1);
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
			// update right paddle position
			if(input > 650){
				paddleMove = 1;
				if(rPadY > 1) rPadY = rPadY - 1;
				else rPadY = 0x01; 
			}
			else if(input < 350){
				paddleMove = 2;
				if(rPadY < 3) rPadY = rPadY + 1;
				else rPadY = 0x03; 
			}
			else
				paddleMove = 0;
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

/*
int LP_Tick(int state) {
	unsigned char tmpD = (~PIND & 0x03);
	// Actions
	switch (state) {
		case sampleLP:
			if ( tmpD == 0x02 ) {
				if(lPadY > 1) lPadY = lPadY - 1;
				else lPadY = 0x01; 
			} else if ( tmpD == 0x01 ) {
				if(lPadY < 3) lPadY = lPadY + 1;
				else lPadY = 0x03; 
			}	
			break;
		default:
			break;
	}
	return state;
}
*/

int LP_Tick(int state) {
	// Actions
	unsigned char retard;
	retard = rand() % 8;
	//PORTB = retard;
	unsigned char tmpD = (~PIND & 0x03);
	if(numPlayers == 1){
	switch (state) {
		case sampleLP:
			//if(ballH == 1 && ballX >= 5 && retard > 5){
			if(ballH == 1 && ballX >= 5){
				if(ballY == 4){
					if(lPadY < 3)
						lPadY++;
				}
				else if(ballY == 1){
					if(lPadY > 1)
						lPadY--;
				}
				else if(ballV == 0){
					if(lPadY > 1)
						lPadY--;
				}
				else if(ballV == 1){
					if(lPadY < 3)
						lPadY++;
				}
			}
			break;
		default:
			break;
	}
	}
	else{
	switch (state) {
		case sampleLP:
			if ( tmpD == 0x02 ) {
				if(lPadY > 1) lPadY = lPadY - 1;
				else lPadY = 0x01; 
			} else if ( tmpD == 0x01 ) {
				if(lPadY < 3) lPadY = lPadY + 1;
				else lPadY = 0x03; 
			}	
			break;
		default:
			break;
	}
	}
	return state;
}

int BALL_Tick(int state) {
	// Action 
	unsigned char tmpD = (~PIND & 0x04);
	//PORTB = tmpD;
	unsigned char curveBall = 0;
	static unsigned char speedChange = 0;
	switch (state) {
		case BALL_start:
			  state = BALL_run;
			  break;
		case BALL_run:
			  //speeding 
			  if(speedChange == 0) tasks[2].period = 350;
			  else{
				  speedChange--;
				  tasks[2].period = 150;
			  }
			  
			  //if ball goin right
			  if(ballH == 0){
				  //if it's not at the left of board, keep incrementing
				  if(ballX > 1) ballX--;
				  else if(ballX == 1){ 
					  //if paddle hit corner
					  if((ballY == rPadY-1) || (ballY == rPadY+1)){
					  	ballH = 1;
					       	ballV = 1-ballV;	
					  	ballX++;
						speedChange = 5;
				  		tasks[2].period = 150;
						sound = 1;
					  }
					  //hit center
					  else if(ballY == rPadY){
						  ballH = 1;
						  ballX++;
						  sound = 1;
						  if(paddleMove == 1 && ballV == 1){
						  	ballV = 0;
						  }
						  else if(paddleMove == 2 && ballV == 0){
							  ballV = 1;
						  }
						
					  }
						/*
						//accel on hit corner
						if(((ballV == 1) && (ballY == rPadY)) || ((ballV == 0) && (ballY == rPadY+1))){
							speedChange == 20;
				  			tasks[2].period = 100;
						}
						*/						
					  //if you miss
					  else{
						  state = BALL_stop;
						  missR = 1;
						  ballX--;
					  }
				  }
			  }
			  
			  else // ballH == 1, ball travelling to right
			  {
				  //if it's not at the left of board, keep incrementing
				  if(ballX < 6) ballX++;
				  else if(ballX == 6){ 
					  //if paddle hit corner
					  if((ballY == lPadY-1) || (ballY == lPadY+1)){
					  	ballH = 0;
					       	ballV = 1-ballV;	
					  	ballX--;
						speedChange = 5;
				  		tasks[2].period = 100;
						sound = 1;
					  }
					  //hit center
					  else if(ballY == lPadY){
						  ballH = 0;
						  ballX--;
						  sound = 1;
					  }
						/*
						//accel on hit corner
						if(((ballV == 1) && (ballY == lPadY)) || ((ballV == 0) && (ballY == lPadY+1))){
							speedChange == 20;
				  			tasks[2].period = 100;
						}
						*/						
					  //if you miss
					  else{
						  state = BALL_stop;
						  missL = 1;
						  ballX++;
					  }
				  }
				  //if(ballX != 7) ballX++;
				  //else{ ballH = 0; ballX--;}
			  }

			  // Vertical bouncing
			  if(ballV == 0){//if ball going "up"
				  //if(curveBall = 0){//if paddle not moving
				  	if(ballY != 0) ballY--; //if not hit bottom
				  	else{ ballV = 1; ballY++;}
				 //}
				 // else{//if paddle moving
				 // 	if(ballY >= 2) ballY = ballY -2; //
				 // 	else{ ballV = 1; ballY++;}
				 // }
			  }
			  else{//ball going up
				  //if(curveBall = 0){
				  	if(ballY != 4) ballY++; //if not hit top
				  	else{ ballV = 0; ballY--;}
				  //}
				  //else{
				  //	if(ballY <= 3) ballY = ballY + 2; //if not hit top
				  //	else{ ballV = 0; ballY--;}
				  //}
			  }
			  break;
		case BALL_stop:
			if(tmpD == 0x04){
				ballX = 0x03;
				ballY = 0x02;
				state = BALL_run;
			}
			else
				state = BALL_stop;
			  break;
		default:
			  break;
	}
	// Return state
	return state;
}


int TickFct_FRQ(int state) {
  switch(state) { // Transitions
     case FRQ_SMStart: // Initial transition
        state = FRQ_s0;
        break;
     case FRQ_s0:
	if(sound == 0)
		state = FRQ_s0;
	else if(sound == 0x01)
		state = FRQ_1;
	else if(sound == 0x02)
		state = FRQ_2;
	break;
     case FRQ_1:
		sound = 0;
		state = FRQ_s0;
	break;
     case FRQ_2:
		sound = 0;
		state = FRQ_s0;
	break;
     default:
        state = FRQ_SMStart;
   } // Transitions

  switch(state) { // State actions
     case FRQ_s0:
	set_PWM(0);
        break;
     case FRQ_1:
	set_PWM(700.03);
        break;
     case FRQ_2:
	set_PWM(293.66);
        break;
     default:
        break;
  } // State actions
  return state;
}
