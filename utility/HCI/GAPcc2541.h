#ifndef _GAP_2541_H_
#define _GAP_2541_H_

#include "../HCI/hci.h"
#define GAP_DEVICE_INIT_CMD          0xFE00
#define GAP_MAKE_DISCOVERABLE_CMD    0xFE06
#define GAP_ADV_DATA_UPD_CMD         0xFE07

#define GAP_DeviceInitDone           0x0600
#define GAP_DeviceDiscovery          0x0601
#define GAP_AdvertDataUpdateDone     0x0602
#define GAP_MakeDiscoverableDone     0x0603
#define GAP_EndDiscoverableDone      0x0604
#define GAP_LinkEstablished          0x0605
#define GAP_LinkTerminated           0x0606
#define GAP_LinkParameterUpdate      0x0607
#define GAP_RandomAddressChanged     0x0608
#define GAP_SignatureUpdated         0x0609
#define GAP_AuthenticationComplete   0x060A
#define GAP_PasskeyNeeded            0x060B
#define GAP_SlaveRequestedSecurity   0x060C
#define GAP_DeviceInformation        0x060D
#define GAP_BondComplete             0x060E
#define GAP_PairingRequested         0x060F
#define GAP_CommandStatus            0x067F


//-------------- ERROR CODE ---------------
#define GAP_ERR_SUCCESS 0x00
#define GAP_ERR_FAILURE 0x00
#define GAP_ERR_INVALIDPARAMETER 0x02
#define GAP_ERR_INVALID_TASK 0x03
#define GAP_ERR_MSG_BUFFER_NOT_AVAIL 0x04
#define GAP_ERR_INVALID_MSG_POINTER 0x05
#define GAP_ERR_INVALID_EVENT_ID 0x06
#define GAP_ERR_INVALID_INTERRUPT_ID 0x07
#define GAP_ERR_NO_TIMER_AVAIL 0x08
#define GAP_ERR_NV_ITEM_UNINIT 0x09
#define GAP_ERR_NV_OPER_FAILED 0x0A
#define GAP_ERR_INVALID_MEM_SIZE 0x0B
#define GAP_ERR_NV_BAD_ITEM_LEN 0x0C
#define GAP_ERR_bleNotReady 0x10
#define GAP_ERR_bleAlreadyInRequestedMode 0x11
#define GAP_ERR_bleIncorrectMode 0x12
#define GAP_ERR_bleMemAllocError 0x13
#define GAP_ERR_bleNotConnected 0x14
#define GAP_ERR_bleNoResources 0x15
#define GAP_ERR_blePending 0x16
#define GAP_ERR_bleTimeout 0x17
#define GAP_ERR_bleInvalidRange 0x18
#define GAP_ERR_bleLinkEncrypted 0x19
#define GAP_ERR_bleProcedureComplete 0x1A
#define GAP_ERR_bleGAPUserCanceled 0x30
#define GAP_ERR_bleGAPConnNotAcceptable 0x31
#define GAP_ERR_bleGAPBondRejected 0x32
#define GAP_ERR_bleInvalidPD 0x40



#define GAP_ERROR_CODE_POS          HCI_GAP_CODE_POS+2
#define GAP_CMD_OPCODE_POS          HCI_GAP_CODE_POS+3
#endif
