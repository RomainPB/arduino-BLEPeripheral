#include "HCIProcessAnswer.h"
#include "..\ASME\ASMEDebug.h"
#include "GAPcc2541.h"
#include "packet\HCICommand.h"
#include "packet\HCIEvent.h"
#include "packet\HCISpecificEvent.h"

HCIProcessAnswer :: HCIProcessAnswer(HciUart *_bleUart)
{
    hciPackets[0] = new HCICommand(this);
    hciPackets[3] = new HCIEvent(this);
    this->bleUart = _bleUart;
}

bool HCIProcessAnswer :: readRxChar(void) {
    return bleUart->readChar();
}

bool HCIProcessAnswer :: process_rx_msg_data() {
    uint16_t i = 0;
    bool done = false;
    uint8_t value = bleUart->getHcidata(HCI_PACKET_TYPE_POS);
    done = hciPackets[value-1]->process(); // on the array the position of the class is one less of the actual HCI code
//     switch (value) {
//         case HCI_EVENT:  // Event
//         done = processEvent();
//         break;
//         
//         default:
//         break;
//     }

    if (done) {
        bleUart->rx_msg_clean();
    }
    
    done = hciPackets[value-1]->isProcessed();
    
    if (done) {
        hciPackets[value-1]->resetAnswerCompleted();//reset FSM
    }
    
    return done;
}
