#if defined(NRFCC2541)

#include <hci.h>
#include <hci_cmds.h>

#include "BLECharacteristic.h"
#include "BLEDescriptor.h"
#include "BLERemoteService.h"
#include "BLERemoteCharacteristic.h"
#include "BLEService.h"
#include "BLEUtil.h"
#include "BLEUuid.h"

#include "cc2541.h"

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
uint32_t sig_counter[GAP_RK_SIZE] = {0,0,0,0};


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

    BLE.write(buf, len);

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

    BLE.write(buf, len);

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
	BLE.write(buf, len);

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

	BLE.write(buf, len);

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

  unsigned char numRemoteServices = 0;
  unsigned char numRemotePipedCharacteristics = 0;
  unsigned char numRemotePipes = 0;
  unsigned char numCustomUuids = 0;

  for (int i = 0; i < numRemoteAttributes; i++) {
    BLERemoteAttribute* remoteAttribute = remoteAttributes[i];
    unsigned short remoteAttributeType = remoteAttribute->type();
    BLEUuid uuid = BLEUuid(remoteAttribute->uuid());

    if (uuid.length() > 2) {
      numCustomUuids++;
    }

    if (remoteAttributeType == BLETypeService) {
      numRemoteServices++;
    } else if (remoteAttributeType == BLETypeCharacteristic) {
      BLERemoteCharacteristic* characteristic = (BLERemoteCharacteristic *)remoteAttribute;
      unsigned char properties = characteristic->properties();

      if (properties) {
        numRemotePipedCharacteristics++;

        if (properties & BLENotify) {
          numRemotePipes++;
        }

        if (properties & BLEIndicate) {
          numRemotePipes++;
        }

        if (properties & BLEWriteWithoutResponse) {
          numRemotePipes++;
        }

        if (properties & BLEWrite) {
          numRemotePipes++;
        }

        if (properties & BLERead) {
          numRemotePipes++;
        }
      }
    }
  }

  this->_remotePipeInfo = (struct remotePipeInfo*)malloc(sizeof(struct remotePipeInfo) * numRemotePipedCharacteristics);

  this->waitForSetupMode();

  hal_hci_data_t setupMsg;
  struct setupMsgData* setupMsgData = (struct setupMsgData*)setupMsg.buffer;

  setupMsg.status_byte = 0;

  bool hasAdvertisementData = advertisementDataType && advertisementDataLength && advertisementData;
  bool hasScanData          = scanDataType && scanDataLength && scanData;
/*
  for (int i = 0; i < NB_BASE_SETUP_MESSAGES; i++) {
    int setupMsgSize = pgm_read_byte_near(&baseSetupMsgs[i].buffer[0]) + 2;

    memcpy_P(&setupMsg, &baseSetupMsgs[i], setupMsgSize);

    if (i == 1) {
      setupMsgData->data[5] = numRemoteServices;
      setupMsgData->data[6] = numLocalPipedCharacteristics;
      setupMsgData->data[7] = numRemotePipedCharacteristics;
      setupMsgData->data[8] = (numLocalPipes + numRemotePipes);

      if (this->_bondStore) {
        setupMsgData->data[4] |= 0x02;
      }

    } else if (i == 2 && hasAdvertisementData) {
      setupMsgData->data[18] |= 0x40;

      setupMsgData->data[22] |= 0x40;

      setupMsgData->data[26] = numCustomUuids;
    } else if (i == 3) {
      if (hasAdvertisementData) {
        setupMsgData->data[16] |= 0x40;
      }

      if (hasScanData) {
        setupMsgData->data[8] |= 0x40;

        setupMsgData->data[12] |= 0x40;

        setupMsgData->data[20] |= 0x40;
      }
    } else if (i == 4) {
      if (this->_bondStore) {
        setupMsgData->data[0] |= 0x01;
      }
    } else if (i == 5 && hasAdvertisementData) {
      setupMsgData->data[0] = advertisementDataType;
      setupMsgData->data[1] = advertisementDataLength;
      memcpy(&setupMsgData->data[2], advertisementData, advertisementDataLength);
    } else if (i == 6 && hasScanData) {
      setupMsgData->data[0] = scanDataType;
      setupMsgData->data[1] = scanDataLength;
      memcpy(&setupMsgData->data[2], scanData, scanDataLength);
    }

    this->sendSetupMessage(&setupMsg);
  }
*/
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
/*
      setupMsgData->length  = 12 + uuidLength;

      setupMsgData->data[0] = 0x04;
      setupMsgData->data[1] = 0x04;
      setupMsgData->data[2] = uuidLength;
      setupMsgData->data[3] = uuidLength;

      setupMsgData->data[4] = (handle >> 8) & 0xff;
      setupMsgData->data[5] = handle & 0xff;
      handle++;

      setupMsgData->data[6] = (localAttributeType >> 8) & 0xff;
      setupMsgData->data[7] = localAttributeType & 0xff;

      setupMsgData->data[8] = ACI_STORE_LOCAL;

      memcpy(&setupMsgData->data[9], uuidData, uuidLength);

      this->sendSetupMessage(&setupMsg, 0x2, gattSetupMsgOffset);*/
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
/*
      setupMsgData->length   = 15 + uuidLength;

      setupMsgData->data[0]  = 0x04;
      setupMsgData->data[1]  = 0x04;
      setupMsgData->data[2]  = 3 + uuidLength;
      setupMsgData->data[3]  = 3 + uuidLength;

      setupMsgData->data[4]  = (handle >> 8) & 0xff;
      setupMsgData->data[5]  = handle & 0xff;
      handle++;

      setupMsgData->data[6]  = (localAttributeType >> 8) & 0xff;
      setupMsgData->data[7]  = localAttributeType & 0xff;

      setupMsgData->data[8]  = ACI_STORE_LOCAL;
      setupMsgData->data[9]  = properties & 0xfe;

      setupMsgData->data[10] = handle & 0xff;
      setupMsgData->data[11] = (handle >> 8) & 0xff;
      localPipeInfo->valueHandle = handle;
      handle++;

      memcpy(&setupMsgData->data[12], uuidData, uuidLength);

      this->sendSetupMessage(&setupMsg, 0x2, gattSetupMsgOffset);

      setupMsgData->length   = 12;

      setupMsgData->data[0]  = 0x04;
      setupMsgData->data[1]  = 0x00;

      if (characteristic->fixedLength()) {
        setupMsgData->data[0] |= 0x02;
      }

      if (properties & BLERead) {
        setupMsgData->data[1] |= 0x04;
      }

      if (this->_bondStore &&
          strcmp(characteristicUuid, "2a00") != 0 &&
          strcmp(characteristicUuid, "2a01") != 0 &&
          strcmp(characteristicUuid, "2a05") != 0) {
        setupMsgData->data[1] |= 0x08;
      }

      if (properties & (BLEWrite | BLEWriteWithoutResponse)) {
        setupMsgData->data[0] |= 0x40;
        setupMsgData->data[1] |= 0x10;
      }

      if (properties & BLENotify) {
        setupMsgData->data[0] |= 0x10;
      }

      if (properties & BLEIndicate) {
        setupMsgData->data[0] |= 0x20;
      }

      unsigned char valueSize = characteristic->valueSize();
      unsigned char valueLength = characteristic->valueLength();

      setupMsgData->data[2]  = valueSize;
      if (characteristic->fixedLength()) {
        setupMsgData->data[2]++;
      }

      setupMsgData->data[3]  = valueLength;

      setupMsgData->data[4]  = (localPipeInfo->valueHandle >> 8) & 0xff;
      setupMsgData->data[5]  = localPipeInfo->valueHandle & 0xff;

      if (uuidLength == 2) {
        setupMsgData->data[6]  = uuidData[1];
        setupMsgData->data[7]  = uuidData[0];
      } else {
        setupMsgData->data[6]  = 0x00;
        setupMsgData->data[7]  = 0x00;
      }

      setupMsgData->data[8]  = ACI_STORE_LOCAL;

      this->sendSetupMessage(&setupMsg, 0x2, gattSetupMsgOffset);
*/
      rc = GATT_AddAttribute ((uint8_t *)uuidData, 2, 
                (properties & (BLEWrite | BLEWriteWithoutResponse)) ?
                 GATT_PERMIT_WRITE : GATT_PERMIT_READ);

      dbgPrint("GATT_AddAttribute ... Done");
      dbgPrint(rc);
/*
      int valueOffset = 0;

      while(valueOffset < valueSize) {
        int chunkSize = min(valueSize - valueOffset, MAX_ATTRIBUTE_VALUE_PER_SETUP_MSG);
        int valueCopySize = min(valueLength - valueOffset, chunkSize);

        setupMsgData->length = 3 + chunkSize;

        memset(setupMsgData->data, 0x00, chunkSize);

        if (valueCopySize > 0) {
          for (int j = 0; j < chunkSize; j++) {
            setupMsgData->data[j] = (*characteristic)[j + valueOffset];
          }
        }

        this->sendSetupMessage(&setupMsg, 0x2, gattSetupMsgOffset);

        valueOffset += chunkSize;
      }

      if (properties & (BLENotify | BLEIndicate)) {
        setupMsgData->length   = 14;

        setupMsgData->data[0]  = 0x46;
        setupMsgData->data[1]  = 0x14;

        setupMsgData->data[2]  = 0x03;
        setupMsgData->data[3]  = 0x02;

        setupMsgData->data[4]  = (handle >> 8) & 0xff;
        setupMsgData->data[5]  = handle & 0xff;
        localPipeInfo->configHandle = handle;
        handle++;

        setupMsgData->data[6]  = 0x29;
        setupMsgData->data[7]  = 0x02;

        setupMsgData->data[8]  = ACI_STORE_LOCAL;

        setupMsgData->data[9]  = 0x00;
        setupMsgData->data[10] = 0x00;

        this->sendSetupMessage(&setupMsg, 0x2, gattSetupMsgOffset);
      }
*/
    } else if (localAttributeType == BLETypeDescriptor) {
      BLEDescriptor* descriptor = (BLEDescriptor *)localAttribute;
/*
      setupMsgData->length   = 12;

      setupMsgData->data[0]  = 0x04;
      setupMsgData->data[1]  = 0x04;

      unsigned char valueLength = descriptor->valueLength();

      setupMsgData->data[2]  = valueLength;
      setupMsgData->data[3]  = valueLength;

      setupMsgData->data[4]  = (handle >> 8) & 0xff;
      setupMsgData->data[5]  = handle & 0xff;
      handle++;

      setupMsgData->data[6]  = uuidData[1];
      setupMsgData->data[7]  = uuidData[0];

      setupMsgData->data[8]  = ACI_STORE_LOCAL;

      this->sendSetupMessage(&setupMsg, 0x2, gattSetupMsgOffset);

      int valueOffset = 0;

      while(valueOffset < valueLength) {
        int chunkSize = min(valueLength - valueOffset, MAX_ATTRIBUTE_VALUE_PER_SETUP_MSG);

        setupMsgData->length = 3 + chunkSize;

        for (int j = 0; j < chunkSize; j++) {
          setupMsgData->data[j] = (*descriptor)[j + valueOffset];
        }

        this->sendSetupMessage(&setupMsg, 0x2, gattSetupMsgOffset);

        valueOffset += chunkSize;
      }
*/
    }
  }

  this->_numLocalPipeInfo = numLocalPiped;

  // terminator
  setupMsgData->length   = 4;

  setupMsgData->data[0]  = 0x00;

  this->sendSetupMessage(&setupMsg, 0x2, gattSetupMsgOffset);

  unsigned short remoteServiceSetupMsgOffset  = 0;
  unsigned char  customUuidIndex = 2;

  for (int i = 0; i < numRemoteAttributes; i++) {
    BLERemoteAttribute* remoteAttribute = remoteAttributes[i];
    BLEUuid uuid = BLEUuid(remoteAttribute->uuid());
    const unsigned char* uuidData = uuid.data();
    unsigned char uuidLength = uuid.length();

    if (remoteAttribute->type() == BLETypeService) {
      BLERemoteService *remoteService = (BLERemoteService*)remoteAttributes[i];

      unsigned char numRemoteCharacteristics = 0;
      setupMsgData->data[3]    = (pipe - 1);

      for (int j = (i + 1); j < numRemoteAttributes; j++) {
        if (remoteAttributes[j]->type() == BLETypeCharacteristic) {
          BLERemoteCharacteristic* remoteCharateristic = (BLERemoteCharacteristic*)remoteAttributes[j];

          struct remotePipeInfo* remotePipeInfo = &this->_remotePipeInfo[numRemotePiped];
          memset(remotePipeInfo, 0, sizeof(struct remotePipeInfo));

          unsigned char properties = remoteCharateristic->properties();

          remotePipeInfo->characteristic = remoteCharateristic;

          if (properties & BLEWriteWithoutResponse) {
            remotePipeInfo->txPipe = pipe;

            pipe++;
          }

          if (properties & BLEWrite) {
            remotePipeInfo->txAckPipe = pipe;

            pipe++;
          }

          if (properties & BLERead) {
            remotePipeInfo->rxReqPipe = pipe;

            pipe++;
          }

          if (properties & BLENotify) {
            remotePipeInfo->rxPipe = pipe;

            pipe++;
          }

          if (properties & BLEIndicate) {
            remotePipeInfo->rxAckPipe = pipe;

            pipe++;
          }

          numRemoteCharacteristics++;

          numRemotePiped++;
        } else {
          break;
        }
      }

      setupMsgData->length = 8;

      if (uuidLength == 2) {
        setupMsgData->data[0]  = uuidData[1];
        setupMsgData->data[1]  = uuidData[0];
        setupMsgData->data[2]  = 0x01;
      } else {
        setupMsgData->data[0]  = uuidData[13];
        setupMsgData->data[1]  = uuidData[12];
        setupMsgData->data[2]  = customUuidIndex;

        customUuidIndex++;
      }

      setupMsgData->data[4]    = numRemoteCharacteristics;

      this->sendSetupMessage(&setupMsg, 0x3, remoteServiceSetupMsgOffset);
    }
  }

  this->_numRemotePipeInfo = numRemotePiped;

  // pipes
  unsigned short pipeSetupMsgOffset  = 0;

  for (int i = 0; i < numLocalPiped; i++) {
    struct localPipeInfo localPipeInfo = this->_localPipeInfo[i];

    setupMsgData->length   = 13;

    setupMsgData->data[0]  = 0x00;
    setupMsgData->data[1]  = 0x00;

    setupMsgData->data[2]  = 0x01;

    setupMsgData->data[3]  = 0x00;
    setupMsgData->data[4]  = 0x00;

    setupMsgData->data[5]  = 0x04;

    setupMsgData->data[6]  = (localPipeInfo.valueHandle >> 8) & 0xff;
    setupMsgData->data[7]  = localPipeInfo.valueHandle & 0xff;

    setupMsgData->data[8]  = (localPipeInfo.configHandle >> 8) & 0xff;
    setupMsgData->data[9]  = localPipeInfo.configHandle & 0xff;

    unsigned char properties = localPipeInfo.characteristic->properties();

    if (properties & BLEBroadcast) {
      setupMsgData->data[4] |= 0x01; // Adv
    }

    if (properties & BLENotify) {
      setupMsgData->data[4] |= 0x02; // TX
    }

    if (properties & BLEIndicate) {
      setupMsgData->data[4] |= 0x04; // TX Ack
    }

    if (properties & BLEWriteWithoutResponse) {
      setupMsgData->data[4] |= 0x08; // RX Ack
    }

    if (properties & BLEWrite) {
      setupMsgData->data[4] |= 0x10; // RX Ack
    }

    if (properties & BLERead) {
      setupMsgData->data[4] |= 0x80; // Set
    }

    this->sendSetupMessage(&setupMsg, 0x4, pipeSetupMsgOffset);
  }

  for (int i = 0; i < numRemotePiped; i++) {
    struct remotePipeInfo remotePipeInfo = this->_remotePipeInfo[i];

    BLERemoteCharacteristic *remoteCharacteristic = remotePipeInfo.characteristic;
    BLEUuid uuid = BLEUuid(remoteCharacteristic->uuid());
    const unsigned char* uuidData = uuid.data();
    unsigned char uuidLength = uuid.length();

    setupMsgData->length   = 13;

    if (uuidLength== 2) {
      setupMsgData->data[0]  = uuidData[1];
      setupMsgData->data[1]  = uuidData[0];
      setupMsgData->data[2]  = 0x01;
    } else {
      setupMsgData->data[0]  = uuidData[13];
      setupMsgData->data[1]  = uuidData[12];
      setupMsgData->data[2]  = customUuidIndex;

      customUuidIndex++;
    }

    setupMsgData->data[3]  = 0x00; // pipe properties
    setupMsgData->data[4]  = 0x00; // pipe properties

    setupMsgData->data[5]  = 0x04;

    setupMsgData->data[6]  = 0x00;
    setupMsgData->data[7]  = 0x00;

    setupMsgData->data[8]  = 0x00;
    setupMsgData->data[9]  = 0x00;

    unsigned char properties = remoteCharacteristic->properties();

    if (properties & BLERead) {
      setupMsgData->data[4] |= 0x40; // ACI_RX_REQ
    }

    if (properties & BLEWriteWithoutResponse) {
      setupMsgData->data[4] |= 0x02; // ACI_TX
    }

    if (properties & BLEWrite) {
      setupMsgData->data[4] |= 0x04; // ACI_TX_ACK
    }

    if (properties & BLENotify) {
      setupMsgData->data[4] |= 0x08; // ACI_RX
    }

    if (properties & BLEIndicate) {
      setupMsgData->data[4] |= 0x10; // ACI_RX_ACK
    }

    this->sendSetupMessage(&setupMsg, 0x4, pipeSetupMsgOffset);
  }

  // custom uuid's (remote only for now)
  unsigned short customUuidSetupMsgOffset = 0;

  for (int i = 0; i < numRemoteAttributes; i++) {
    BLERemoteAttribute* remoteAttribute = remoteAttributes[i];
    BLEUuid uuid = BLEUuid(remoteAttribute->uuid());
    const unsigned char* uuidData = uuid.data();
    unsigned char uuidLength = uuid.length();

    setupMsgData->length = 3 + uuidLength;
    memcpy(&setupMsgData->data, uuidData, uuidLength);

    setupMsgData->data[12]  = 0;
    setupMsgData->data[13]  = 0;

    this->sendSetupMessage(&setupMsg, 0x5, customUuidSetupMsgOffset);
  }

  // crc
  unsigned short crcOffset = 0;
  setupMsgData->length   = 6;
  setupMsgData->data[0]  = 3;

  this->sendSetupMessage(&setupMsg, 0xf, crcOffset);

 
}

#define RX_BUFFER_MAX_LEN  255
typedef struct {
    char data[RX_BUFFER_MAX_LEN];
    uint16_t len;
    bool complete;
} rx_buffer_t;

rx_buffer_t rx_buffer;

void rx_msg_clean(void);
void rx_msg_clean(void) {
    dbgPrintln(rx_buffer.data);
    dbgPrintln(F("Cleaning buffer ..."));
    memset(&rx_buffer, 0, sizeof(rx_buffer_t));
}

bool store_rx_msg_data(uint8_t data) {
    
    if (data < (RX_BUFFER_MAX_LEN-1)) {
        rx_buffer.data[rx_buffer.len] = data;
        rx_buffer.len++;

    } else {
        rx_msg_clean();
    }
}

bool processVendorSpecificEvent() {
    uint16_t i = 0;
    bool done = false;

    if (rx_buffer.len <= 3 ) {
        return done;
    }

    return done;
}

bool processCommandStatus() {
    uint16_t i = 0;
    bool done = false;

    if (rx_buffer.len == (2+(rx_buffer.data[2]) )) {
        done = true;
    }
    if (rx_buffer.data[2] && rx_buffer.data[3]) {
        if (!initDone) {
           initDone = true; 
        }
        dbgPrint(F("SUCCESS"));
    } else {
        dbgPrint(F("FAILED"));
    }
    return done;
}

bool processEvent() 
{
    uint16_t i = 0;
    bool done = false;

    if (rx_buffer.len <= 2 ) {
        return done;
    }

    switch (rx_buffer.data[1]) {
       
    case 0xFF:  // Vendor specific Event Opcode
        done = processVendorSpecificEvent();
    break;

    case 0x0F:  // Command Status
        done = processCommandStatus();
    break;
    
    case 0x0E:  // Command Complete
    case 0x3E:  // LE Event Opcode
    default:
    break;
    
    }
    return done;
}

void process_rx_msg_data() {
    uint16_t i = 0;
    bool done = false;

    switch (rx_buffer.data[0]) {
    case 4:  // Event
        done = processEvent();
    break;
     
    default:
    break;
    }

    if (done) {
        rx_msg_clean();
    }
}


void cc2541::poll() {
    dbgPrint(F("poll  TBD"));
    while (BLE.available()) {
        store_rx_msg_data(BLE.read());
        process_rx_msg_data();
    }

      GAP_MakeDiscoverable(DISCOVERABLE_UNIDIR, ADDRTYPE_PUBLIC, addr, 0, 0);
      //sendSetupMessage();
      dbgPrint("Make Discoverable");

  // We enter the if statement only when there is a ACI event available to be processed
/*
  if (lib_aci_event_get(&this->_aciState, &this->_aciData)) {
    aci_evt_t* aciEvt = &this->_aciData.evt;

    switch(aciEvt->evt_opcode) {

      // As soon as you reset the nRF8001 you will get an ACI Device Started Event

      case ACI_EVT_DEVICE_STARTED: {
        this->_aciState.data_credit_total = aciEvt->params.device_started.credit_available;
        switch(aciEvt->params.device_started.device_mode) {
          case ACI_DEVICE_SETUP:

            // When the device is in the setup mode

#ifdef NRF_8001_DEBUG
            Serial.println(F("Evt Device Started: Setup"));
#endif
            break;

          case ACI_DEVICE_STANDBY:
#ifdef NRF_8001_DEBUG
            Serial.println(F("Evt Device Started: Standby"));
#endif
            //Looking for an iPhone by sending radio advertisements
            //When an iPhone connects to us we will get an ACI_EVT_CONNECTED event from the nRF8001
            if (aciEvt->params.device_started.hw_error) {
              delay(20); //Handle the HW error event correctly.
            } else if (this->_bondStore && this->_bondStore->hasData()) {
              this->_dynamicDataSequenceNo = 1;
              this->_dynamicDataOffset = 0;

              unsigned char chunkSize;
              this->_bondStore->getData(&chunkSize, this->_dynamicDataOffset, sizeof(chunkSize));
              this->_dynamicDataOffset++;

              unsigned char chunkData[DYNAMIC_DATA_MAX_CHUNK_SIZE];
              this->_bondStore->getData(chunkData, this->_dynamicDataOffset, chunkSize);
              this->_dynamicDataOffset += chunkSize;

              lib_aci_write_dynamic_data(this->_dynamicDataSequenceNo, chunkData, chunkSize);
            } else {
              this->startAdvertising();
            }
            break;
        }
      }
      break; //ACI Device Started Event

      case ACI_EVT_CMD_RSP:
        //If an ACI command response event comes with an error -> stop
        if (ACI_STATUS_SUCCESS != aciEvt->params.cmd_rsp.cmd_status &&
            ACI_STATUS_TRANSACTION_CONTINUE != aciEvt->params.cmd_rsp.cmd_status &&
            ACI_STATUS_TRANSACTION_COMPLETE != aciEvt->params.cmd_rsp.cmd_status) {
          //ACI ReadDynamicData and ACI WriteDynamicData will have status codes of
          //TRANSACTION_CONTINUE and TRANSACTION_COMPLETE
          //all other ACI commands will have status code of ACI_STATUS_SCUCCESS for a successful command
#ifdef NRF_8001_DEBUG
          Serial.print(F("ACI Command "));
          Serial.println(aciEvt->params.cmd_rsp.cmd_opcode, HEX);
          Serial.print(F("Evt Cmd respone: Status "));
          Serial.println(aciEvt->params.cmd_rsp.cmd_status, HEX);
#endif
        } else {
          switch (aciEvt->params.cmd_rsp.cmd_opcode) {
            case ACI_CMD_READ_DYNAMIC_DATA: {
#ifdef NRF_8001_DEBUG
              Serial.print(F("Dynamic data read sequence "));
              Serial.print(aciEvt->params.cmd_rsp.params.read_dynamic_data.seq_no);
              Serial.print(F(": "));
              Serial.print(aciEvt->len - 4);
              Serial.print(F(": "));

              BLEUtil::printBuffer(aciEvt->params.cmd_rsp.params.read_dynamic_data.dynamic_data, aciEvt->len - 4);
#endif
              if (aciEvt->params.cmd_rsp.params.read_dynamic_data.seq_no == 1) {
                this->_dynamicDataOffset = 0;
              }

              unsigned char chunkLength = aciEvt->len - 4;

              this->_bondStore->putData(&chunkLength, this->_dynamicDataOffset, sizeof(chunkLength));
              this->_dynamicDataOffset++;

              this->_bondStore->putData(aciEvt->params.cmd_rsp.params.read_dynamic_data.dynamic_data, this->_dynamicDataOffset, chunkLength);
              this->_dynamicDataOffset += chunkLength;

              if (aciEvt->params.cmd_rsp.cmd_status == ACI_STATUS_TRANSACTION_CONTINUE) {
                lib_aci_read_dynamic_data();
              } else if (aciEvt->params.cmd_rsp.cmd_status == ACI_STATUS_TRANSACTION_COMPLETE) {
                this->startAdvertising();
              }
              break;
            }

            case ACI_CMD_WRITE_DYNAMIC_DATA: {
#ifdef NRF_8001_DEBUG
              Serial.print(F("Dynamic data write sequence "));
              Serial.print(this->_dynamicDataSequenceNo);
              Serial.println(F(" complete"));
#endif
              if (aciEvt->params.cmd_rsp.cmd_status == ACI_STATUS_TRANSACTION_CONTINUE) {
                this->_dynamicDataSequenceNo++;

                unsigned char chunkSize;
                this->_bondStore->getData(&chunkSize, this->_dynamicDataOffset, sizeof(chunkSize));
                this->_dynamicDataOffset++;

                unsigned char chunkData[DYNAMIC_DATA_MAX_CHUNK_SIZE];
                this->_bondStore->getData(chunkData, this->_dynamicDataOffset, chunkSize);
                this->_dynamicDataOffset += chunkSize;

                lib_aci_write_dynamic_data(this->_dynamicDataSequenceNo, chunkData, chunkSize);
              } else if (aciEvt->params.cmd_rsp.cmd_status == ACI_STATUS_TRANSACTION_COMPLETE) {
                delay(20);
                this->startAdvertising();
              }
              break;
            }

            case ACI_CMD_GET_DEVICE_VERSION:
              break;

            case ACI_CMD_GET_DEVICE_ADDRESS: {
#ifdef NRF_8001_DEBUG
              char address[18];

              BLEUtil::addressToString(aciEvt->params.cmd_rsp.params.get_device_address.bd_addr_own, address);

              Serial.print(F("Device address = "));
              Serial.println(address);

              Serial.print(F("Device address type = "));
              Serial.println(aciEvt->params.cmd_rsp.params.get_device_address.bd_addr_type, DEC);
#endif
              if (this->_eventListener) {
                this->_eventListener->BLEDeviceAddressReceived(*this, aciEvt->params.cmd_rsp.params.get_device_address.bd_addr_own);
              }
              break;
            }

            case ACI_CMD_GET_BATTERY_LEVEL: {
              float batteryLevel = aciEvt->params.cmd_rsp.params.get_battery_level.battery_level * 0.00352;
#ifdef NRF_8001_DEBUG
              Serial.print(F("Battery level = "));
              Serial.println(batteryLevel);
#endif
              if (this->_eventListener) {
                this->_eventListener->BLEDeviceBatteryLevelReceived(*this, batteryLevel);
              }
              break;
            }

            case ACI_CMD_GET_TEMPERATURE: {
              float temperature = aciEvt->params.cmd_rsp.params.get_temperature.temperature_value / 4.0;
#ifdef NRF_8001_DEBUG
              Serial.print(F("Temperature = "));
              Serial.println(temperature);
#endif
              if (this->_eventListener) {
                this->_eventListener->BLEDeviceTemperatureReceived(*this, temperature);
              }
              break;
            }
          }
        }
        break;

      case ACI_EVT_CONNECTED:
#ifdef NRF_8001_DEBUG
        char address[18];
        BLEUtil::addressToString(aciEvt->params.connected.dev_addr, address);

        Serial.print(F("Evt Connected "));
        Serial.println(address);
#endif
        this->_closedPipesCleared = false;
        this->_remoteServicesDiscovered = false;

        if (this->_eventListener) {
          this->_eventListener->BLEDeviceConnected(*this, aciEvt->params.connected.dev_addr);
        }

        this->_aciState.data_credit_available = this->_aciState.data_credit_total;
        break;

      case ACI_EVT_PIPE_STATUS: {
#ifdef NRF_8001_DEBUG
        Serial.println(F("Evt Pipe Status "));
#endif
        uint64_t closedPipes;
        memcpy(&closedPipes, aciEvt->params.pipe_status.pipes_closed_bitmap, sizeof(closedPipes));

#ifdef NRF_8001_DEBUG
        uint64_t openPipes;
        memcpy(&openPipes, aciEvt->params.pipe_status.pipes_open_bitmap, sizeof(openPipes));

        Serial.println((unsigned long)openPipes, HEX);
        Serial.println((unsigned long)closedPipes, HEX);
#endif
        bool discoveryFinished = lib_aci_is_discovery_finished(&this->_aciState);
        if (closedPipes == 0 && !discoveryFinished) {
          this->_closedPipesCleared = true;
        }

        for (int i = 0; i < this->_numLocalPipeInfo; i++) {
          struct localPipeInfo* localPipeInfo = &this->_localPipeInfo[i];

          if (localPipeInfo->txPipe) {
            localPipeInfo->txPipeOpen = lib_aci_is_pipe_available(&this->_aciState, localPipeInfo->txPipe);
          }

          if (localPipeInfo->txAckPipe) {
            localPipeInfo->txAckPipeOpen = lib_aci_is_pipe_available(&this->_aciState, localPipeInfo->txAckPipe);
          }

          bool subscribed = (localPipeInfo->txPipeOpen || localPipeInfo->txAckPipeOpen);

          if (localPipeInfo->characteristic->subscribed() != subscribed) {
            if (this->_eventListener) {
              this->_eventListener->BLEDeviceCharacteristicSubscribedChanged(*this, *localPipeInfo->characteristic, subscribed);
            }
          }
        }

        if (this->_closedPipesCleared && discoveryFinished && !this->_remoteServicesDiscovered) {
          if (!this->_remoteServicesDiscovered && this->_eventListener) {
            this->_remoteServicesDiscovered = true;

            this->_eventListener->BLEDeviceRemoteServicesDiscovered(*this);
          }
        }
        break;
      }

      case ACI_EVT_TIMING:
#ifdef NRF_8001_DEBUG
        Serial.print(F("Timing change received conn Interval: 0x"));
        Serial.println(aciEvt->params.timing.conn_rf_interval, HEX);
#endif
        break;

      case ACI_EVT_DISCONNECTED:
#ifdef NRF_8001_DEBUG
        Serial.println(F("Evt Disconnected/Advertising timed out"));
#endif
        // all characteristics unsubscribed on disconnect
        for (int i = 0; i < this->_numLocalPipeInfo; i++) {
          struct localPipeInfo* localPipeInfo = &this->_localPipeInfo[i];

          if (localPipeInfo->characteristic->subscribed()) {
            if (this->_eventListener) {
              this->_eventListener->BLEDeviceCharacteristicSubscribedChanged(*this, *localPipeInfo->characteristic, false);
            }
          }
        }

        if (this->_eventListener) {
          this->_eventListener->BLEDeviceDisconnected(*this);
        }

        if (this->_storeDynamicData) {
          lib_aci_read_dynamic_data();

          this->_storeDynamicData = false;
        } else {
          this->startAdvertising();
        }
        break;

      case ACI_EVT_BOND_STATUS:
#ifdef NRF_8001_DEBUG
        Serial.println(F("Evt Bond Status"));
        Serial.println(aciEvt->params.bond_status.status_code);
#endif
        this->_storeDynamicData = (aciEvt->params.bond_status.status_code == ACI_BOND_STATUS_SUCCESS) &&
                                    (this->_aciState.bonded != ACI_BOND_STATUS_SUCCESS);

        this->_aciState.bonded = aciEvt->params.bond_status.status_code;

        this->_remoteServicesDiscovered = false;

        if (aciEvt->params.bond_status.status_code == ACI_BOND_STATUS_SUCCESS && this->_eventListener) {
          this->_eventListener->BLEDeviceBonded(*this);
        }
        break;

      case ACI_EVT_DATA_RECEIVED: {
        uint8_t dataLen = aciEvt->len - 2;
        uint8_t pipe = aciEvt->params.data_received.rx_data.pipe_number;
#ifdef NRF_8001_DEBUG
        Serial.print(F("Data Received, pipe = "));
        Serial.println(aciEvt->params.data_received.rx_data.pipe_number, DEC);

        BLEUtil::printBuffer(aciEvt->params.data_received.rx_data.aci_data, dataLen);
#endif

        for (int i = 0; i < this->_numLocalPipeInfo; i++) {
          struct localPipeInfo* localPipeInfo = &this->_localPipeInfo[i];

          if (localPipeInfo->rxAckPipe == pipe || localPipeInfo->rxPipe == pipe) {
            if (localPipeInfo->rxAckPipe == pipe) {
              lib_aci_send_ack(&this->_aciState, localPipeInfo->rxAckPipe);
            }

            if (this->_eventListener) {
              this->_eventListener->BLEDeviceCharacteristicValueChanged(*this, *localPipeInfo->characteristic, aciEvt->params.data_received.rx_data.aci_data, dataLen);
            }
            break;
          }
        }

        for (int i = 0; i < this->_numRemotePipeInfo; i++) {
          struct remotePipeInfo* remotePipeInfo = &this->_remotePipeInfo[i];

          if (remotePipeInfo->rxPipe == pipe || remotePipeInfo->rxAckPipe == pipe || remotePipeInfo->rxReqPipe) {
            if (remotePipeInfo->rxAckPipe == pipe) {
             lib_aci_send_ack(&this->_aciState, remotePipeInfo->rxAckPipe);
            }

            if (this->_eventListener) {
              this->_eventListener->BLEDeviceRemoteCharacteristicValueChanged(*this, *remotePipeInfo->characteristic, aciEvt->params.data_received.rx_data.aci_data, dataLen);
            }
            break;
          }
        }
        break;
      }

      case ACI_EVT_DATA_CREDIT:
        this->_aciState.data_credit_available = this->_aciState.data_credit_available + aciEvt->params.data_credit.credit;
        break;

      case ACI_EVT_PIPE_ERROR:
        //See the appendix in the nRF8001 Product Specication for details on the error codes
#ifdef NRF_8001_DEBUG
        Serial.print(F("ACI Evt Pipe Error: Pipe #:"));
        Serial.print(aciEvt->params.pipe_error.pipe_number, DEC);
        Serial.print(F("  Pipe Error Code: 0x"));
        Serial.println(aciEvt->params.pipe_error.error_code, HEX);
#endif

        //Increment the credit available as the data packet was not sent.
        //The pipe error also represents the Attribute protocol Error Response sent from the peer and that should not be counted
        //for the credit.
        if (ACI_STATUS_ERROR_PEER_ATT_ERROR != aciEvt->params.pipe_error.error_code) {
          this->_aciState.data_credit_available++;
        } else if (this->_bondStore) {
          lib_aci_bond_request();
        }
        break;

      case ACI_EVT_HW_ERROR:
#ifdef NRF_8001_DEBUG
        Serial.print(F("HW error: "));
        Serial.println(aciEvt->params.hw_error.line_num, DEC);

        for(uint8_t counter = 0; counter <= (aciEvt->len - 3); counter++) {
          Serial.write(aciEvt->params.hw_error.file_name[counter]); //uint8_t file_name[20];
        }
        Serial.println();
#endif
        this->startAdvertising();
        break;
    }
  } else {
    //Serial.println(F("No ACI Events available"));
    // No event in the ACI Event queue and if there is no event in the ACI command queue the arduino can go to sleep
    // Arduino can go to sleep now
    // Wakeup from sleep from the RDYN line
  }
  */
}

bool cc2541::updateCharacteristicValue(BLECharacteristic& characteristic) {

  bool success = true;
  /*
  struct localPipeInfo* localPipeInfo = this->localPipeInfoForCharacteristic(characteristic);
#ifdef NRF_8001_DEBUG
  Serial.println(F("updateCharacteristicValue  TBD"));
#endif
  /*
  if (localPipeInfo) {
    if (localPipeInfo->advPipe && (this->_broadcastPipe == localPipeInfo->advPipe)) {
      success &= lib_aci_set_local_data(&this->_aciState, localPipeInfo->advPipe, (uint8_t*)characteristic.value(), characteristic.valueLength());
    }

    if (localPipeInfo->setPipe) {
      success &= lib_aci_set_local_data(&this->_aciState, localPipeInfo->setPipe, (uint8_t*)characteristic.value(), characteristic.valueLength());
    }

    if (localPipeInfo->txPipe && localPipeInfo->txPipeOpen) {
      if (this->canNotifyCharacteristic(characteristic)) {
        this->_aciState.data_credit_available--;
        success &= lib_aci_send_data(localPipeInfo->txPipe, (uint8_t*)characteristic.value(), characteristic.valueLength());
      } else {
        success = false;
      }
    }

    if (localPipeInfo->txAckPipe && localPipeInfo->txAckPipeOpen) {
      if (this->canIndicateCharacteristic(characteristic)) {
        this->_aciState.data_credit_available--;
        success &= lib_aci_send_data(localPipeInfo->txAckPipe, (uint8_t*)characteristic.value(), characteristic.valueLength());
      } else {
        success = false;
      }
    }
  }
*/
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
/*
  struct remotePipeInfo* remotePipeInfo = this->remotePipeInfoForCharacteristic(characteristic);

  if (remotePipeInfo) {
    unsigned char pipe = remotePipeInfo->rxPipe ? remotePipeInfo->rxPipe : remotePipeInfo->rxAckPipe;

    if (pipe) {
      success = lib_aci_close_remote_pipe(&this->_aciState, pipe);
    }
  }
*/
    dbgPrint(F("unsubcribeRemoteCharacteristic"));
    return success;
}

void cc2541::startAdvertising() {
/*
  uint16_t advertisingInterval = (this->_advertisingInterval * 16) / 10;

  if (this->_connectable) {
    if (this->_bondStore == NULL || this->_bondStore->hasData())   {
      lib_aci_connect(0, advertisingInterval);
    } else {
      lib_aci_bond(180, advertisingInterval);
    }
  } else {
    lib_aci_broadcast(0, advertisingInterval);
  }
*/
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
 /*
  bool setupMode = false;

  while (!setupMode) {
    if (lib_aci_event_get(&this->_aciState, &this->_aciData)) {
      aci_evt_t* aciEvt = &this->_aciData.evt;

      switch(aciEvt->evt_opcode) {
        case ACI_EVT_DEVICE_STARTED: {
          switch(aciEvt->params.device_started.device_mode) {
            case ACI_DEVICE_SETUP:
              //When the device is in the setup mode
#ifdef NRF_8001_DEBUG
              Serial.println(F("Evt Device Started: Setup"));
#endif
              setupMode = true;
              break;
          }
        }

        case ACI_EVT_CMD_RSP:
          setupMode = true;
          break;
      }
    } else {
      delay(1);
    }
  }
  */
  uint32_t counter = 0;
  while (!initDone) {
    if (counter > 10) {
       dbgPrint(F("waitForSetupMode ..."));
       break;
    }
    counter++;
    delay(100);
  }
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
