#ifndef HCIEvent_h
#define HCIEvent_h

#include "HCIPacket.h"

class HCIEvent : public HCIPacket {    
    
    //variables
    public:
    protected:
    private:  

    //functions
    public:
        HCIEvent(HCIProcessAnswer *pa):HCIPacket(pa){};
        ~HCIEvent();
        bool process(void);                       
    protected:
    private:
        HCIEvent( const HCIEvent &c );
        HCIEvent();// default constructor is not valid 
        HCIEvent& operator=( const HCIEvent &c );
        bool waitingCmdAnswer(void);
        bool processVendorSpecificEvent(void);
        bool processCommandStatus(void);
        bool processCommandComplete(void);
};

#endif // HCIEvent_h
