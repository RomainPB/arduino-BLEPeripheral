/*
* ASMEDebug.h
*
* Created: 11/12/2015 9:21:33 PM
* Author: smkk
*/

#include "ASMEDebug.h"
#include <string.h>

#define MAXLOG    1024

static uint8_t  rxlog[MAXLOG] = {};
static uint16_t rxlog_len = 0;
static uint8_t  txlog[MAXLOG] = {};
static uint16_t txlog_len = 0;

void rx_buffer_log_add(uint8_t data) 
{

    if (!rxlog_len) memset(rxlog, 0xaa, MAXLOG-1);

    // Buffering for further debugging
    if (rxlog_len < MAXLOG) {
        rxlog[rxlog_len++] = data;
    }
}

void tx_buffer_log_add(uint8_t data) 
{
    // Buffering for further debugging

    if (!txlog_len) memset(txlog, 0xaa, MAXLOG-1);

    if (txlog_len < MAXLOG) {
        txlog[txlog_len++] = data;
    }
}
