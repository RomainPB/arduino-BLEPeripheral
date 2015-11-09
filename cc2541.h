#ifndef _CC_2541_H_
#define _CC_2541_H_

#include "Arduino.h"

//#include <utility/lib_aci.h>
#include "utility/HCI/hci.h"
#include "BLEDevice.h"


extern bool testOK;

/** Data type for ACI commands and events */
#define HAL_HCI_MAX_LENGTH    32

typedef struct _hci_packed_ {
    uint8_t status_byte;
    uint8_t buffer[HAL_HCI_MAX_LENGTH+1];
}  hal_hci_data_t;


class cc2541 : protected BLEDevice
{
  friend class BLEPeripheral;

  protected:
    struct localPipeInfo {
      BLECharacteristic* characteristic;

      unsigned short     valueHandle;
      unsigned short     configHandle;

      unsigned char      advPipe;
      unsigned char      txPipe;
      unsigned char      txAckPipe;
      unsigned char      rxPipe;
      unsigned char      rxAckPipe;
      unsigned char      setPipe;

      bool               txPipeOpen;
      bool               txAckPipeOpen;
    };

    struct remotePipeInfo {
      BLERemoteCharacteristic*  characteristic;

      unsigned char             txPipe;
      unsigned char             txAckPipe;
      unsigned char             rxPipe;
      unsigned char             rxAckPipe;
      unsigned char             rxReqPipe;
    };

    cc2541(unsigned char req, unsigned char rdy, unsigned char rst);

    virtual ~cc2541();

    virtual void begin(unsigned char advertisementDataType,
                unsigned char advertisementDataLength,
                const unsigned char* advertisementData,
                unsigned char scanDataType,
                unsigned char scanDataLength,
                const unsigned char* scanData,
                BLELocalAttribute** localAttributes,
                unsigned char numLocalAttributes,
                BLERemoteAttribute** remoteAttributes,
                unsigned char numRemoteAttributes);

    virtual void poll();

    virtual void startAdvertising();
    virtual void disconnect();

    virtual bool updateCharacteristicValue(BLECharacteristic& characteristic);
    virtual bool broadcastCharacteristic(BLECharacteristic& characteristic);
    virtual bool canNotifyCharacteristic(BLECharacteristic& characteristic);
    virtual bool canIndicateCharacteristic(BLECharacteristic& characteristic);

    virtual bool canReadRemoteCharacteristic(BLERemoteCharacteristic& characteristic);
    virtual bool readRemoteCharacteristic(BLERemoteCharacteristic& characteristic);
    virtual bool canWriteRemoteCharacteristic(BLERemoteCharacteristic& characteristic);
    virtual bool writeRemoteCharacteristic(BLERemoteCharacteristic& characteristic, const unsigned char value[], unsigned char length);
    virtual bool canSubscribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic);
    virtual bool subscribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic);
    virtual bool canUnsubscribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic);
    virtual bool unsubcribeRemoteCharacteristic(BLERemoteCharacteristic& characteristic);

    virtual void requestAddress();
    virtual void requestTemperature();
    virtual void requestBatteryLevel();

  private:
    void waitForSetupMode();
    void sendSetupMessage(hal_hci_data_t* data);
    void sendSetupMessage(hal_hci_data_t* setupMsg, unsigned char type, unsigned short& offset);

    struct localPipeInfo* localPipeInfoForCharacteristic(BLECharacteristic& characteristic);
    struct remotePipeInfo* remotePipeInfoForCharacteristic(BLERemoteCharacteristic& characteristic);

  private:

    struct localPipeInfo*       _localPipeInfo;
    unsigned char               _numLocalPipeInfo;
    unsigned char               _broadcastPipe;

    bool                        _closedPipesCleared;
    bool                        _remoteServicesDiscovered;
    struct remotePipeInfo*      _remotePipeInfo;
    unsigned char               _numRemotePipeInfo;

    unsigned int                _dynamicDataOffset;
    unsigned char               _dynamicDataSequenceNo;
    bool                        _storeDynamicData;
};

#define dbgPrint(x) Serial1.println(x)
#define dbgPrintln(x) Serial1.println(x)
#endif
