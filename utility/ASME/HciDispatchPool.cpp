/* 
* HciDispatchPool.cpp
*
* Created: 11/12/2015 1:01:23 PM
* Author: smkk
*/
#include "HciDispatchPool.h"
#include <string.h>
// default destructor
HciDispatchPool::~HciDispatchPool()
{
       
} //~HciDispatchPool

HciDispatchPool::HciDispatchPool(HciUart *_bleUart)
{
   this->bleUart = _bleUart;
   uint16_t structSize = sizeof(hci_msg_t);
    memset(hci_msg, 0, structSize*MAX_HCI_CALLBACKS);
    msg_num=0;       
}

int HciDispatchPool::sendNextMsg(void) {
    
    if (!msg_num) {
        return 0;
    }

    for (int i = 0 ; (i < MAX_HCI_CALLBACKS) ; ++i) {
        if (hci_msg[i].len) {
            if (bleUart) 
                bleUart->sendMsg(hci_msg[i].buf, hci_msg[i].len);
            /*BLE.write(hci_msg[i].buf, hci_msg[i].len);*/
            delMsg(i);
                        
            break;
        }
    }
    return 0;
}


int HciDispatchPool::addMsg(uint8_t *msg, uint16_t len) {
    
    uint16_t counter = 0;
    uint16_t i = 0;
    

    if (len >= MAX_HCI_MSG_SIZE) {
        return -1;
    }

    if (msg_num) {

        while (counter != msg_num) {
            if (i >= MAX_HCI_CALLBACKS)
            return -1;

            if (hci_msg[i].len) {
                counter++;
            }
            i++;
        }
    }

    memcpy(hci_msg[i].buf, msg, len);
    hci_msg[i].len = len;
    msg_num++;
    
    return 0;
}

int HciDispatchPool::delMsg(uint16_t index) {
    
    if (index >=MAX_HCI_CALLBACKS) {
        return -1;
    }

    if (hci_msg[index].len) {
        memset(hci_msg[index].buf, 0, MAX_HCI_MSG_SIZE);
        hci_msg[index].len = 0;
        msg_num--;
    }

    return 0;
}