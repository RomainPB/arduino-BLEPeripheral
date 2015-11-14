
/*
* ASMEDebug.h
*
* Created: 11/12/2015 9:21:33 PM
* Author: smkk
*/
#ifndef __ASME_DEBUG_H__
#define __ASME_DEBUG_H__


#include <stdint-gcc.h>

extern uint8_t debug, debug2;
extern uint8_t debugArray[255];

#define dbgPrint(x) Serial1.println(x)
#define dbgPrintln(x) Serial1.println(x)

#endif