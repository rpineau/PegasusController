//
//  dmfc.h
//  NexDome
//
//  Created by Rodolphe Pineau on 2017/05/30.
//  NexDome X2 plugin

#ifndef __PEGASUS_C__
#define __PEGASUS_C__
#include <math.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include <exception>
#include <typeinfo>
#include <stdexcept>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"

#define PEGA_DEBUG

#ifdef PEGA_DEBUG
#if defined(SB_WIN_BUILD)
#define PEGA_LOGFILENAME "C:\\PegasusLog.txt"
#elif defined(SB_LINUX_BUILD)
#define PEGA_LOGFILENAME "/tmp/PegasusLog.txt"
#elif defined(SB_MAC_BUILD)
#define PEGA_LOGFILENAME "/tmp/PegasusLog.txt"
#endif
#endif


#define SERIAL_BUFFER_SIZE 256
#define MAX_TIMEOUT 1000
#define LOG_BUFFER_SIZE 256

enum DMFC_Errors    {DMFC_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, DMFC_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum DeviceType     {NONE = 0, DMFC, SMFC};
enum MotorType      {DC = 0, STEPPER};
enum GetLedStatus   {OFF = 0, ON};
enum SetLEdStatus   {SWITCH_OFF = 1, SWITCH_ON};
enum MotorDir       {NORMAL = 0 , REVERSE};
enum MotorStatus    {IDLE = 0, MOVING};
enum RotaryEnable   {R_ON = 0, R_OFF};


typedef struct {
    int     nDeviceType;
    bool    bReady;
    char    szVersion[SERIAL_BUFFER_SIZE];
    int     nMotorType;
    double  dTemperature;
    int     nCurPos;
    bool    bMoving;
    int     nLedStatus;
    bool    bReverse;
    bool    bEncodeEnabled;
    int     nBacklash;
} dmfcStatus;

// field indexes in response for A command
#define fSTATUS     0
#define fVERSIONS   1
#define fMOTOR_MODE 2
#define fTEMP       3
#define fPOS        4
#define fMoving     5
#define fLED        6
#define fREVERSE    7
#define fDIS_ENC    8
#define fBACKLASH   9

class CPegasusController
{
public:
    CPegasusController();
    ~CPegasusController();

    int         Connect(const char *pszPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; };
    void        setLogger(LoggerInterface *pLogger) { m_pLogger = pLogger; };

    // move commands
    int         haltFocuser();
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);
    int         isMotorMoving(bool &bMoving);

    // getter and setter
    void        setDebugLog(bool bEnable) {m_bDebugLog = bEnable; };

    int         getStatus(int &nStatus);
    int         getConsolidatedStatus(void);

    int         getMotoMaxSpeed(int &nSpeed);
    int         setMotoMaxSpeed(int nSpeed);

    int         getBacklashComp(int &nSteps);
    int         setBacklashComp(int nSteps);

    int         setEnableRotaryEncoder(bool bEnabled);
    int         getEnableRotaryEncoder(bool &bEnabled);

    int         getFirmwareVersion(char *pszVersion, int nStrMaxLen);
    int         getTemperature(double &dTemperature);

    int         getPosition(int &nPosition);

    int         getLedStatus(int &nStatus);
    int         setLedStatus(int nStatus);

    int         getMotorType(int &nType);
    int         setMotorType(int nType);

    int         syncMotorPosition(int nPos);

    int         getRotaryEncPos(int &nPos);  // same as getMotoMaxSpeed

    int         getDeviceType(int &nDevice);

    int         getPosLimit(void);
    void        setPosLimit(int nLimit);

    bool        isPosLimitEnabled(void);
    void        enablePosLimit(bool bEnable);

    int         setReverseEnable(bool bEnabled);
    int         getReverseEnable(bool &bEnabled);

    void        debugLinked(void);

protected:

    int             dmfcCommand(const char *pszCmd, char *pszResult, int nResultMaxLen);
    int             readResponse(char *pszRespBuffer, int nBufferLen);
    int             parseResp(char *pszResp, std::vector<std::string>  &sParsedRes);


    SerXInterface   *m_pSerx;
    LoggerInterface *m_pLogger;

    bool            m_bDebugLog;
    bool            m_bIsConnected;
    char            m_szFirmwareVersion[SERIAL_BUFFER_SIZE];
    char            m_szLogBuffer[LOG_BUFFER_SIZE];

    std::vector<std::string>    m_svParsedRespForA;

    dmfcStatus      m_globalStatus;
    int             m_nTargetPos;
    int             m_nPosLimit;
    bool            m_bPosLimitEnabled;

#ifdef PEGA_DEBUG
	// timestamp for logs
	char *timestamp;
	time_t ltime;
	FILE *Logfile;	  // LogFile
#endif

};

#endif //__PEGASUS_C__
