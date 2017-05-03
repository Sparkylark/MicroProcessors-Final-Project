/*********************************************************************/
/* Author: Dr. Han-Way Huang                                         */
/* Date: 01/31/2005                                                  */
/* Organization: Minnesota State University, Mankato                 */
/*********************************************************************/
/* The following function creates a time delay which is equal to the */
/* multiple of 50 us. The value passed in Y specifies the number of  */
/* milliseconds to be delayed.                                       */
/*********************************************************************/
#include "derivative.h"
#define TFFCA 0x10
#define MCZF  0x80

void delayby10us(int k)
{
     int i;
     TSCR1 |= TFFCA;    // enable fast timer flag clear
     for (i = 0; i < k; i++) {
         MCCTL = 0x04;  // enable modulus down counter with 1:1 as prescaler
         MCCNT = 240;   // let modulus down counter count down from 240
         while(!(MCFLG & MCZF));
         MCCTL &= ~0x04;// disable modulus down counter
     }
}
void delayby50us(int k)
{
     int i;
     TSCR1 |= TFFCA;    // enable fast timer flag clear
     for (i = 0; i < k; i++) {
         MCCTL = 0x04;  // enable modulus down counter with 1:1 as prescaler
         MCCNT = 1200;  // let modulus down counter count down from 1200
         while(!(MCFLG & MCZF));
         MCCTL &= ~0x04;// disable modulus down counter
     }
}
/**********************************************************************/
/* The following function creates a time delay which is equal to the  */
/* multiple of 1 ms. The value passed in Y specifies the number of one*/
/* milliseconds to bdelayed.                                          */
/**********************************************************************/
void delayby1ms(int k)
{
     int i;
     TSCR1 |= TFFCA;    // enable fast timer flag clear
     for (i = 0; i < k; i++) {
         MCCTL = 0x07;  // enable modulus down counter with 1:16 as prescaler
         MCCNT = 1500;  // let modulus down counter count down from 1500
         while(!(MCFLG & MCZF));
         MCCTL &= ~0x04;// disable modulus down counter
     }
}
/**********************************************************************/
/* The following function creates a time delay which is equal to the  */
/* multiple of 10 ms. The value passed in Y specifies the number of   */
/* one milliseconds to be delayed.                                    */
/**********************************************************************/
void delayby10ms(int k)
{
     int i;
     TSCR1 |= TFFCA;    // enable fast timer flag clear
     for (i = 0; i < k; i++) {
         MCCTL = 0x07;  // enable modulus down counter with 1:16 as prescaler
         MCCNT = 15000; // let modulus down counter count down from 15000
         while(!(MCFLG & MCZF));
         MCCTL &= ~0x04;// disable modulus down counter
     }
}
/**********************************************************************/
/* The following function creates a time delay which is equal to the  */
/* multiple of 100 ms. The value passed in Y specifies the number of  */
/* 100 milliseconds to be delayed.                                    */
/**********************************************************************/
void delayby100ms(int k)
{
     int j, i;
     TSCR1 |= TFFCA;            // enable fast timer flag clear
     for (j = 0; j < k; j++) {
         for (i = 0; i < 10; i++) {
             MCCTL = 0x07;      // enable modulus down counter with 1:16 as prescaler
             MCCNT = 15000;     // let modulus down counter count down from 15000
             while(!(MCFLG & MCZF));
             MCCTL &= ~0x04;    // disable modulus down counter
         }
     }
}
