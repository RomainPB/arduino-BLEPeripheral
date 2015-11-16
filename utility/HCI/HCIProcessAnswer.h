#ifndef __HCI_PROCESS_ANSWER
#define __HCI_PROCESS_ANSWER

#include "..\ASME\HciUart.h"
#include "..\ASME\HciDispatchPool.h"


class HCIProcessAnswer
{
    //variables
    public:
    protected:
    private:
    rx_buffer_t rx_buffer;
    HciUart *bleUart;
    HciDispatchPool *txPool;
    
    //functions
    public:
    HCIProcessAnswer(HciUart *_bleUart, HciDispatchPool *_txPool);       
    ~HCIProcessAnswer();
    bool processVendorSpecificEvent(void);
    bool processCommandStatus(void);
    bool processCommandComplete(void);
    bool processEvent(void);
    bool process_rx_msg_data(void);
    bool readRxChar(void);
   
    
    protected:
    private:
    HCIProcessAnswer(void); // protect unwanted default constructor

    HCIProcessAnswer( const HCIProcessAnswer &c );
    HCIProcessAnswer& operator=( const HCIProcessAnswer &c );
    bool store_rx_msg_data(uint8_t data);
    bool waitingCmdAnswer(void);

}; //HCIProcessAnswer

#endif // __HCI_PROCESS_ANSWER
