#ifndef __HCI_PROCESS_ANSWER
#define __HCI_PROCESS_ANSWER

#include "..\ASME\HciUart.h"
#include "packet\HCIPacket.h"


class HCIProcessAnswer
{
    //variables
    public:
    protected:
    private:
    rx_buffer_t rx_buffer;
    HciUart *bleUart;
    HCIPacket *hciPackets[4];
    
    //functions
    public:
    HCIProcessAnswer(HciUart *_bleUart);       
    ~HCIProcessAnswer();
    inline bool reachMessageLen(void) {
        return (this->bleUart->reachMessageLen());
    }

    inline uint8_t getHcidata(uint8_t hciPos) {
        return this->bleUart->getHcidata(hciPos);
    }
    
    inline bool reachGAPCode(void){
        return this->bleUart->reachGAPCode();
    }
    
    inline bool reachSpecificLen(uint8_t testLen){
        return this->bleUart->reachSpecificLen(testLen);
    }
    
    inline bool lessThanPosition(uint8_t hciPos) {
        return this->bleUart->lessThanPosition(hciPos);
    }
    
    bool process_rx_msg_data(void);
    bool readRxChar(void);
   
    
    protected:
    private:
    HCIProcessAnswer(void); // protect unwanted default constructor

    HCIProcessAnswer( const HCIProcessAnswer &c );
    HCIProcessAnswer& operator=( const HCIProcessAnswer &c );
    bool store_rx_msg_data(uint8_t data);

}; //HCIProcessAnswer

#endif // __HCI_PROCESS_ANSWER
