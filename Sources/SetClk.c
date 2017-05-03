#include "derivative.h"

#define     LOCK         0x08
#define     PLLSEL       0x80
// -------------------------------------------------------------------
// This function enables PLL and use a 4-MHz crystal oscillator to
// generate 24-MHz E clock.
// -------------------------------------------------------------------
void SetClk4 (void) {
    SYNR    = 0x05; // set SYSCLK to 24 MHz from a 4-MHz oscillator
    REFDV   = 0;    //  " 
    PLLCTL  = 0x60; // turn on PLL, set automatic 
    while(!(CRGFLG & LOCK));
    CLKSEL  |= PLLSEL; // clock derived from PLL
}
// -------------------------------------------------------------------
// This function enables PLL and use an 8-MHz crystal oscillator to
// generate 24-MHz E clock.
// -------------------------------------------------------------------
void SetClk8 (void) {
    SYNR    = 0x02; // set SYSCLK to 24 MHz from a 4-MHz oscillator
    REFDV   = 0;    //  " 
    PLLCTL  = 0x60; // turn on PLL, set automatic 
    while(!(CRGFLG & LOCK));
    CLKSEL  |= PLLSEL; // clock derived from PLL
}