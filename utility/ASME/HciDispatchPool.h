#include <stdint-gcc.h>
/* 
* HciDispatchPool.h
*
* Created: 11/12/2015 1:01:23 PM
* Author: smkk
*/


#ifndef __HCIDISPATCHPOOL_H__
#define __HCIDISPATCHPOOL_H__
#include "HciUart.h"

#define MAX_HCI_CALLBACKS  64
#define MAX_HCI_MSG_SIZE  255


class HciDispatchPool
{
//variables
public:

protected:
typedef struct msg_t_ {
    uint8_t  buf[MAX_HCI_MSG_SIZE];
    uint16_t len;
} hci_msg_t;

private:
    hci_msg_t hci_msg[MAX_HCI_CALLBACKS];
    uint16_t msg_num;    
    HciUart *bleUart;

//functions
public:
	HciDispatchPool(){};
    HciDispatchPool(HciUart *_bleUart);
	~HciDispatchPool();
        int addMsg(uint8_t *msg, uint16_t len);
        int sendNextMsg(void );
        
protected:
private:
    int delMsg(uint16_t index);
	HciDispatchPool( const HciDispatchPool &c );
	HciDispatchPool& operator=( const HciDispatchPool &c );

}; //HciDispatchPool

#endif //__HCIDISPATCHPOOL_H__
