
/*
* ASMEDebug.h
*
* Created: 11/12/2015 9:21:33 PM
* Author: smkk
*/
#ifndef __ASME_DEBUG_H__
#define __ASME_DEBUG_H__

#include <stdint-gcc.h>

#define dbgPrint(x) Serial1.println(x)
#define dbgPrintln(x) Serial1.println(x)

void rx_buffer_log_add(uint8_t data);
void tx_buffer_log_add(uint8_t data);

#endif
