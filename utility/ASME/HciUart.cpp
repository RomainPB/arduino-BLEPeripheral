/*
* HciUart.cpp
*
* Created: 11/12/2015 9:21:33 PM
* Author: smkk
*/
#include "HciUart.h"
#include "ASMEDebug.h"

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
    if (hasChar){
        store_rx_msg_data(BLE.read());
    }
    
    return hasChar;
}

void HciUart :: rx_msg_clean(void) {
    dbgPrintln(rx_buffer.data);
    dbgPrintln(F("Cleaning buffer ..."));
    memset(rx_buffer.data, 0xa5, RX_BUFFER_MAX_LEN);
    rx_buffer.len=0;
    // rx_buffer.paramCounter=0;   da definire come e dove mettere
}

void HciUart :: sendMsg(uint8_t *buf ,uint8_t len) {
    //rx_buffer.messageProcessed = started;   da definire come e dove mettere
    rx_msg_clean();
    BLE.write(buf, len);
    for (int i = 0; i < len; ++i) {
        tx_buffer_log_add(buf[i]);
    }
}


bool HciUart :: reachMessageLen(void) {
    return (rx_buffer.len-1 <= HCI_MESSAGE_LEN_POS); 
}

    uint8_t HciUart :: getHcidata(uint8_t hciPos) {
        return rx_buffer.data[hciPos];
    }
    
    bool HciUart :: reachGAPCode(void){
        return (rx_buffer.len-1 <= HCI_GAP_CODE_POS);
    }
    
    bool reachMessageLen(void) ;
    
    bool HciUart :: reachSpecificLen(uint8_t testLen){
        return (rx_buffer.len == testLen);
    }
    
    bool HciUart :: lessThanPosition(uint8_t hciPos) {
        return (rx_buffer.len <= hciPos);
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