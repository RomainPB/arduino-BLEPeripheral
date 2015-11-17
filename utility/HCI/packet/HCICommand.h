#ifndef HCICommand_h
#define HCICommand_h

#include "HCIPacket.h"

class HCICommand : public HCIPacket {
    //variables
    public:
    protected:
    private:

    //functions
    public:
        HCICommand(HCIProcessAnswer *pa):HCIPacket(pa){};
        ~HCICommand();
    bool process(void){return true;};      
        protected:
        private:
        HCICommand( const HCICommand &c );
        HCICommand();// default constructor is not valid
        HCICommand& operator=( const HCICommand &c );
        bool processVendorSpecificEvent(void);
        bool processCommandStatus(void);
        bool processCommandComplete(void);
};

#endif // HCICommand_h
