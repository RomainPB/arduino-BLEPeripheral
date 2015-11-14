#include "HCIProcessAnswer.h"
#include "..\ASME\ASMEDebug.h"
#include "GAPcc2541.h"

#define GAPCODE(ogf, ocf) (ocf | ogf << 8)

typedef enum {
    noError,
    started,
    partial,
    error
}messageProcessedE;

messageProcessedE messageProcessed;
uint16_t paramCounter;
static bool initDone = false;

HCIProcessAnswer :: HCIProcessAnswer(HciUart *_bleUart, HciDispatchPool *_txPool)
{
    this->bleUart = _bleUart;
    this->txPool = _txPool;
}

bool HCIProcessAnswer :: readRxChar(void) {
   return bleUart->readChar();
}

bool HCIProcessAnswer :: processVendorSpecificEvent(void) {
    bool done = false;

    
    if (bleUart->reachGAPCode() ) {
        done = false;
        } else {
        paramCounter++;
    }
    
    // when all the params are received, exit with true
    if  (paramCounter == bleUart->getHcidata(HCI_MESSAGE_LEN_POS)) {


        dbgPrint(F("processVendorSpecificEvent SUCCESS"));
        
        uint16_t gapCode = GAPCODE(bleUart->getHcidata(HCI_GAP_CODE_POS+1), bleUart->getHcidata(HCI_GAP_CODE_POS));
        
        switch(gapCode) {
            case GAP_CommandStatus:
            if (bleUart->getHcidata(GAP_ERROR_CODE_POS) == GAP_ERR_SUCCESS)
            messageProcessed = partial;
            else
            messageProcessed = error;

            break;
            
            case GAP_DeviceInitDone:
            if (bleUart->getHcidata(GAP_ERROR_CODE_POS) == GAP_ERR_SUCCESS) {
                messageProcessed = noError;
                debug++;
                memset(debugArray, 0xA5, 255);
                txPool->sendNextMsg();
            } else
            messageProcessed = error;
            break;
            
            default:
            break;
        }
        done = true;
    }

    return done;
}

bool HCIProcessAnswer :: processCommandStatus(void) {
    bool done = false;

    if (bleUart->reachSpecificLen((2+(bleUart->getHcidata(HCI_MESSAGE_LEN_POS))))) {
        done = true;
    }

    if (bleUart->getHcidata(HCI_MESSAGE_LEN_POS) && bleUart->getHcidata(HCI_GAP_CODE_POS)) {
        if (!initDone) {
            initDone = true;
            } else {
            txPool->sendNextMsg();
        }
        dbgPrint(F("SUCCESS"));
        } else {
        dbgPrint(F("FAILED"));
    }
    return done;
}

bool HCIProcessAnswer :: processCommandComplete(void) {
    uint16_t i = 0;
    bool done = false;
    
    if (bleUart->lessThanPosition(HCI_COMMAND_COMPLETE_CODE_POS)) {
        done = false;
        } else {
        paramCounter++;
    }
    

    // when all the params are received, exit with true
    if  (paramCounter == bleUart->getHcidata(HCI_MESSAGE_LEN_POS)) {
        
        txPool->sendNextMsg();

    }
    return done;
}

bool HCIProcessAnswer :: processEvent()
{
    uint16_t i = 0;
    bool done = false;
    
    if (bleUart->lessThanPosition(HCI_MESSAGE_LEN_POS)) {
        return done;
        } else if (bleUart->reachMessageLen() ) {
        paramCounter=0; // reach the paramLenField, reset Counter
    }


    switch (bleUart->getHcidata(HCI_EVENT_TYPE_POS)) {
        
        case Ev_Vendor_Specific:  // Vendor specific Event Opcode
        done = processVendorSpecificEvent();
        break;

        case EV_Command_Status:  // Command Status
        done = processCommandStatus();
        break;
        
        case EV_Command_Complete:  // Command Complete

        done = processCommandComplete();
        break;
        case EV_LE_Event_Code:     // LE Event Opcode
        default:
        break;
        
    }
    return done;
}


bool HCIProcessAnswer :: process_rx_msg_data() {
    uint16_t i = 0;
    bool done = false;
    uint8_t value = bleUart->getHcidata(HCI_PACKET_TYPE_POS);
    switch (value) {
        case HCI_EVENT:  // Event
        done = processEvent();
        break;
        
        default:
        break;
    }

    if (done) {
        bleUart->rx_msg_clean();
    }
    
    return (messageProcessed == noError);
    
}