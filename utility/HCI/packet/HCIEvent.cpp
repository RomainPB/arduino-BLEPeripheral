#include "HCIEvent.h"
#include "..\..\ASME\ASMEDebug.h"
#include "..\GAPcc2541.h"
#include "..\HCIProcessAnswer.h"

uint16_t paramCounter;
static bool initDone = false;

#define GAPCODE(ogf, ocf) (ocf | ogf << 8)



bool HCIEvent::waitingCmdAnswer(void)
{
    volatile uint16_t gapCode = GAPCODE(pa->getHcidata(GAP_CMD_OPCODE_POS+1),
                                        pa->getHcidata(GAP_CMD_OPCODE_POS));

    switch (gapCode) {
        case GAP_ADV_DATA_UPD_CMD:
        case GAP_MAKE_DISCOVERABLE_CMD:
        case GAP_DEVICE_INIT_CMD:
        return true;
        default:
        return false;
    }
}

bool HCIEvent :: processVendorSpecificEvent(void) {
    bool done = false;

    
    if (pa->reachGAPCode() ) {
        done = false;
        } else {
        paramCounter++;
    }
    
    // when all the params are received, exit with true
    if  (paramCounter == pa->getHcidata(HCI_MESSAGE_LEN_POS)) {

        dbgPrint(F("processVendorSpecificEvent SUCCESS"));
        
        uint16_t gapCode = GAPCODE(pa->getHcidata(HCI_GAP_CODE_POS+1), pa->getHcidata(HCI_GAP_CODE_POS));
        
        switch(gapCode) {
            case GAP_CommandStatus:
            if (pa->getHcidata(GAP_ERROR_CODE_POS) == GAP_ERR_SUCCESS) {
                if (waitingCmdAnswer() == true) {
                    messageProcessed = partial;
                    } else {
                    messageProcessed = noError;
                }

                } else {
                messageProcessed = error;
            }
            break;
            
            case GAP_DeviceInitDone:
            if (pa->getHcidata(GAP_ERROR_CODE_POS) == GAP_ERR_SUCCESS) {
                messageProcessed = noError;
            } else
            messageProcessed = error;
            break;
            
            case GAP_MakeDiscoverableDone:
            if (pa->getHcidata(GAP_ERROR_CODE_POS) == GAP_ERR_SUCCESS) {
                messageProcessed = noError;
            } else
            messageProcessed = error;
            break;
            
            case GAP_AdvertDataUpdateDone:
            if (pa->getHcidata(GAP_ERROR_CODE_POS) == GAP_ERR_SUCCESS) {
                messageProcessed = noError;
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

bool HCIEvent :: processCommandStatus(void) {
    bool done = false;

    if (pa->reachSpecificLen((2+(pa->getHcidata(HCI_MESSAGE_LEN_POS))))) {
        done = true;
    }

    if (pa->getHcidata(HCI_MESSAGE_LEN_POS) && pa->getHcidata(HCI_GAP_CODE_POS)) {
        if (!initDone) {
            initDone = true;
        } else {
            messageProcessed=noError;
        }
        dbgPrint(F("SUCCESS"));
        } else {
        dbgPrint(F("FAILED"));
    }
    return done;
}

bool HCIEvent :: processCommandComplete(void) {
    uint16_t i = 0;
    bool done = false;
    
    if (pa->lessThanPosition(HCI_COMMAND_COMPLETE_CODE_POS)) {
        done = false;
    } else {
        paramCounter++;
    }
    

    // when all the params are received, exit with true
    if  (paramCounter == pa->getHcidata(HCI_MESSAGE_LEN_POS)) {
        
        messageProcessed=noError;

    }
    return done;
}


bool HCIEvent :: process(void)
{
    uint16_t i = 0;
    bool done = false;
    
    if (pa->lessThanPosition(HCI_MESSAGE_LEN_POS)) {
        return done;
        } else if (pa->reachMessageLen() ) {
        // reach the paramLenField.
        //The next data will be the first param
        paramCounter=1;
    }

    switch (pa->getHcidata(HCI_EVENT_TYPE_POS)) {
        
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
