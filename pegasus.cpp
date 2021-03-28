//
//  nexdome.cpp
//  NexDome X2 plugin
//
//  Created by Rodolphe Pineau on 6/11/2016.

#include "pegasus.h"


CPegasusController::CPegasusController()
{
    m_globalStatus.nDeviceType = NONE;
    m_globalStatus.bReady = false;
    memset(m_globalStatus.szVersion,0,SERIAL_BUFFER_SIZE);
    m_globalStatus.nMotorType = STEPPER;

    m_nTargetPos = 0;
    m_nPosLimit = 0;
    m_bPosLimitEnabled = false;
    m_bAbborted = false;
    
    m_pSerx = NULL;
    m_pLogger = NULL;
    m_pSleeper = NULL;
    

#ifdef PEGA_DEBUG
#if defined(SB_WIN_BUILD)
	m_sLogfilePath = getenv("HOMEDRIVE");
	m_sLogfilePath += getenv("HOMEPATH");
	m_sLogfilePath += "\\PegasusLog.txt";
#elif defined(SB_LINUX_BUILD)
	m_sLogfilePath = getenv("HOME");
	m_sLogfilePath += "/PegasusLog.txt";
#elif defined(SB_MAC_BUILD)
	m_sLogfilePath = getenv("HOME");
	m_sLogfilePath += "/PegasusLog.txt";
#endif
	Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#ifdef	PEGA_DEBUG
	ltime = time(NULL);
	char *timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusController Constructor Called.\n", timestamp);
	fflush(Logfile);
#endif

}

CPegasusController::~CPegasusController()
{
#ifdef	PEGA_DEBUG
	// Close LogFile
	if (Logfile) fclose(Logfile);
#endif
}

int CPegasusController::Connect(const char *pszPort)
{
    int nErr = DMFC_OK;
    int nDevice;

    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusController::Connect Called %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    // 19200 8N1
    nErr = m_pSerx->open(pszPort, 19200, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1");
    if(nErr == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return nErr;

    m_pSleeper->sleep(2000); // apparently needed on old DMFC
    
#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusController::Connect connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif
	
    if (m_bDebugLog && m_pLogger) {
        snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::Connect] Connected.\n");
        m_pLogger->out(m_szLogBuffer);

        snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::Connect] Getting device type.\n");
        m_pLogger->out(m_szLogBuffer);
    }

    // get status so we can figure out what device we are connecting to.
#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusController::Connect getting device type\n", timestamp);
	fflush(Logfile);
#endif
    nErr = getDeviceType(nDevice);
    if(nErr) {
		m_bIsConnected = false;
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::Connect **** ERROR **** getting device type\n", timestamp);
		fflush(Logfile);
#endif
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
    char szResp[SERIAL_BUFFER_SIZE];
    
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = dmfcCommand("H\n", szResp, SERIAL_BUFFER_SIZE);
	m_bAbborted = true;
	
	return nErr;
}

int CPegasusController::gotoPosition(int nPos)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if (m_bPosLimitEnabled && nPos>m_nPosLimit)
        return ERR_LIMITSEXCEEDED;

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusController::gotoPosition moving to %d\n", timestamp, nPos);
    fflush(Logfile);
#endif

    sprintf(szCmd,"M:%d\n", nPos);
    nErr = dmfcCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    m_nTargetPos = nPos;

    return nErr;
}

int CPegasusController::moveRelativeToPosision(int nSteps)
{
    int nErr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusController::moveRelativeToPosision moving by %d steps\n", timestamp, nSteps);
    fflush(Logfile);
#endif
    m_nTargetPos = m_globalStatus.nCurPos + nSteps;
    nErr = gotoPosition(m_nTargetPos);
    return nErr;
}

#pragma mark command complete functions

int CPegasusController::isGoToComplete(bool &bComplete)
{
    int nErr = DMFC_OK;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    getPosition(m_globalStatus.nCurPos);
#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusController::isGoToComplete m_globalStatus.nCurPos = %d steps\n", timestamp, m_globalStatus.nCurPos);
    fprintf(Logfile, "[%s] CPegasusController::isGoToComplete m_nTargetPos = %d steps\n", timestamp, m_nTargetPos);
    fflush(Logfile);
#endif
    if(m_bAbborted) {
		bComplete = true;
		m_nTargetPos = m_globalStatus.nCurPos;
		m_bAbborted = false;
	}
    else if(m_globalStatus.nCurPos == m_nTargetPos)
        bComplete = true;
    else
        bComplete = false;
    return nErr;
}

int CPegasusController::isMotorMoving(bool &bMoving)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	

    // OK_SMFC or OK_DMFC
    nErr = dmfcCommand("I\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(atoi(szResp)) {
        bMoving = true;
        m_globalStatus.bMoving = MOVING;
    }
    else {
        bMoving = false;
        m_globalStatus.bMoving = IDLE;
    }

    return nErr;
}

#pragma mark getters and setters
int CPegasusController::getDeviceType(int &nDevice)
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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
        else if(strstr(szResp,"OK_DC")) {
            m_globalStatus.nDeviceType = FC;
        }
        nDevice = m_globalStatus.nDeviceType;
        nErr = DMFC_OK;
        getConsolidatedStatus();
    }
    else {
        nErr = COMMAND_FAILED;
    }

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusController::getDeviceType m_globalStatus.nDeviceType = %d\n", timestamp, m_globalStatus.nDeviceType);
    fflush(Logfile);
#endif
    
    return nErr;
}

int CPegasusController::getConsolidatedStatus()
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = dmfcCommand("A\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus about to parse response\n", timestamp);
	fflush(Logfile);
#endif

    // parse response
    nErr = parseResp(szResp, m_svParsedRespForA);
#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus response parsing done\n", timestamp);
	fflush(Logfile);
#endif
    if(m_svParsedRespForA.empty()) {
#ifdef PEGA_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus parsing returned an empty vector\n", timestamp);
        fflush(Logfile);
#endif
        return DMFC_BAD_CMD_RESPONSE;
    }

#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus Status = %s\n", timestamp, m_svParsedRespForA[fSTATUS].c_str());
	fflush(Logfile);
#endif

	if(m_svParsedRespForA[fSTATUS].find("OK_")!= -1) {
        m_globalStatus.bReady = true;
        if(m_svParsedRespForA[fSTATUS].find("_SMFC")!= -1) {
            m_globalStatus.nDeviceType = SMFC;
        }
        else if(m_svParsedRespForA[fSTATUS].find("_DMFC")!= -1) {
            m_globalStatus.nDeviceType = DMFC;
		}
        else if(m_svParsedRespForA[fSTATUS].find("_FC")!= -1) {
            m_globalStatus.nDeviceType = FC;
        }
    }
    else {
        m_globalStatus.bReady = false;
    }
    
    strncpy(m_globalStatus.szVersion,  m_svParsedRespForA[fVERSIONS].c_str(), SERIAL_BUFFER_SIZE);
    m_globalStatus.nMotorType = atoi(m_svParsedRespForA[fMOTOR_MODE].c_str());
    m_globalStatus.dTemperature = atof(m_svParsedRespForA[fTEMP].c_str());
    m_globalStatus.nCurPos = atoi(m_svParsedRespForA[fPOS].c_str());
    if(m_svParsedRespForA.size()>5){ // SMFC seems to not have these fields
        m_globalStatus.bMoving = atoi(m_svParsedRespForA[fMoving].c_str());
        m_globalStatus.nLedStatus = atoi(m_svParsedRespForA[fLED].c_str());
        m_globalStatus.bReverse = atoi(m_svParsedRespForA[fREVERSE].c_str());
        m_globalStatus.bEncodeEnabled = atoi(m_svParsedRespForA[fDIS_ENC].c_str());
        m_globalStatus.nBacklash = atoi(m_svParsedRespForA[fBACKLASH].c_str());
    }
#ifdef PEGA_DEBUG
    else {
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus response is only 5 fields\n", timestamp);
        fflush(Logfile);
    }
#endif
#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	
	fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus nDeviceType    : %d\n", timestamp, m_globalStatus.nDeviceType);
	fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus szVersion      : %s\n", timestamp, m_globalStatus.szVersion);
	fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus nMotorType     : %d\n", timestamp, m_globalStatus.nMotorType);
	fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus dTemperature   : %3.2f\n", timestamp, m_globalStatus.dTemperature);
	fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus nCurPos        : %d\n", timestamp, m_globalStatus.nCurPos);
	if(m_svParsedRespForA.size()>5){
		fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus bMoving        : %s\n", timestamp, m_globalStatus.bMoving?"Yes":"No");
		fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus nLedStatus     : %d\n", timestamp, m_globalStatus.nLedStatus);
		fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus bReverse       : %s\n", timestamp, m_globalStatus.bReverse?"Yes":"No");
		fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus bEncodeEnabled : %s\n", timestamp, m_globalStatus.bEncodeEnabled?"Yes":"No");
		fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus nBacklash      : %d\n", timestamp, m_globalStatus.nBacklash);
	}
	fflush(Logfile);
#endif

	
    return nErr;
}

int CPegasusController::getMotoMaxSpeed(int &nSpeed)
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> svParsedResp;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = dmfcCommand("B\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // parse response
    svParsedResp.clear();
    nErr = parseResp(szResp, svParsedResp);
#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusController::getMotoMaxSpeed sParsedResp length : %lu\n", timestamp, svParsedResp.size());
    fflush(Logfile);
#endif

    nSpeed = atoi(svParsedResp[1].c_str());

    return nErr;
}

int CPegasusController::setMotoMaxSpeed(int nSpeed)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	
    sprintf(szCmd,"S:%d\n", nSpeed);
    nErr = dmfcCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);

    return nErr;
}

int CPegasusController::getBacklashComp(int &nSteps)
{
    int nErr;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	
    nErr = getConsolidatedStatus();
    nSteps = m_globalStatus.nBacklash;

    return nErr;
}

int CPegasusController::setBacklashComp(int nSteps)
{
    int nErr = DMFC_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusController::setBacklashComp setting backlash comp : %s\n", timestamp, szCmd);
    fflush(Logfile);
#endif

    sprintf(szCmd,"C:%d\n", nSteps);
    nErr = dmfcCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(!nErr)
        m_globalStatus.nBacklash = nSteps;

    return nErr;
}


int CPegasusController::setEnableRotaryEncoder(bool bEnabled)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    char szCmd[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(bEnabled)
        sprintf(szCmd,"E:%d\n", R_ON);
    else
        sprintf(szCmd,"E:%d\n", R_OFF);

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusController::setEnableRotaryEncoder setting rotary enable : %s\n", timestamp, szCmd);
    fflush(Logfile);
#endif

    nErr = dmfcCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);

    return nErr;
}

int CPegasusController::getEnableRotaryEncoder(bool &bEnabled)
{
    int nErr;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = getConsolidatedStatus();
    bEnabled = m_globalStatus.bEncodeEnabled;

    return nErr;
}

int CPegasusController::getFirmwareVersion(char *pszVersion, int nStrMaxLen)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = dmfcCommand("T\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // convert response
    dTemperature = atof(szResp);

    return nErr;
}

int CPegasusController::getPosition(int &nPosition)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = dmfcCommand("P\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // convert response
    nPosition = atoi(szResp);
    m_globalStatus.nCurPos = nPosition;
    return nErr;
}

int CPegasusController::getLedStatus(int &nStatus)
{
    int nErr = DMFC_OK;
    int nLedStatus = 0;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> sParsedResp;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = dmfcCommand("P\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // parse response
    nErr = parseResp(szResp, sParsedResp);
    nLedStatus = atoi(sParsedResp[1].c_str());
    switch(nLedStatus) {
        case 0:
            nStatus = OFF;
            break;
        case 1:
            nStatus = ON;
            break;
    }

    return nErr;
}

int CPegasusController::setLedStatus(int nStatus)
{
    int nErr = DMFC_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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
    nErr = dmfcCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);

    return nErr;
}

int CPegasusController::getMotorType(int &nType)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nType = m_globalStatus.nMotorType;
#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusController::setMotorType getting motor type, m_globalStatus.nMotorType : %d\n", timestamp, nType);
	fflush(Logfile);
#endif

	nErr = dmfcCommand("R\n", szResp, SERIAL_BUFFER_SIZE);
	if(nErr) {
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::setMotorType Error getting motor type : %s\n", timestamp, szResp);
		fflush(Logfile);
#endif
        return nErr;
	}

	if(strstr(szResp,"1")) {
        nType = STEPPER;
    }
    else if(strstr(szResp,"0")) {
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
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusController::setMotorType setting motor type : %d\n", timestamp, nType);
    fflush(Logfile);
#endif

    if(m_globalStatus.nDeviceType == DMFC) {
        switch (nType) {
            case STEPPER:
                nErr = dmfcCommand("R:1\n", szResp, SERIAL_BUFFER_SIZE);
                break;
            case DC:
                nErr = dmfcCommand("R:0\n", szResp, SERIAL_BUFFER_SIZE);
                break;

            default:
                break;
        }
    }

    return nErr;
}

int CPegasusController::syncMotorPosition(int nPos)
{
    int nErr = DMFC_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
	char szResp[SERIAL_BUFFER_SIZE];
    
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "W:%d\n", nPos);
    nErr = dmfcCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    nErr |= getConsolidatedStatus();
    return nErr;
}

int CPegasusController::getRotaryEncPos(int &nPos)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = dmfcCommand("X\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // convert response
    nPos = atoi(szResp);

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
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(bEnabled)
        sprintf(szCmd,"N:%d\n", REVERSE);
    else
        sprintf(szCmd,"N:%d\n", NORMAL);

#ifdef PEGA_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CPegasusController::setReverseEnable setting reverse : %s\n", timestamp, szCmd);
    fflush(Logfile);
#endif

    nErr = dmfcCommand(szCmd, szResp, SERIAL_BUFFER_SIZE);

#ifdef PEGA_DEBUG
    if(nErr) {
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CPegasusController::setReverseEnable **** ERROR **** setting reverse (\"%s\") : %d\n", timestamp, szCmd, nErr);
        fflush(Logfile);
    }
#endif

    return nErr;
}

int CPegasusController::getReverseEnable(bool &bEnabled)
{
    int nErr;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = getConsolidatedStatus();
    bEnabled = m_globalStatus.bReverse;

    return nErr;
}

void CPegasusController::debugLinked(void)
{
#ifdef PEGA_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CPegasusController::debugLinked **** ERROR **** m_bLinked is false !!!!!! \n", timestamp);
        fflush(Logfile);
#endif

}

#pragma mark command and response functions

int CPegasusController::dmfcCommand(const char *pszszCmd, char *pszResult, int nResultMaxLen)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    m_pSerx->purgeTxRx();
    if (m_bDebugLog && m_pLogger) {
        snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::dmfcCommand] Sending %s\n",pszszCmd);
        m_pLogger->out(m_szLogBuffer);
    }
#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusController::dmfcCommand Sending %s\n", timestamp, pszszCmd);
	fflush(Logfile);
#endif
    nErr = m_pSerx->writeFile((void *)pszszCmd, strlen(pszszCmd), ulBytesWrite);
    m_pSerx->flushTx();

    // printf("Command %s sent. wrote %lu bytes\n", szCmd, ulBytesWrite);
    if(nErr){
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::dmfcCommand] writeFile Error.\n");
            m_pLogger->out(m_szLogBuffer);
        }
        return nErr;
    }

    if(pszResult) {
        // read response
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::dmfcCommand] Getting response.\n");
            m_pLogger->out(m_szLogBuffer);
        }
        nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
        if(nErr){
            if (m_bDebugLog && m_pLogger) {
                snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::dmfcCommand] readResponse Error.\n");
                m_pLogger->out(m_szLogBuffer);
            }
        }
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::dmfcCommand response \"%s\"\n", timestamp, szResp);
		fflush(Logfile);
#endif
        // printf("Got response : %s\n",resp);
        strncpy(pszResult, szResp, nResultMaxLen);
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::dmfcCommand response copied to pszResult : \"%s\"\n", timestamp, pszResult);
		fflush(Logfile);
#endif
    }
    return nErr;
}

int CPegasusController::readResponse(char *pszRespBuffer, int nBufferLen)
{
    int nErr = DMFC_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    memset(pszRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = pszRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, 1, ulBytesRead, MAX_TIMEOUT);
        if(nErr) {
            if (m_bDebugLog && m_pLogger) {
                snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::readResponse] readFile Error.\n");
                m_pLogger->out(m_szLogBuffer);
            }
            return nErr;
        }

        if (ulBytesRead !=1) {// timeout
            if (m_bDebugLog && m_pLogger) {
                snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::readResponse] readFile Timeout.\n");
                m_pLogger->out(m_szLogBuffer);
            }
#ifdef PEGA_DEBUG
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(Logfile, "[%s] CPegasusController::readResponse timeout\n", timestamp);
			fflush(Logfile);
#endif
            nErr = ERR_NORESPONSE;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::readResponse] ulBytesRead = %lu\n",ulBytesRead);
            m_pLogger->out(m_szLogBuffer);
        }
    } while (*pszBufPtr++ != '\n' && ulTotalBytesRead < nBufferLen );

    if(ulTotalBytesRead)
        *(pszBufPtr-1) = 0; //remove the \n

    return nErr;
}


int CPegasusController::parseResp(char *pszResp, std::vector<std::string>  &svParsedResp)
{
    std::string sSegment;
    std::vector<std::string> svSeglist;
    std::stringstream ssTmp(pszResp);

#ifdef PEGA_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusController::parseResp parsing \"%s\"\n", timestamp, pszResp);
	fflush(Logfile);
#endif
	svParsedResp.clear();
    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, ':'))
    {
        svSeglist.push_back(sSegment);
#ifdef PEGA_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] CPegasusController::parseResp sSegment : %s\n", timestamp, sSegment.c_str());
        fflush(Logfile);
#endif
    }

    svParsedResp = svSeglist;


    return DMFC_OK;
}
