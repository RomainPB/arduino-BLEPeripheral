/* 
* HciUart.h
*
* Created: 11/12/2015 9:21:33 PM
* Author: smkk
*/
#ifndef __HCIUART_H__
#define __HCIUART_H__

#include "Arduino.h"
#include "utility\HCI\hci.h"

#define RX_BUFFER_MAX_LEN  255

typedef struct {
    char data[RX_BUFFER_MAX_LEN];
    uint16_t len;
    bool complete;
} rx_buffer_t;


class HciUart
{
//variables
public:
protected:
private:
    rx_buffer_t rx_buffer;

//functions
public:
	HciUart();
	~HciUart();
    void rx_msg_clean(void);
    void sendMsg(uint8_t *buf ,uint8_t len);
    bool readChar(void);
    
    uint8_t getHcidata(uint8_t hciPos);
    
    bool reachGAPCode(void);
       
    bool reachMessageLen(void) ;
    
    bool reachSpecificLen(uint8_t testLen);
    
    bool lessThanPosition(uint8_t hciPos);
    
protected:
private:
	HciUart( const HciUart &c );
	HciUart& operator=( const HciUart &c );
    bool store_rx_msg_data(uint8_t data);

}; //HciUart

#endif //__HCIUART_H__
