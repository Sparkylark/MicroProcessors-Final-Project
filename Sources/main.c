#include <hidef.h>      /* common defines and macros */
#include "derivative.h"      /* derivative-specific definitions */
#include "SetClk.h"
#include "lcdUtil.h"


// Values used to sum the ATD things 
short i;
int values[8];
int sum;

// char aray to store user input
char input_buf[10];                                
int user_input_length;
enum INPUT_STATE {
  MINUS_OR_FIRST,
  FIRST,
  SECOND_OR_POINT,
  POINT,
  LAST,
  WAIT
} input_state; 
int back_to_point = 0; // Flag for LAST return
char displayAngle[4] = {0x20,0x30,0x30,0x30}; 

int ADCout = 972; //output of the ADC conversion interrupt
  //Cleaned by SPI and changed from (-90.0, 90.0) to (0, 1801)
int scanDuty = 1659; //current Duty of Scan Mode
int scanDir = 1;  //direction the servo should rotate towards (-1 or 1)
const int scanChoices[5] = {34749, 25607, 17374, 8687, 2896};
int isSecondInterrupt = 0;
//The possible timer comparator increments for all 5 possible servo scan speeds
int currentScanChoice = 4;
int sysMode = 0;
//0 = Rest; 1 = Potentiometer; 2 = Phototransistor; 3 = AutoScan; 4 = Manual Serial
int buttonTimeCount = 0;
int angleSign = 1;  // Indicates the sign of a manually entered angle
int effectiveAngle = 0;
int unsignedAngle = 0;  //Value that ranges from 0 to 900 for angle displaying to LCD
int effectiveSign = 1; // Indicates sign of currently-set manual angle
int readAngle = 0;
int shiftedAngle = 900;
int sci_count = 0;

unsigned int scaleProduct;

void openAD0(void); // Opens ATD0 and begins scanning light sensor (AN7)
void openSCI0(void);
void putch(char cx);
void putstr(char *cptr);
void newline(void);

void main(void) {
  // Turn off 7-segs and enable LEDs
  DDRP |= 0x0F;
  DDRB |= 0xFF;
  DDRJ |= 0x02;
  PTP |= 0x0F;
  PTJ &= 0xFD;
  
  SetClk8();
  openAD0();
  openSCI0();
  openLCD();
  putsLCD("   Rest Mode    ");
  cmd2LCD(0xC2);
  putsLCD("Angle:    .");
  DDRH &= 0xFE;   //Set PH0 (SW5) to be an input
  PPSH = 0x00;    //Falling-Edge Polarity for PH
  PIEH = 0x01;    //Enable PH0 interrupt
  
  PWMPOL = PWMPOL_PPOL5_MASK; //Use high-to-low polarity
  PWMCLK = 0x00;  //Use Clock A for PWM Channel 5
  PWMPRCLK = 0x03;  //Set prescale of 8 for Clock A
  PWMCTL |= PWMCTL_CON45_MASK;  //Use Channels 4 and 5 in 16-bit mode
  PWMPER45 = 60000; //Set Period of PWM to 20ms
  PWMDTY45 = 4500;  //Set Duty of PWM to 1.5 ms initially (0 degrees)
  //***FOR SERVO: MIN DUTY = 1659, MAX DUTY = 6839***
  //***RANGE = 5901***
  // 5180
  PWME = PWME_PWME5_MASK; //Enable PWM Channel 5 (PP5)
  
  TSCR1 = 0x90;   //Set TEN and TFFCA
  TSCR2 = 0x02;   //Prescale Factor = 4
  TIOS = 0x60;    //Use Timers 5 and 6 for output compare
  TCTL1 = 0x04;   //Set PT5 to not toggle upon successful compare
  TIE = 0x60;     //Enable OC6 and OC5 interrupts
	asm("cli");
	
	while(1){
	  if(sysMode == 0) {
	    PWMDTY45 = 4249;
	    displayAngle[0] = 0x20;
	    displayAngle[1] = 0x30;
	    displayAngle[2] = 0x30;
	    displayAngle[3] = 0x30;
	  }else if(sysMode == 1 || sysMode == 2){ //Scale ADC output to correct duty cycle range
      //ADC ranges from 0 to 1023; PWM ranges from 1659 to 7560
	    //Achieve approximate scaling of 5.76 to obtain correct PWM range
	    //Must not ever have a product higher than 65536, so precision is limited
	    scaleProduct = ADCout * 5;     
	    PWMDTY45 = (scaleProduct + 1659); //Update PWM's Duty based on newest data from ADC
	  }else if(sysMode == 4){
	    //Serial degree input ranges from 0 to 1800; PWM ranges from 1659 to 6839
	    //Achieve approximate scaling of 2.88 to obtain correct PWM range 
	    scaleProduct = (effectiveAngle + 900) * 29;     
	    scaleProduct = scaleProduct / 10;
	    PWMDTY45 = (scaleProduct + 1659); //Update PWM's Duty based on translated degree data from Terminal
	    unsignedAngle = effectiveAngle * effectiveSign;
	    if (effectiveSign == 1)
	      displayAngle[0] = 0x20;
	    else
	      displayAngle[0] = 0x2D; 

	  }
	  if(sysMode > 0 && sysMode < 4){ //Generate numeric characters for displaying angle on the LCD
	    scaleProduct = (PWMDTY45 - 1659) * 10;
	    unsignedAngle = (scaleProduct / 29) - 900;
	    if(unsignedAngle < 0){ //Enforce Positive Angle
	      displayAngle[0] = 0x2D;
	      unsignedAngle = unsignedAngle * -1;
	    } else displayAngle[0] = 0x20;
	       
	  }
	  if ( sysMode != 0 ) {
	    displayAngle[1] = ((unsignedAngle) / 100) + 0x30;
	    displayAngle[2] = ((unsignedAngle - 100 * (unsignedAngle / 100)) / 10) + 0x30;
	    displayAngle[3] = (unsignedAngle % 10) + 0x30;
	  }
	  cmd2LCD(0xC9);            //Update Angle Display on LCD
	  putcLCD(displayAngle[0]);
    putcLCD(displayAngle[1]);
    putcLCD(displayAngle[2]);
    putcLCD('.');
    putcLCD(displayAngle[3]);
    putsLCD("  ");
	}
}

/* SCAN MODE CALCULATIONS - TIMER CLOCK = 6 MHz
Duty incremented EVERY OTHER INTERRUPT
1)60 sec: 360M tmrClk - 34749 clocks per Interrupt
2)45 sec: 270M tmrClk - 25607
3)30 sec: 180M tmrClk - 17374
4)15 sec:  90M tmrClk - 8687
5)5  sec:  30M tmrClk - 2896
*/

void openAD0(void) { 
  ATD0CTL2 = 0xE2;
  // delayby10us(2); If something doesn't work, implement the delay
  ATD0CTL3 = 0x00;
  ATD0CTL4 = 0x25;
  ATD0CTL5 = 0xA4;
}

void interrupt 14 ScanTimerHandler(){
  TC6 += scanChoices[currentScanChoice];
  
  //the following section acts as a debouncer for SW5 and SCI inputs
  if(buttonTimeCount>0) buttonTimeCount--; 
  if(sci_count>0) sci_count--;
  
  else if(sysMode == 3){ //if in scan mode after debouncing the button
    if(isSecondInterrupt == 1) {
      scanDuty += scanDir;  //increment or decrement scanDuty
      isSecondInterrupt = 0;
    }else isSecondInterrupt = 1;
    if(scanDuty < 1660 && scanDir == -1)
      scanDir = 1;
    else if(scanDuty > 6838 && scanDir == 1)
      scanDir = -1;
    PWMDTY45 = scanDuty;  //place new duty cycle in PWM
  }
}

/*TONE CALCULATIONS
Min Tone: 750 Hz - 4000 clocks
Max Tone: 1250 Hz - 2400 clocks
Range = 1600 clocks
Range of PWM - 5180 
Need 0.3089 scale of PWM Duty
*/

void interrupt 13 ToneTimerHandler(){
  unsigned int timerProduct = PWMDTY45 - 1659;
  timerProduct = timerProduct * 8;
  timerProduct = timerProduct / 26; 
  TC5 += 4000 - timerProduct; 
}

void interrupt 25 PH0Handler(){
  PIFH = 0x01;
  if(buttonTimeCount==0){
    buttonTimeCount = 50; //Force 20 Timer 6 interrupts (~200 ms) to pass between mode changes
    if(sysMode == 0){       //0 = Rest
      sysMode = 1;
      putstr("Potentiometer Source"); 
      newline();
      cmd2LCD(0x80);
      putsLCD(" Potentiometer  ");
      ATD0CTL5 = 0xA7;  //ADC uses potentiometer source     
    }else if(sysMode == 1){ //1 = Potentiometer                      
      sysMode = 2;
      putstr("Phototransistor Source");
      newline();
      cmd2LCD(0x80);
      putsLCD("Phototransistor ");
      ATD0CTL5 = 0xA4;  //ADC uses phototransistor source  
    }else if(sysMode == 2){ //2 = Phototransistor
      sysMode = 3;
      scanDuty = 1659;
      scanDir = 1;
      putstr("AutoScan Select:");
      newline();
      putstr(" 0: 60 Second Scan ");
      newline();
      putstr(" 1: 45 Second Scan ");
      newline();
      putstr(" 2: 30 Second Scan ");
      newline();
      putstr(" 3: 15 Second Scan ");
      newline();
      putstr(" 4: 5 Second Scan ");
      newline();
      cmd2LCD(0x80);
      putsLCD("   Scan Mode    ");
      currentScanChoice = 0;  
    }else if(sysMode == 3){ //3 = AutoScan
      sysMode = 4;
      putstr("Manual Angle:");
      newline();
      putstr("   Input [Range -90.0 to 90.0] = ");
      newline();
      cmd2LCD(0x80);
      putsLCD("  Manual Entry  ");
    }else{  //4 = Manual Serial  
      putstr("Rest mode");
      newline();
      cmd2LCD(0x80);
      putsLCD("   Rest Mode    ");                   
      readAngle = 0;
      input_state = MINUS_OR_FIRST;      
      sysMode = 0;
      PWMDTY45 = 4500;       //Set Duty of PWM to 1.5 ms
    }
  }
}
/*
enum INPUT_STATE {
  MINUS_OR_FIRST,
  FIRST,
  SECOND_OR_POINT,
  POINT,
  LAST,
  WAIT
} input_state; 
*/
void interrupt 20 sci0_interrupt(void) {
  char cx = SCI0DRL;
  if(sci_count == 0){
    sci_count = 3;
    //putstr("@");
    //putch(cx);
    //newline();
    if (sysMode == 4){
      switch(input_state){
       case MINUS_OR_FIRST :
         //putstr("MINUS_OR_FIRST");
         //newline();
         if (cx == 0x2D) { //Minus
           angleSign = -1;
           input_state = FIRST;
           putch(cx);
         } else if (cx >= 0x30 && cx <= 0x39) {  //Number
           angleSign = 1;
           readAngle = 10 * (cx - 0x30);
           input_state = SECOND_OR_POINT;
           putch(cx);
         }
         break;
       case FIRST :
         //putstr("FIRST");
         //newline();
         if (cx >= 0x30 && cx <= 0x39) { //Number
           readAngle = angleSign * 10 * (cx - 0x30);
           input_state = SECOND_OR_POINT;
           putch(cx);
         } /*else if(cx == 0x08){  //Backspace
           putch(cx);
           input_state = MINUS_OR_FIRST;
         } */
         break;
       case SECOND_OR_POINT :
         //putstr("SECOND_OR_POINT");
         //newline();
         if (cx == 0x2E) {  //Point
           input_state = LAST;
           back_to_point = 0;
           putch(cx);
         } else if (cx >= 0x30 && cx <= 0x39 && !(cx > 0x30 && readAngle * angleSign == 90)) { //Number
           readAngle = readAngle * 10 + 10 * angleSign * (cx - 0x30);
           input_state = POINT;
           putch(cx);
         } else if (cx == 0x0D) { //Enter
           effectiveAngle = readAngle;
           effectiveSign = angleSign;
           input_state = MINUS_OR_FIRST;
           newline();
         } /*else if(cx == 0x08){  //Backspace
           putch(cx);
           readAngle = 0;
           if(angleSign == 1) //Got here from MINUS_OR_FIRST
             input_state = MINUS_OR_FIRST;
           else   //Got here from FIRST
             input_state = FIRST;
         } */
         break;
       case POINT :
         //putstr("POINT");
         //newline();
         if (cx == 0x2E) { //Point
           input_state = LAST;
           back_to_point = 1;
           putch(cx);
         } else if (cx == 0x0D) { //Enter
           //shiftedAngle = readAngle + 900;
           effectiveAngle = readAngle;
           effectiveSign = angleSign;
           input_state = MINUS_OR_FIRST;
           newline();
         } /*else if(cx == 0x08){  //Backspace
           putch(cx);
           readAngle = ((readAngle * angleSign) - (readAngle * angleSign) % 100) / 10 * angleSign;
           input_state = SECOND_OR_POINT;
         } */
         break;
       case LAST : 
         //putstr("LAST");
         //newline();
         if (cx >= 0x30 && cx <= 0x39 && !(cx > 0x30 && readAngle * angleSign == 900)) { //Number
           readAngle += angleSign * (cx - 0x30);
           input_state = WAIT;
           putch(cx);
         } /*else if (cx == 0x08) { //Backspace
           if (back_to_point == 1) 
             input_state = POINT;           
            else 
             input_state = SECOND_OR_POINT;          
           putch(cx);         
         } */
         break;
       case WAIT : 
         //putstr("WAIT");
         //newline();
         if(cx == 0x0D){ //Enter
           //shiftedAngle = readAngle + 900;
           effectiveAngle = readAngle;
           effectiveSign = angleSign;
           input_state = MINUS_OR_FIRST; 
           newline();
         } /*else if (cx == 0x08) { //Backspace
           putch(cx);
           readAngle = ((readAngle * angleSign) - (readAngle * angleSign) % 10) * angleSign;
           input_state = LAST;
         } */
         break;                                    
      }
    } else if (sysMode == 3) {
       if (cx >= 0x30 && cx <= 0x34){
          currentScanChoice = cx - 0x30;
          putstr("Changed scan speed to: ");            
          putch(cx);
          newline();
      }
    }
  }
  SCI0SR1 |= 0x20;
}

//Obtains the average value of 8 samples for conversion into a duty cycle
void interrupt 22 ATD_Handler(){
  values[0] = ATD0DR0;
  values[1] = ATD0DR1;
  values[2] = ATD0DR2;
  values[3] = ATD0DR3;
  values[4] = ATD0DR4;
  values[5] = ATD0DR5;
  values[6] = ATD0DR6;
  values[7] = ATD0DR7;
  sum = 0;
  for (i = 0; i < 8; i++){
    sum += values[i];
  }
  
  //Drive the LEDs based on Potentiometer or Phototransistor data
  ADCout = sum >> 3;
  if(sysMode == 1 || sysMode == 2)
    PORTB = (ADCout >> 2); 
  else PORTB = 0x00;
  
  
  
}

void openSCI0(void)
{
     SCI0BDH = 0;
     SCI0BDL = 156;
     SCI0CR1 = 0x4C;
     SCI0CR2 = 0x2C;
}

void putch(char cx)
{
    while(!(SCI0SR1 & SCI0SR1_TDRE_MASK))
       continue;
    SCI0DRL = cx;
    //return 0;
}

void putstr(char *cptr)
{
    while(*cptr) {
        putch(*cptr);
        cptr++;
    }
    //return 0;
}


void newline(void)
{
     putch(0x0D);
     putch(0x0A);
     //return 0;
}