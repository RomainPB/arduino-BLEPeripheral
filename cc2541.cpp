#if defined(NRFCC2541) || defined(ARDUINO_SAMD_SMARTEVERYTHING)

#include "BLECharacteristic.h"
#include "BLEDescriptor.h"
#include "BLERemoteService.h"
#include "BLERemoteCharacteristic.h"
#include "BLEService.h"
#include "BLEUtil.h"
#include "BLEUuid.h"

#include "utility/HCI/hci.h"
#include "utility/HCI/hci_cmds.h"
#include "utility/HCI/GAPcc2541.h"
#include "cc2541.h"

#define GAPCODE(ogf, ocf) (ocf | ogf << 8)

struct setupMsgData {
  unsigned char length;
  unsigned char cmd;
  unsigned char type;
  unsigned char offset;
  unsigned char data[28];
};

#define NB_BASE_SETUP_MESSAGES                  7
#define MAX_ATTRIBUTE_VALUE_PER_SETUP_MSG       28

#define DYNAMIC_DATA_MAX_CHUNK_SIZE             26
#define DYNAMIC_DATA_SIZE                       (DYNAMIC_DATA_MAX_CHUNK_SIZE * 6)

static bool initDone = false;
/* Having PROGMEM here caused the setup messages to be zeroed out */
static const hal_hci_data_t baseSetupMsgs[NB_BASE_SETUP_MESSAGES] /*PROGMEM*/ = {};

#define MAX_HCI_MSG_SIZE  255
#define MAX_HCI_CALLBACKS  64
typedef struct msg_t_ {
    uint8_t  buf[MAX_HCI_MSG_SIZE];
    uint16_t len;
} hci_msg_t;

class HciDispatchPool {
    hci_msg_t hci_msg[MAX_HCI_CALLBACKS];
    uint16_t msg_num;

    int delMsg(uint16_t index);
public:
    HciDispatchPool();
    int addMsg(uint8_t *msg, uint16_t len);
    int sendNextMsg(void );
};


#define RX_BUFFER_MAX_LEN  255
typedef enum {
    noError,
    started,
    partial,
    error
}messageProcessedE;

typedef struct {
    char data[RX_BUFFER_MAX_LEN];
    uint16_t len;
    bool complete;
    uint16_t paramCounter;
    messageProcessedE messageProcessed;
} rx_buffer_t;

rx_buffer_t rx_buffer;
volatile int debugMsgSent=0, debug=0;


HciDispatchPool *hci_tx_pool;
HciDispatchPool::HciDispatchPool(void) 
{
    memset(hci_msg, 0, sizeof(hci_msg_t)*MAX_HCI_CALLBACKS);
    msg_num=0;
}

int HciDispatchPool::delMsg(uint16_t index) {
    
    if ((!hci_tx_pool) || (index >=MAX_HCI_CALLBACKS)) {
        return -1;
    }

    if (hci_msg[index].len) {
        memset(hci_msg[index].buf, 0, MAX_HCI_MSG_SIZE);
        hci_msg[index].len = 0;
        msg_num--;
    }

    return 0;
}

void rx_msg_clean(void) {
    dbgPrintln(rx_buffer.data);
    dbgPrintln(F("Cleaning buffer ..."));
    memset(rx_buffer.data, 0xa5, RX_BUFFER_MAX_LEN);
    rx_buffer.len=0;
    rx_buffer.paramCounter=0;
}

void sendMsg(uint8_t *buf ,uint8_t len) {
    debugMsgSent++;
    rx_buffer.messageProcessed = started;
    rx_msg_clean();
    BLE.write(buf, len);
}

int HciDispatchPool::sendNextMsg(void) {
    
    if (!msg_num) {
        return 0;
    }

    for (int i = 0 ; (i < MAX_HCI_CALLBACKS) ; ++i) {
        if (hci_msg[i].len) {
            sendMsg(hci_msg[i].buf, hci_msg[i].len);
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
  
    if ((len >= MAX_HCI_MSG_SIZE) || !hci_tx_pool) {
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


cc2541::cc2541(unsigned char req, unsigned char rdy, unsigned char rst) :
  BLEDevice(),

  _localPipeInfo(NULL),
  _numLocalPipeInfo(0),
  _broadcastPipe(0),

  _closedPipesCleared(false),
  _remoteServicesDiscovered(false),
  _remotePipeInfo(NULL),
  _numRemotePipeInfo(0),

  _dynamicDataOffset(0),
  _dynamicDataSequenceNo(0),
  _storeDynamicData(false)
{
    hci_tx_pool = new HciDispatchPool();
}

cc2541::~cc2541() {
  if (this->_localPipeInfo) {
    free(this->_localPipeInfo);
  }

  if (this->_remotePipeInfo) {
    free(this->_remotePipeInfo);
  }
}

// calculate combined ogf/ocf value

#define GAP_PROFILE_BROADCASTER 0x01
#define GAP_PROFILE_OBSERVER    0x02
#define GAP_PROFILE_PERIPHERAL  0x04
#define GAP_PROFILE_CENTRAL     0x08
#define GAP_RK_SIZE 16

uint8_t irk[GAP_RK_SIZE] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t srk[GAP_RK_SIZE] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint32_t sig_counter[GAP_RK_SIZE] = {1,0,0,0};


int GAP_DeviceInit(uint8_t profileRole, uint8_t maxScanResponses, uint8_t *pIRK, uint8_t *pSRK, uint32_t *pSignCounter )
{
    uint8_t buf[42];
    uint8_t len = 0;
    
    buf[len++] = 0x01;                  // -Type    : 0x01 (Command)
    buf[len++] = 0x00;                  // -Opcode  : 0xFE00 (GAP_DeviceInit)
    buf[len++] = 0xFE;
    
    buf[len++] = 0x26;                  // -Data Length
    buf[len++] = profileRole;           //  Profile Role
    buf[len++] = maxScanResponses;      //  MaxScanRsps
    memcpy(&buf[len], pIRK, 16);        //  IRK
    len += 16;
    memcpy(&buf[len], pSRK, 16);        //  SRK
    len += 16;
    memcpy(&buf[len], pSignCounter, 4); //  SignCounter
    len += 4;

    //sendMsg(buf, len);
    hci_tx_pool->addMsg(buf, len);
    return 1;
}


#define  CONNECTABLE_UNIDIR 0
#define  CONNECTABLE_DIRECT 1
#define  DISCOVERABLE_UNIDIR  2
#define  NOTCONNECTABLE_UNIDIR 3

#define ADDRTYPE_PUBLIC   0 
#define ADDRTYPE_STATIC   1
#define ADDRTYPE_PRIVATE_NONRESOLVE  2
#define ADDRTYPE_PRIVATE_RESOLVE  3
uint8_t addr[6] = {0,0,0,0,0,0};

int GAP_MakeDiscoverable(uint8_t eventType, uint8_t initiatorAddrType, uint8_t *initiatorAddr, 
                         uint8_t channelMap, uint8_t filterPolicy)
{
    uint8_t buf[42] = {};
    uint8_t len = 0;
    
    buf[len++] = 0x01;                  // -Type    : 0x01 (Command)
    buf[len++] = 0x06;                  // -Opcode  : 0xFE06 (GAP_MakeDiscoverable)
    buf[len++] = 0xFE;
    
    buf[len++] = eventType;                 
    buf[len++] = initiatorAddrType;           
    memcpy(&buf[len], initiatorAddr, 6);       
    len += 6;
    buf[len++] = channelMap;
    buf[len++] = filterPolicy;

    //sendMsg(buf, len);
    hci_tx_pool->addMsg(buf, len);
    return 1;
}



	
int GATT_AddService(uint16_t nAttributes)
{
	uint8_t buf[42] = {};
	uint8_t len = 0;
	
	buf[len++] = 0x01;                  // -Type    : 0x01 (Command)
	buf[len++] = 0xFC;                  // -Opcode  : 0xFDFC (GATT_AddService)
	buf[len++] = 0xFD;
	
	buf[len++] = 0x00;
	buf[len++] = 0x28;

	buf[len++] = nAttributes && 0xFF;
	buf[len++] = (nAttributes && 0xFF00) >> 8;
    buf[len++] =  7; // encKey = 7..16
	//BLE.write(buf, len);
    hci_tx_pool->addMsg(buf, len);
	return 1;
}


#define GATT_UUID_SIZE 16

uint8_t gatt_uuid[GATT_UUID_SIZE] = {'L','E','D',0,0,0,0,0,0,0,0,0,0,0,0,0};

#define GATT_PERMIT_READ         0x01
#define GATT_PERMIT_WRITE        0x02
#define GATT_PERMIT_AUTHEN_READ  0x04	
#define GATT_PERMIT_AUTHEN_WRITE 0x08	
#define GATT_PERMIT_AUTHOR_READ  0x10	
#define GATT_PERMIT_AUTHOR_WRITE 0x20

int GATT_AddAttribute (uint8_t *uuid, uint8_t uuid_len, uint8_t permission)
{
	uint8_t buf[42] = {};
	uint8_t len = 0;
    int i = 0;	

	buf[len++] = 0x01;                  // -Type    : 0x01 (Command)
	buf[len++] = 0xFE;                  // -Opcode  : 0xFDFE (GATT_AddAttribute)
	buf[len++] = 0xFD;
	for (i = 0; ((i < uuid_len) || (i < GATT_UUID_SIZE)); ++i) {
        buf[len+i] = uuid[i];
    }	 
    len +=16;
   
    buf[len++] = permission;

	//BLE.write(buf, len);
    hci_tx_pool->addMsg(buf, len);

	return 1;
}

void cc2541::begin(unsigned char advertisementDataType,
                      unsigned char advertisementDataLength,
                      const unsigned char* advertisementData,
                      unsigned char scanDataType,
                      unsigned char scanDataLength,
                      const unsigned char* scanData,
                      BLELocalAttribute** localAttributes,
                      unsigned char numLocalAttributes,
                      BLERemoteAttribute** remoteAttributes,
                      unsigned char numRemoteAttributes)
{
  unsigned char numLocalPipedCharacteristics = 0;
  unsigned char numLocalPipes = 0;
  int rc = 0;
  
  Serial1.begin(115200);
  BLE.begin(115200);

  rc = GAP_DeviceInit (GAP_PROFILE_PERIPHERAL, 3, irk, srk, sig_counter);
  dbgPrint("Begin ... Init Done");
  dbgPrint(rc);

  for (int i = 0; i < numLocalAttributes; i++) {
    BLELocalAttribute* localAttribute = localAttributes[i];

    if (localAttribute->type() == BLETypeCharacteristic) {
      BLECharacteristic* characteristic = (BLECharacteristic *)localAttribute;
      unsigned char properties = characteristic->properties();

      if (properties) {
        numLocalPipedCharacteristics++;

        if (properties & BLEBroadcast) {
          numLocalPipes++;
        }

        if (properties & BLENotify) {
          numLocalPipes++;
        }

        if (properties & BLEIndicate) {
          numLocalPipes++;
        }

        if (properties & BLEWriteWithoutResponse) {
          numLocalPipes++;
        }

        if (properties & BLEWrite) {
          numLocalPipes++;
        }

        if (properties & BLERead) {
          numLocalPipes++;
        }
      }
    }
  }

  this->_localPipeInfo = (struct localPipeInfo*)malloc(sizeof(struct localPipeInfo) * numLocalPipedCharacteristics);

 // this->waitForSetupMode();

  hal_hci_data_t setupMsg;
  struct setupMsgData* setupMsgData = (struct setupMsgData*)setupMsg.buffer;

  setupMsg.status_byte = 0;

  bool hasAdvertisementData = advertisementDataType && advertisementDataLength && advertisementData;
  bool hasScanData          = scanDataType && scanDataLength && scanData;
  // GATT
  unsigned short gattSetupMsgOffset = 0;
  unsigned short handle             = 1;
  unsigned char  pipe               = 1;
  unsigned char  numLocalPiped      = 0;
  unsigned char  numRemotePiped     = 0;

  for (int i = 0; i < numLocalAttributes; i++) {
    BLELocalAttribute* localAttribute = localAttributes[i];
    unsigned short localAttributeType = localAttribute->type();
    BLEUuid uuid = BLEUuid(localAttribute->uuid());
    const unsigned char* uuidData = uuid.data();
    unsigned char uuidLength = uuid.length();

    if (localAttributeType == BLETypeService) {
      BLEService* service = (BLEService *)localAttribute;

      rc = GATT_AddService(numLocalAttributes);
      dbgPrint("GATT_AddService ... Done");
      dbgPrint(rc);

    } else if (localAttributeType == BLETypeCharacteristic) {
      BLECharacteristic* characteristic = (BLECharacteristic *)localAttribute;
      unsigned char properties = characteristic->properties();
      const char* characteristicUuid = characteristic->uuid();

      struct localPipeInfo* localPipeInfo = &this->_localPipeInfo[numLocalPiped];

      memset(localPipeInfo, 0, sizeof(struct localPipeInfo));

      localPipeInfo->characteristic = characteristic;

      if (properties) {
        numLocalPiped++;

        if (properties & BLEBroadcast) {
          localPipeInfo->advPipe = pipe;

          pipe++;
        }

        if (properties & BLENotify) {
          localPipeInfo->txPipe = pipe;

          pipe++;
        }

        if (properties & BLEIndicate) {
          localPipeInfo->txAckPipe = pipe;

          pipe++;
        }

        if (properties & BLEWriteWithoutResponse) {
          localPipeInfo->rxPipe = pipe;

          pipe++;
        }

        if (properties & BLEWrite) {
          localPipeInfo->rxAckPipe = pipe;

          pipe++;
        }

        if (properties & BLERead) {
          localPipeInfo->setPipe = pipe;

          pipe++;
        }
      }

      rc = GATT_AddAttribute ((uint8_t *)uuidData, 2, 
                (properties & (BLEWrite | BLEWriteWithoutResponse)) ?
                 GATT_PERMIT_WRITE : GATT_PERMIT_READ);

      dbgPrint("GATT_AddAttribute ... Done");
      dbgPrint(rc);
      //this->waitForSetupMode();

    } else if (localAttributeType == BLETypeDescriptor) {
      BLEDescriptor* descriptor = (BLEDescriptor *)localAttribute;
    }
  }

  this->_numLocalPipeInfo = numLocalPiped;


  hci_tx_pool->sendNextMsg();
}


bool store_rx_msg_data(uint8_t data) {    if (rx_buffer.len < (RX_BUFFER_MAX_LEN-1)) {
        rx_buffer.data[rx_buffer.len] = data;
        rx_buffer.len++;

    } else {
        rx_msg_clean();
    }
}

bool processVendorSpecificEvent() {
    uint16_t i = 0;
    bool done = false;

    
    if (rx_buffer.len <= HCI_GAP_CODE_POS ) {
        done = false;
    } else {
        rx_buffer.paramCounter++;
    }

    if ((debugMsgSent==6) && (rx_buffer.paramCounter==5))
        debug++;
    // when all the params are received, exit with true
    if  (rx_buffer.paramCounter == rx_buffer.data[HCI_MESSAGE_LEN_POS] ) {  


         dbgPrint(F("processVendorSpecificEvent SUCCESS")); 
          
        uint16_t gapCode = GAPCODE(rx_buffer.data[HCI_GAP_CODE_POS+1], rx_buffer.data[HCI_GAP_CODE_POS]);
        
        switch(gapCode) {        
            case GAP_CommandStatus:
            if (rx_buffer.data[GAP_ERROR_CODE_POS] == GAP_ERR_SUCCESS) 
                rx_buffer.messageProcessed = partial;
            else
                rx_buffer.messageProcessed = error;

            break;
            
            case GAP_DeviceInitDone:
            if (rx_buffer.data[GAP_ERROR_CODE_POS] == GAP_ERR_SUCCESS) {
                rx_buffer.messageProcessed = noError;
                hci_tx_pool->sendNextMsg();
             } else
                rx_buffer.messageProcessed = error;
            break;
            
            default:
            break;
        } 
        done = true;
    } 

    return done;
}

bool processCommandStatus() {
    uint16_t i = 0;
    bool done = false;

    if (rx_buffer.len == (2+(rx_buffer.data[HCI_MESSAGE_LEN_POS]) )) {
        done = true;
    }

    if (rx_buffer.data[HCI_MESSAGE_LEN_POS] && rx_buffer.data[3]) {
        if (!initDone) {
           initDone = true; 
        } else {
           hci_tx_pool->sendNextMsg();
        }
        dbgPrint(F("SUCCESS"));
    } else {
        dbgPrint(F("FAILED"));
    }
    return done;
}

bool processCommandComplete() {
    uint16_t i = 0;
    bool done = false;
    
    if (rx_buffer.len <= HCI_COMMAND_COMPLETE_CODE_POS ) {
        done = false;
    } else {
        rx_buffer.paramCounter++;
    }
    

     // when all the params are received, exit with true
    if  (rx_buffer.paramCounter == rx_buffer.data[HCI_MESSAGE_LEN_POS] ) {  
        
      hci_tx_pool->sendNextMsg();

    } 
    return done;
}

bool processEvent() 
{
    uint16_t i = 0;
    bool done = false;
    

            
    
    if (rx_buffer.len <= HCI_MESSAGE_LEN_POS ) {
        return done;
    } else if (rx_buffer.len == HCI_MESSAGE_LEN_POS ) {        
        rx_buffer.paramCounter=0; // reach the paramLenField, reset Counter               
    }        


    switch (rx_buffer.data[HCI_EVENT_TYPE_POS]) {
       
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

bool process_rx_msg_data() {
    uint16_t i = 0;
    bool done = false;

    switch (rx_buffer.data[HCI_PACKET_TYPE_POS]) {
    case HCI_EVENT:  // Event
        done = processEvent();
    break;
     
    default:
    break;
    }

    if (done) {
        rx_msg_clean();
    }
    
    return (rx_buffer.messageProcessed == noError);
    
}

void cc2541::poll() {
    dbgPrint(F("poll  TBD"));
    while (BLE.available()) {
        store_rx_msg_data(BLE.read());
        if (process_rx_msg_data()) {
            //GAP_MakeDiscoverable(DISCOVERABLE_UNIDIR, ADDRTYPE_PUBLIC, addr, 0, 0);
            //sendSetupMessage();
           // dbgPrint("Make Discoverable");
        }
    }
}    

bool cc2541::updateCharacteristicValue(BLECharacteristic& characteristic) {

  bool success = true;
  return success;
}

bool cc2541::broadcastCharacteristic(BLECharacteristic& characteristic) {
  bool success = false;

  struct localPipeInfo* localPipeInfo = this->localPipeInfoForCharacteristic(characteristic);

  if (localPipeInfo) {
    if (localPipeInfo->advPipe) {
      uint64_t advPipes = ((uint64_t)1) << (localPipeInfo->advPipe);

        dbgPrint(F("broadcastCharacteristic  TBD"));
      //success = lib_aci_open_adv_pipes((uint8_t*)&advPipes) &&
      //            lib_aci_set_local_data(&this->_aciState, localPipeInfo->advPipe, (uint8_t*)characteristic.value(), characteristic.valueLength());

      if (success) {
        this->_broadcastPipe = localPipeInfo->advPipe;
      }
    }
  }

  return success;
}

bool cc2541::canNotifyCharacteristic(BLECharacteristic& characteristic) {
    dbgPrint(F("canNotifyCharacteristic  TBD"));
  //return (this->_aciState.data_credit_available > 0);
  return true;
}

bool cc2541::canIndicateCharacteristic(BLECharacteristic& characteristic) {
    dbgPrint(F("canIndicateCharacteristic  TBD"));
  //return (this->_aciState.data_credit_available > 0);
  return true;
}

bool cc2541::canReadRemoteCharacteristic(BLERemoteCharacteristic& characteristic) {
  bool success = false;

  struct remotePipeInfo* remotePipeInfo = this->remotePipeInfoForCharacteristic(characteristic);

  if (remotePipeInfo) {
    unsigned char rxReqPipe = remotePipeInfo->rxReqPipe;

    if (rxReqPipe) {
        dbgPrint(F("canReadRemoteCharacteristic  TBD"));
      //success = lib_aci_is_pipe_available(&this->_aciState, rxReqPipe);
    }
  }

  return success;
}

bool cc2541::readRemoteCharacteristic(BLERemoteCharacteristic& characteristic) {
  bool success = false;

  struct remotePipeInfo* remotePipeInfo = this->remotePipeInfoForCharacteristic(characteristic);

  if (remotePipeInfo) {
    unsigned char rxReqPipe = remotePipeInfo->rxReqPipe;

    if (rxReqPipe) {
        dbgPrint(F("readRemoteCharacteristic  TBD"));
      // success = lib_aci_is_pipe_available(&this->_aciState, rxReqPipe) &&
      //          lib_aci_request_data(&this->_aciState, rxReqPipe);
    }
  }

  return success;
}

bool cc2541::canWriteRemoteCharacteristic(BLERemoteCharacteristic& characteristic) {
  bool success = false;

  struct remotePipeInfo* remotePipeInfo = this->remotePipeInfoForCharacteristic(characteristic);

  if (remotePipeInfo) {
    unsigned char pipe = remotePipeInfo->txAckPipe ? remotePipeInfo->txAckPipe : remotePipeInfo->txPipe;

    if (pipe) {
       dbgPrint(F("canWriteRemoteCharacteristic  TBD"));
      //success = lib_aci_is_pipe_available(&this->_aciState, pipe);
    }
  }

  return success;
}

bool cc2541::writeRemoteCharacteristic(BLERemoteCharacteristic& characteristic, const unsigned char value[], unsigned char length) {
  bool success = false;

  struct remotePipeInfo* remotePipeInfo = this->remotePipeInfoForCharacteristic(characteristic);

  if (remotePipeInfo) {
    unsigned char pipe = remotePipeInfo->txAckPipe ? remotePipeInfo->txAckPipe : remotePipeInfo->txPipe;

    if (pipe) {
        dbgPrint(F("writeRemoteCharacteristic  TBD"));
    //  success = lib_aci_is_pipe_available(&this->_aciState, pipe) &&
    //            lib_aci_send_data(pipe, (uint8_t*)value, length);
    }
  }

  return success;
}

bool cc2541::canSubscribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic) {
  bool success = false;

  struct remotePipeInfo* remotePipeInfo = this->remotePipeInfoForCharacteristic(characteristic);

  if (remotePipeInfo) {
    if (remotePipeInfo->characteristic == &characteristic) {
      unsigned char pipe = remotePipeInfo->rxPipe ? remotePipeInfo->rxPipe : remotePipeInfo->rxAckPipe;

      if (pipe) {
        dbgPrint(F("canSubscribeRemoteCharacteristic  TBD"));
        // success = lib_aci_is_pipe_closed(&this->_aciState, pipe);
      }
    }
  }

  return success;
}

bool cc2541::subscribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic) {
  bool success = false;

  struct remotePipeInfo* remotePipeInfo = this->remotePipeInfoForCharacteristic(characteristic);

  if (remotePipeInfo) {
    unsigned char pipe = remotePipeInfo->rxPipe ? remotePipeInfo->rxPipe : remotePipeInfo->rxAckPipe;

    if (pipe) {
        dbgPrint(F("subscribeRemoteCharacteristic  TBD"));
      //success = lib_aci_is_pipe_closed(&this->_aciState, pipe) && lib_aci_open_remote_pipe(&this->_aciState, pipe);
    }
  }

  return success;
}

bool cc2541::canUnsubscribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic) {
  return this->canSubscribeRemoteCharacteristic(characteristic);
}

bool cc2541::unsubcribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic) {
  bool success = false;
    dbgPrint(F("unsubcribeRemoteCharacteristic"));
    return success;
}

void cc2541::startAdvertising() {
  dbgPrint(F("Advertising started."));
}

void cc2541::disconnect() {
 // lib_aci_disconnect(&this->_aciState, ACI_REASON_TERMINATE);
    dbgPrint(F("disconnect"));
}

void cc2541::requestAddress() {
  //lib_aci_get_address();
    dbgPrint(F("requestAddress"));
}

void cc2541::requestTemperature() {
  //lib_aci_get_temperature();
  dbgPrint(F("requestTemperature"));
}

void cc2541::requestBatteryLevel() {
  //lib_aci_get_battery_level();
  dbgPrint(F("requestBatteryLevel"));

}

void cc2541::waitForSetupMode()
{
  dbgPrint(F("waitForSetupMode"));
}



void cc2541::sendSetupMessage(hal_hci_data_t* setupMsg, unsigned char type, unsigned short& offset) {
  //struct setupMsgData* setupMsgData = (hal_hci_data_t*)(setupMsg->buffer);

  //setupMsgData->cmd      = ACI_CMD_SETUP;
  //setupMsgData->type     = (type << 4) | ((offset >> 8) & 0x0f);
  //setupMsgData->offset   = (offset & 0xff);

  BLE.print((const char *)setupMsg->buffer);

  //offset += (setupMsgData->length - 3);
}

struct cc2541::localPipeInfo* cc2541::localPipeInfoForCharacteristic(BLECharacteristic& characteristic) {
  struct localPipeInfo* result = NULL;

  for (int i = 0; i < this->_numLocalPipeInfo; i++) {
    struct localPipeInfo* localPipeInfo = &this->_localPipeInfo[i];

    if (localPipeInfo->characteristic == &characteristic) {
      result = localPipeInfo;
      break;
    }
  }

  return result;
}

struct cc2541::remotePipeInfo* cc2541::remotePipeInfoForCharacteristic(BLERemoteCharacteristic& characteristic) {
  struct remotePipeInfo* result = NULL;

  for (int i = 0; i < this->_numRemotePipeInfo; i++) {
    struct remotePipeInfo* remotePipeInfo = &this->_remotePipeInfo[i];

    if (remotePipeInfo->characteristic == &characteristic) {
      result = remotePipeInfo;
      break;
    }
  }

  return result;
}

#endif
