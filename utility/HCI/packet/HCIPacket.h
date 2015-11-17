/* 
* HCIPacket.h
*
* Created: 11/15/2015 7:11:58 PM
* Author: smkk
*/
#ifndef __HCIPACKET_H__
#define __HCIPACKET_H__

class HCIProcessAnswer;
typedef enum {
        noError,
        idle,
        started,
        partial,
        error
}messageProcessedE;

class HCIPacket
{         
    
    //variables
    public:
    protected:
    HCIProcessAnswer *pa;
    messageProcessedE messageProcessed;
    private:

    //functions
    public:
	    HCIPacket(HCIProcessAnswer *_pa):pa(_pa){resetAnswerCompleted();};
	    ~HCIPacket();
        virtual bool process(void){};
        inline virtual bool isProcessed(void){return messageProcessed==noError;};
        inline void resetAnswerCompleted(void){messageProcessed=idle;};
    protected:
    private:
    HCIPacket(); // default constructor is not valid 
	HCIPacket( const HCIPacket &c );
	//HCIPacket& operator=( const HCIPacket &c );

}; //HCIPacket

#endif //__HCIPACKET_H__
