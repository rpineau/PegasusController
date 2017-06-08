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

#ifdef	PEGA_DEBUG
	Logfile = fopen(PEGA_LOGFILENAME, "w");
	ltime = time(NULL);
	char *timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CPegasusController Constructor Called.\n", timestamp);
	fflush(Logfile);
#endif

}

CPegasusController::~CPegasusController()
{
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();
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
    if(m_pSerx->open(pszPort, 19200, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1") == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

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

        snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::Connect] Getting Firmware.\n");
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

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = dmfcCommand("H\n", NULL, 0);

    return nErr;
}

int CPegasusController::gotoPosition(int nPos)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    sprintf(szCmd,"G%d\n", nSteps);
    nErr = dmfcCommand(szCmd, NULL, 0);

    return nErr;
}

#pragma mark command complete functions

int CPegasusController::isGoToComplete(bool &bComplete)
{
    int nErr = DMFC_OK;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	

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
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	

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
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::isMotorMoving] Exception: %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
	} catch (...) {
		nErr = DMFC_BAD_CMD_RESPONSE;
		if (m_bDebugLog && m_pLogger) {
			snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::isMotorMoving] Exception: %s\n",szResp);
			m_pLogger->out(m_szLogBuffer);
		}
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::isMotorMoving **** Exception ****  : %s\n", timestamp, szResp);
		fflush(Logfile);
#endif
	}

    return nErr;
}

#pragma mark getters and setters
int CPegasusController::getStatus(int &nStatus)
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
    try {
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

    } catch (const std::exception& e) {
        nErr = DMFC_BAD_CMD_RESPONSE;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getConsolidatedStatus] Exception: %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus **** Exception **** getting Consolidated status : %s\n%s\n", timestamp, e.what(), szResp);
		fflush(Logfile);
#endif

	} catch (...) {
		nErr = DMFC_BAD_CMD_RESPONSE;
		if (m_bDebugLog && m_pLogger) {
			snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getConsolidatedStatus] Exception : %s\n",szResp);
			m_pLogger->out(m_szLogBuffer);
		}
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::getConsolidatedStatus **** Exception **** getting Consolidated status : %s\n", timestamp, szResp);
		fflush(Logfile);
#endif
	}
    return nErr;
}

int CPegasusController::getMotoMaxSpeed(int &nSpeed)
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> sParsedResp;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	

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
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getMotoMaxSpeed] Exception : %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
	} catch (...) {
		nErr = DMFC_BAD_CMD_RESPONSE;
		if (m_bDebugLog && m_pLogger) {
			snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getMotoMaxSpeed] Exception : %s\n",szResp);
			m_pLogger->out(m_szLogBuffer);
		}
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::getMotoMaxSpeed **** Exception ****  : %s\n", timestamp, szResp);
		fflush(Logfile);
#endif
	}

    return nErr;
}

int CPegasusController::setMotoMaxSpeed(int nSpeed)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	
    sprintf(szCmd,"S%d\n", nSpeed);
    nErr = dmfcCommand(szCmd, NULL, 0);

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
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	
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
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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
    try {
        dTemperature = atof(szResp);
    } catch (const std::exception& e) {
        nErr = DMFC_BAD_CMD_RESPONSE;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getTemperature] Exception: %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
	} catch (...) {
		nErr = DMFC_BAD_CMD_RESPONSE;
		if (m_bDebugLog && m_pLogger) {
			snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getTemperature] Exception: %s\n",szResp);
			m_pLogger->out(m_szLogBuffer);
		}
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::getTemperature **** Exception ****  : %s\n", timestamp, szResp);
		fflush(Logfile);
#endif
	}

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
    try {
        nPosition = atoi(szResp);
    } catch (const std::exception& e) {
        nErr = DMFC_BAD_CMD_RESPONSE;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getPosition] Exception: %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
	} catch (...) {
		nErr = DMFC_BAD_CMD_RESPONSE;
		if (m_bDebugLog && m_pLogger) {
			snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getPosition] Exception: %s\n",szResp);
			m_pLogger->out(m_szLogBuffer);
		}
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::getPosition **** Exception ****  : %s\n", timestamp, szResp);
		fflush(Logfile);
#endif
	}

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
            snprintf(m_szLogBuffer, LOG_BUFFER_SIZE, "[CPegasusController::getLedStatus] Exception: %s\n%s\n", e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
	} catch (...) {
		nErr = DMFC_BAD_CMD_RESPONSE;
		if (m_bDebugLog && m_pLogger) {
			snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getLedStatus] Exception: %s\n",szResp);
			m_pLogger->out(m_szLogBuffer);
		}
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::getLedStatus **** Exception ****  : %s\n", timestamp, szResp);
		fflush(Logfile);
#endif
	}

    return nErr;
}

int CPegasusController::setLedStatus(int nStatus)
{
    int nErr = DMFC_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
	
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
    nErr = dmfcCommand(szCmd, NULL, 0);

    return nErr;
}

int CPegasusController::getMotorType(int &nType)
{
    int nErr = DMFC_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "W%d\n", nPos);
    nErr = dmfcCommand(szCmd, NULL, 0);
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
    try {
        nPos = atoi(szResp);
    } catch (const std::exception& e) {
        nErr = DMFC_BAD_CMD_RESPONSE;
        if (m_bDebugLog && m_pLogger) {
            snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getPosition] Exception: %s\n%s\n",e.what(), szResp);
            m_pLogger->out(m_szLogBuffer);
        }
	} catch (...) {
		nErr = DMFC_BAD_CMD_RESPONSE;
		if (m_bDebugLog && m_pLogger) {
			snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CPegasusController::getPosition] Exception: %s\n",szResp);
			m_pLogger->out(m_szLogBuffer);
		}
#ifdef PEGA_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::getPosition **** Exception ****  : %s\n", timestamp, szResp);
		fflush(Logfile);
#endif
	}

    return nErr;
}

int CPegasusController::getDeviceType(int &nDevice)
{
    int nErr;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = getConsolidatedStatus();
#ifdef PEGA_DEBUG
	if(nErr) {
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CPegasusController::Connect **** ERROR **** getting Consolidated status : %d\n", timestamp, nErr);
		fflush(Logfile);
	}
#endif
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
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

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
    }
    // do we have all the fields ?
    if (svSeglist.size()<10)
        return DMFC_BAD_CMD_RESPONSE;

    svParsedResp = svSeglist;
    return DMFC_OK;
}
