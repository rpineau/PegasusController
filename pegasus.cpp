//
//  nexdome.cpp
//  NexDome X2 plugin
//
//  Created by Rodolphe Pineau on 6/11/2016.


#include "pegasus.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif


CPegasusController::CPegasusController()
{
    m_globalStatus.nDeviceType = NONE;
    m_globalStatus.bReady = false;
    memset(m_globalStatus.szVersion,0,SERIAL_BUFFER_SIZE);
    m_globalStatus.nMotorType = STEPPER;

    m_nTargetPos = 0;
    m_nPosLimit = 0;
    m_bPosLimitEnabled = false;
    
    m_pSerx = NULL;
    m_pLogger = NULL;
}

CPegasusController::~CPegasusController()
{
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();
}

int CPegasusController::Connect(const char *pszPort)
{
    int nErr = DMFC_OK;
    int nDevice;

    if(!m_pSerx)
        return ERR_COMMNOLINK;

    // 19200 8N1
    if(m_pSerx->open(pszPort, 19200, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if (m_bDebugLog && m_pLogger) {
        snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::Connect] Connected.\n");
        m_pLogger->out(m_szLogBuffer);

        snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::Connect] Getting Firmware.\n");
        m_pLogger->out(m_szLogBuffer);
    }

    // get status so we can figure out what device we are connecting to.
    nErr = getDeviceType(nDevice);
    if(nErr) {
        return nErr;
    }
    // m_globalStatus.deviceType now contains the device type
    return nErr;
}

void CPegasusController::Disconnect()
{
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();
    m_bIsConnected = false;
}

#pragma mark move commands
int CPegasusController::haltFocuser()
{
    int nErr;

    nErr = dmfcCommand("H\n", NULL, 0);

    return nErr;
}

int CPegasusController::gotoPosition(int nPos)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];

    if (m_bPosLimitEnabled && nPos>m_nPosLimit)
        return ERR_LIMITSEXCEEDED;

    sprintf(szCmd,"M%d\n", nPos);
    nErr = dmfcCommand(szCmd, NULL, 0);
    m_nTargetPos = nPos;

    return nErr;
}

int CPegasusController::moveRelativeToPosision(int nSteps)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];

    sprintf(szCmd,"G%d\n", nSteps);
    nErr = dmfcCommand(szCmd, NULL, 0);

    return nErr;
}

#pragma mark command complete functions

int CPegasusController::isGoToComplete(bool &bComplete)
{
    int nErr = DMFC_OK;

    getPosition(m_globalStatus.nCurPos);
    if(m_globalStatus.nCurPos == m_nTargetPos)
        bComplete = true;
    else
        bComplete = false;
    return nErr;
}

int CPegasusController::isMotorMoving(bool &bMoving)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    // OK_SMFC or OK_DMFC
    nErr = dmfcCommand("I\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    try {
        if(atoi(szResp)) {
            bMoving = true;
            m_globalStatus.bMoving = MOVING;
        }
        else {
            bMoving = false;
            m_globalStatus.bMoving = IDLE;
        }
    } catch (const std::exception& e) {
        nErr = DMFC_BAD_CMD_RESPONSE;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::isMotorMoving] nError: %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
    }

    return nErr;
}

#pragma mark getters and setters
int CPegasusController::getStatus(int &nStatus)
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];

    // OK_SMFC or OK_DMFC
    nErr = dmfcCommand("#\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strstr(szResp,"OK_")) {
        if(strstr(szResp,"OK_SMFC")) {
            m_globalStatus.nDeviceType = SMFC;
        }
        else if(strstr(szResp,"OK_DMFC")) {
            m_globalStatus.nDeviceType = DMFC;
        }
        nStatus = DMFC_OK;
        nErr = DMFC_OK;
    }
    else {
        nErr = COMMAND_FAILED;
    }
    return nErr;
}

int CPegasusController::getConsolidatedStatus()
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];

    nErr = dmfcCommand("A\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // parse response
    nErr = parseResp(szResp, m_svParsedRespForA);
    try {
        if(m_svParsedRespForA[fSTATUS].find("OK_")) {
            m_globalStatus.bReady = true;
            if(strstr(szResp,"OK_SMFC")) {
                m_globalStatus.nDeviceType = SMFC;
            }
            else if(strstr(szResp,"OK_DMFC")) {
                m_globalStatus.nDeviceType = DMFC;
            }
        }
        else {
            m_globalStatus.bReady = false;
        }
        strncpy(m_globalStatus.szVersion,  m_svParsedRespForA[fVERSIONS].c_str(), SERIAL_BUFFER_SIZE);
        m_globalStatus.nMotorType = atoi(m_svParsedRespForA[fMOTOR_MODE].c_str());
        m_globalStatus.dTemperature = atof(m_svParsedRespForA[fTEMP].c_str());
        m_globalStatus.nCurPos = atoi(m_svParsedRespForA[fPOS].c_str());
        m_globalStatus.bMoving = atoi(m_svParsedRespForA[fMoving].c_str());
        m_globalStatus.nLedStatus = atoi(m_svParsedRespForA[fLED].c_str());
        m_globalStatus.bReverse = atoi(m_svParsedRespForA[fREVERSE].c_str());
        m_globalStatus.bEncodeEnabled = atoi(m_svParsedRespForA[fDIS_ENC].c_str());
        m_globalStatus.nBacklash = atoi(m_svParsedRespForA[fBACKLASH].c_str());

    } catch (const std::exception& e) {
        nErr = DMFC_BAD_CMD_RESPONSE;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getConsolidatedStatus] nError: %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
    }
    return nErr;
}

int CPegasusController::getMotoMaxSpeed(int &nSpeed)
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> sParsedResp;

    nErr = dmfcCommand("B\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // parse response
    nErr = parseResp(szResp, sParsedResp);
    try {
        nSpeed = atoi(sParsedResp[1].c_str());
    } catch (const std::exception& e) {
        nErr = DMFC_BAD_CMD_RESPONSE;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getMotoMaxSpeed] nError: %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
    }

    return nErr;
}

int CPegasusController::setMotoMaxSpeed(int nSpeed)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];

    sprintf(szCmd,"S%d\n", nSpeed);
    nErr = dmfcCommand(szCmd, NULL, 0);

    return nErr;
}

int CPegasusController::getBacklashComp(int &nSteps)
{
    int nErr;

    nErr = getConsolidatedStatus();
    nSteps = m_globalStatus.nBacklash;

    return nErr;
}

int CPegasusController::setBacklashComp(int nSteps)
{
    int nErr = DMFC_OK;
    char szCmd[SERIAL_BUFFER_SIZE];

    sprintf(szCmd,"C%d\n", nSteps);
    nErr = dmfcCommand(szCmd, NULL, 0);
    if(!nErr)
        m_globalStatus.nBacklash = nSteps;

    return nErr;
}


int CPegasusController::setEnableRotaryEncoder(bool bEnabled)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    char szCmd[SERIAL_BUFFER_SIZE];

    if(bEnabled)
        sprintf(szCmd,"E%d\n", R_ON);
    else
        sprintf(szCmd,"E%d\n", R_OFF);

    nErr = dmfcCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);

    return nErr;
}

int CPegasusController::getEnableRotaryEncoder(bool &bEnabled)
{
    int nErr;

    nErr = getConsolidatedStatus();
    bEnabled = m_globalStatus.bEncodeEnabled;

    return nErr;
}

int CPegasusController::getFirmwareVersion(char *pszVersion, int nStrMaxLen)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    nErr = dmfcCommand("V\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    strncpy(pszVersion, szResp, nStrMaxLen);
    return nErr;
}

int CPegasusController::getTemperature(double &dTemperature)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    nErr = dmfcCommand("T\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // convert response
    try {
        dTemperature = atof(szResp);
    } catch (const std::exception& e) {
        nErr = DMFC_BAD_CMD_RESPONSE;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getTemperature] nError: %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
    }

    return nErr;
}

int CPegasusController::getPosition(int &nPosition)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    nErr = dmfcCommand("P\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // convert response
    try {
        nPosition = atoi(szResp);
    } catch (const std::exception& e) {
        nErr = DMFC_BAD_CMD_RESPONSE;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getPosition] nError: %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
    }

    return nErr;
}

int CPegasusController::getLedStatus(int &nStatus)
{
    int nErr = DMFC_OK;
    int nLedStatus = 0;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> sParsedResp;


    nErr = dmfcCommand("P\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // parse response
    nErr = parseResp(szResp, sParsedResp);
    try {
        nLedStatus = atoi(sParsedResp[1].c_str());
        switch(nLedStatus) {
            case 0:
                nStatus = OFF;
                break;
            case 1:
                nStatus = ON;
                break;
        }
    } catch (const std::exception& e) {
        nErr = DMFC_BAD_CMD_RESPONSE;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer, LOG_BUFFER_SIZE, "[CPegasusController::getLedStatus] nError: %s\n%s\n", e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
    }

    return nErr;
}

int CPegasusController::setLedStatus(int nStatus)
{
    int nErr = DMFC_OK;
    char szCmd[SERIAL_BUFFER_SIZE];

    switch (nStatus) {
        case ON:
            snprintf(szCmd, SERIAL_BUFFER_SIZE, "L:%d\n", SWITCH_ON);
            break;
        case OFF:
            snprintf(szCmd, SERIAL_BUFFER_SIZE, "L:%d\n", SWITCH_OFF);
            break;

        default:
            break;
    }
    nErr = dmfcCommand(szCmd, NULL, 0);

    return nErr;
}

int CPegasusController::getMotorType(int &nType)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    nType = m_globalStatus.nMotorType;
    nErr = dmfcCommand("R\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strstr(szResp,"0")) {
        nType = STEPPER;
    }
    else if(strstr(szResp,"1")) {
        nType = DC;
    }
    else {
        nErr = ERR_CMDFAILED;
    }

    m_globalStatus.nMotorType = nType;
    return nErr;
}

int CPegasusController::setMotorType(int nType)
{
    int nErr = DMFC_OK;

    if(m_globalStatus.nDeviceType == DMFC) {
        switch (nType) {
            case STEPPER:
                nErr = dmfcCommand("R0\n", NULL, 0);
                break;
            case DC:
                nErr = dmfcCommand("R1\n", NULL, 0);
                break;

            default:
                break;
        }
    }
    else {
        nErr = COMMAND_FAILED;
    }
    return nErr;
}

int CPegasusController::getMotorDirection(int &nDir)
{
    int nErr = DMFC_OK;

    nErr = getConsolidatedStatus();
    if(m_globalStatus.bReverse) {
        nDir = REVERSE;
    }
    else {
        nDir = NORMAL;
    }

    return nErr;
}

int CPegasusController::setMotorDirection(int nDir)
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];
    char szCmd[SERIAL_BUFFER_SIZE];

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "N%d\n", nDir);
    nErr = dmfcCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(nDir == REVERSE) {
        m_globalStatus.bReverse = true;
    }
    else {

        m_globalStatus.bReverse = false;
    }
    return nErr;
}

int CPegasusController::syncMotorPosition(int nPos)
{
    int nErr = DMFC_OK;
    char szCmd[SERIAL_BUFFER_SIZE];

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "W%d\n", nPos);
    nErr = dmfcCommand(szCmd, NULL, 0);
    return nErr;
}

int CPegasusController::getRotaryEncPos(int &nPos)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    nErr = dmfcCommand("X\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // convert response
    try {
        nPos = atoi(szResp);
    } catch (const std::exception& e) {
        nErr = DMFC_BAD_CMD_RESPONSE;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getPosition] nError: %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
    }

    return nErr;
}

int CPegasusController::getDeviceType(int &nDevice)
{
    int nErr;

    nErr = getConsolidatedStatus();
    nDevice = m_globalStatus.nDeviceType;

    return nErr;
}

int CPegasusController::getPosLimit()
{
    return m_nPosLimit;
}

void CPegasusController::setPosLimit(int nLimit)
{
    m_nPosLimit = nLimit;
}

bool CPegasusController::isPosLimitEnabled()
{
    return m_bPosLimitEnabled;
}

void CPegasusController::enablePosLimit(bool bEnable)
{
    m_bPosLimitEnabled = bEnable;
}


int CPegasusController::setReverseEnable(bool bEnabled)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    char szCmd[SERIAL_BUFFER_SIZE];

    if(bEnabled)
        sprintf(szCmd,"N%d\n", REVERSE);
    else
        sprintf(szCmd,"N%d\n", NORMAL);

    nErr = dmfcCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);

    return nErr;
}

int CPegasusController::getReverseEnable(bool &bEnabled)
{
    int nErr;

    nErr = getConsolidatedStatus();
    bEnabled = m_globalStatus.bReverse;

    return nErr;
}


#pragma mark command and response functions

int CPegasusController::dmfcCommand(const char *pszszCmd, char *pszResult, int nResultMaxLen)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;

    m_pSerx->purgeTxRx();
    if (m_bDebugLog && m_pLogger) {
        snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CXagyl::filterWheelCommand] Sending %s\n",pszszCmd);
        m_pLogger->out(m_szLogBuffer);
    }
    nErr = m_pSerx->writeFile((void *)pszszCmd, strlen(pszszCmd), ulBytesWrite);
    m_pSerx->flushTx();

    // printf("Command %s sent. wrote %lu bytes\n", szCmd, ulBytesWrite);
    if(nErr){
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CXagyl::filterWheelCommand] writeFile nError.\n");
            m_pLogger->out(m_szLogBuffer);
        }
        return nErr;
    }

    if(pszResult) {
        // read response
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CXagyl::filterWheelCommand] Getting response.\n");
            m_pLogger->out(m_szLogBuffer);
        }
        nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
        if(nErr){
            if (m_bDebugLog && m_pLogger) {
                snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CXagyl::filterWheelCommand] readResponse nError.\n");
                m_pLogger->out(m_szLogBuffer);
            }
        }
        // printf("Got response : %s\n",resp);
        strncpy(pszResult, szResp, nResultMaxLen);
    }
    return nErr;
}

int CPegasusController::readResponse(char *pszRespBuffer, int nBufferLen)
{
    int nErr = DMFC_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;

    memset(pszRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = pszRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, 1, ulBytesRead, MAX_TIMEOUT);
        if(nErr) {
            if (m_bDebugLog && m_pLogger) {
                snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CNexDome::readResponse] readFile nError.\n");
                m_pLogger->out(m_szLogBuffer);
            }
            return nErr;
        }

        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CNexDome::readResponse] respBuffer = %s\n",pszRespBuffer);
            m_pLogger->out(m_szLogBuffer);
        }

        if (ulBytesRead !=1) {// timeout
            if (m_bDebugLog && m_pLogger) {
                snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CNexDome::readResponse] readFile Timeout.\n");
                m_pLogger->out(m_szLogBuffer);
            }
            nErr = DMFC_BAD_CMD_RESPONSE;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CNexDome::readResponse] ulBytesRead = %lu\n",ulBytesRead);
            m_pLogger->out(m_szLogBuffer);
        }
    } while (*pszBufPtr++ != '\n' && ulTotalBytesRead < nBufferLen );

    *pszBufPtr = 0; //remove the \n
    return nErr;
}


int CPegasusController::parseResp(char *pszResp, std::vector<std::string>  &svParsedResp)
{
    int n;
    std::string sSegment;
    std::vector<std::string> svSeglist;
    std::stringstream ssTmpGinf(pszResp);

    std::cout << "tmpGinf = " << ssTmpGinf.str() << "\n";
    // split the string into vector elements
    while(std::getline(ssTmpGinf, sSegment, ','))
    {
        svSeglist.push_back(sSegment);
    }
    // do we have all the fields ?
    if (svSeglist[0] == "V1")
        n =9;
    else
        n = 25;

    if(svSeglist.size() < n)
        return DMFC_BAD_CMD_RESPONSE;

    svParsedResp = svSeglist;
    return DMFC_OK;
}
