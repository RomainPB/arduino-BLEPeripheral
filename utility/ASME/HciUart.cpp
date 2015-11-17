/*
* HciUart.cpp
*
* Created: 11/12/2015 9:21:33 PM
* Author: smkk
*/
#include "HciUart.h"
#include "ASMEDebug.h"

// default constructor
HciUart::HciUart()
{
    BLE.begin(115200);
    BLE.flush();
} //HciUart

// default destructor
HciUart::~HciUart()
{
} //~HciUart

bool HciUart :: readChar(void){
    bool hasChar = BLE.available();
    uint8_t data;
    if (hasChar){
        data  = BLE.read();        
        store_rx_msg_data(data);
    }
    
    return hasChar;
}

void HciUart :: rx_msg_clean(void) {
    dbgPrintln(rx_buffer.data);
    dbgPrintln(F("Cleaning buffer ..."));
    memset(rx_buffer.data, 0xa5, RX_BUFFER_MAX_LEN);
    rx_buffer.len=0;
}

void HciUart :: sendMsg(uint8_t *buf ,uint8_t len) {
    
    rx_msg_clean();    
    BLE.write(buf, len);
    
    for (int i = 0; i < len; ++i) {
        tx_buffer_log_add(buf[i]);
    }
}

// PRIVATE
bool HciUart :: store_rx_msg_data(uint8_t data) {
    if (rx_buffer.len < (RX_BUFFER_MAX_LEN-1)) {           
        rx_buffer.data[rx_buffer.len] = data;
        rx_buffer.len++;

        rx_buffer_log_add(data);
        } else {
        rx_msg_clean();
    }
}
