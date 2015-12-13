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
#include "utility/ASME/HciUart.h"
#include "utility/ASME/ASMEDebug.h"
#include "utility/HCI/packet/HCICommand.h"
#include "utility/HCI/packet/HCIEvent.h"

// just for production test
bool bleProductionTestOK=false;

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

/* Having PROGMEM here caused the setup messages to be zeroed out */
static const hal_hci_data_t baseSetupMsgs[NB_BASE_SETUP_MESSAGES] /*PROGMEM*/ = {};

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


int cc2541 :: GAP_DeviceInit(uint8_t profileRole, uint8_t maxScanResponses, uint8_t *pIRK, uint8_t *pSRK, uint32_t *pSignCounter )
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

int cc2541 :: GAP_MakeDiscoverable(uint8_t eventType, uint8_t initiatorAddrType, uint8_t *initiatorAddr, 
                         uint8_t channelMap, uint8_t filterPolicy)
{
    uint8_t buf[42] = {};
    uint8_t len = 0;
    
    buf[len++] = 0x01;                  // -Type    : 0x01 (Command)
    buf[len++] = 0x06;                  // -Opcode  : 0xFE06 (GAP_MakeDiscoverable)
    buf[len++] = 0xFE;
    buf[len++] = 10;
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


#define UPD_ADV_SCAN_RSP   0 
#define UPD_ADV_ADV_DATA   1

int cc2541 :: GAP_UpdateAdvertisingData(uint8_t adParam, uint8_t advLen, uint8_t *data)
{
    uint8_t buf[42] = {};
    uint8_t len = 0;
    
    buf[len++] = 0x01;                  // -Type    : 0x01 (Command)
    buf[len++] = 0x07;                  // -Opcode  : 0xFE06 (GAP_MakeDiscoverable)
    buf[len++] = 0xFE;
    buf[len++] = advLen + 2;            // len
    buf[len++] = adParam;                 
    buf[len++] = advLen;           
    memcpy(&buf[len], data, advLen);       
    len += advLen;

    hci_tx_pool->addMsg(buf, len);
    return 1;
}

#define GATT_UUID_SIZE 16
	
int cc2541 :: GATT_AddService(uint16_t nAttributes, uint8_t *uuid, uint8_t uuid_len)
{
	uint8_t buf[42] = {};
	uint8_t len = 0;
	uint8_t i = 0;

	buf[len++] = 0x01;                  // -Type    : 0x01 (Command)
	buf[len++] = 0xFC;                  // -Opcode  : 0xFDFC (GATT_AddService)
	buf[len++] = 0xFD;
	
    //buf[len++] = 0x05;                  // Data len

    buf[len++] = uuid_len + 3;          // data len
    //for (i = 0; ((i < uuid_len) && (i < GATT_UUID_SIZE)); ++i) {
    //     buf[len+i] = uuid[i];
    //}
    //len += min(uuid_len, GATT_UUID_SIZE);

	buf[len++] = 0x00;
	buf[len++] = 0x28;

	buf[len++] = nAttributes & 0xFF;
	buf[len++] = (nAttributes & 0xFF00) >> 8;
    buf[len++] =  0x10; // encKey = 7..16

    hci_tx_pool->addMsg(buf, len);
	return 1;
}



uint8_t gatt_uuid[GATT_UUID_SIZE] = {'L','E','D',0,0,0,0,0,0,0,0,0,0,0,0,0};

#define GATT_PERMIT_READ         0x01
#define GATT_PERMIT_WRITE        0x02
#define GATT_PERMIT_AUTHEN_READ  0x04	
#define GATT_PERMIT_AUTHEN_WRITE 0x08	
#define GATT_PERMIT_AUTHOR_READ  0x10	
#define GATT_PERMIT_AUTHOR_WRITE 0x20

int cc2541 :: GATT_AddAttribute (uint8_t *uuid, uint8_t uuid_len, uint8_t permission)
{
	uint8_t buf[42] = {};
	uint8_t len = 0;
    int i = 0;	

	buf[len++] = 0x01;                  // -Type    : 0x01 (Command)
	buf[len++] = 0xFE;                  // -Opcode  : 0xFDFE (GATT_AddAttribute)
	buf[len++] = 0xFD;
    buf[len++] = uuid_len + 1;          // data len
	for (i = 0; ((i < uuid_len) && (i < GATT_UUID_SIZE)); ++i) {
        buf[len+i] = uuid[i];
    }	 
    len += min(uuid_len, GATT_UUID_SIZE);
   
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

  delay(500);
  
  Serial1.begin(115200);
  HciUart *bleUart = new HciUart();
#if (BTOOL_DEBUG)
  return;
#endif
  hci_tx_pool = new HciDispatchPool(bleUart);      
  hci_rx_process = new HCIProcessAnswer(bleUart);
  
  rc = GAP_DeviceInit (GAP_PROFILE_PERIPHERAL, 5, irk, srk, sig_counter);
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

/*
 // this->_localPipeInfo = (struct localPipeInfo*)malloc(sizeof(struct localPipeInfo) * numLocalPipedCharacteristics);

 // this->waitForSetupMode();

  hal_hci_data_t setupMsg;
  struct setupMsgData* setupMsgData = (struct setupMsgData*)setupMsg.buffer;

  setupMsg.status_byte = 0;
*/
  bool hasAdvertisementData = advertisementDataType && advertisementDataLength && advertisementData;
  bool hasScanData          = scanDataType && scanDataLength && scanData;
  // GATT
  unsigned short gattSetupMsgOffset = 0;
  unsigned short handle             = 1;
  unsigned char  pipe               = 1;
  unsigned char  numLocalPiped      = 0;
  unsigned char  numRemotePiped     = 0;
  static uint8_t serviceDone = 0;

  uint8_t partAttr = 0;
  for (int j = 0; j < numLocalAttributes; j++) {
      BLELocalAttribute* localAttribute = localAttributes[j];
      unsigned short localAttributeType = localAttribute->type();
      BLEUuid uuid = BLEUuid(localAttribute->uuid());
      const unsigned char* uuidData = uuid.data();
      unsigned char uuidLength = uuid.length();
  
      if (uuidLength <=2) partAttr++;
  }

  for (int i = 0; i < numLocalAttributes; i++) {
    BLELocalAttribute* localAttribute = localAttributes[i];
    unsigned short localAttributeType = localAttribute->type();
    BLEUuid uuid = BLEUuid(localAttribute->uuid());
    const unsigned char* uuidData = uuid.data();
    unsigned char uuidLength = uuid.length();

    if(uuidLength > 2) continue;

    if (localAttributeType == BLETypeService) {
      BLEService* service = (BLEService *)localAttribute;


      if (serviceDone) continue;
      // 1 service more!!!
      //rc = GATT_AddService(numLocalAttributes, (uint8_t *)uuidData, uuidLength);
      rc = GATT_AddService(partAttr-1, (uint8_t *)BLETypeService, uuidLength);
      dbgPrint("GATT_AddService ... Done");
      dbgPrint(rc);
      serviceDone++;

    } else if (localAttributeType == BLETypeCharacteristic) {
      BLECharacteristic* characteristic = (BLECharacteristic *)localAttribute;
      unsigned char properties = characteristic->properties();
      const char* characteristicUuid = characteristic->uuid();
/*
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
      */
      rc = GATT_AddAttribute ((uint8_t *)uuidData, uuidLength, 
                (properties & (BLEWrite | BLEWriteWithoutResponse)) ?
                 GATT_PERMIT_WRITE : GATT_PERMIT_READ);

      dbgPrint("GATT_AddAttribute ... Done");
      dbgPrint(rc);
      //this->waitForSetupMode();

    } else if (localAttributeType == BLETypeDescriptor) {
      BLEDescriptor* descriptor = (BLEDescriptor *)localAttribute;
    }
  }/*

  this->_numLocalPipeInfo = numLocalPiped;
*/
  uint8_t data[] = {0x02,0x01,0x06};
  GAP_UpdateAdvertisingData(UPD_ADV_ADV_DATA, 3, data);
  GAP_MakeDiscoverable(NOTCONNECTABLE_UNIDIR, ADDRTYPE_PUBLIC, addr, 
                       0x7, 0x0);

  hci_tx_pool->sendNextMsg();
}

void cc2541::poll() {
#if (BTOOL_DEBUG)
   while (BLE.available()) {
      Serial1.write(BLE.read());
   }
   while (Serial1.available()) {
       BLE.write(Serial1.read());
   }
#else 
    dbgPrint(F("poll  TBD"));
    while (hci_rx_process->readRxChar()) {        
        if (hci_rx_process->process_rx_msg_data()) {
            bleProductionTestOK=true; // just for production test
            hci_tx_pool->sendNextMsg();
        }       
    }
#endif
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
