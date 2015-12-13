#ifndef HCISpecificEvent_h
#define HCISpecificEvent_h

#include "HCIEvent.h"
#include "..\..\ASME\HciUart.h"


class HCISpecificEvent : public HCIEvent {

    
     //variables
     public:
     protected:
     private:
     
     //functions
     public:
     HCISpecificEvent(HciUart *_bleUart);
     ~HCISpecificEvent();
     bool process(void);            
     
     
     protected:
     private:
     HCISpecificEvent(void); // protect unwanted default constructor

     HCISpecificEvent( const HCISpecificEvent &c );
     HCISpecificEvent& operator=( const HCISpecificEvent &c );
    };

#endif // HCISpecificEvent_h
