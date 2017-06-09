#include <stdio.h>
#include <string.h>
#include "x2focuser.h"

#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/mutexinterface.h"
#include "../../licensedinterfaces/basicstringinterface.h"
#include "../../licensedinterfaces/tickcountinterface.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serialportparams2interface.h"

X2Focuser::X2Focuser(const char* pszDisplayName, 
												const int& nInstanceIndex,
												SerXInterface						* pSerXIn, 
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn, 
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface				* pIniUtilIn,
												LoggerInterface						* pLoggerIn,
												MutexInterface						* pIOMutexIn,
												TickCountInterface					* pTickCountIn)

{
	m_pSerX							= pSerXIn;		
	m_pTheSkyXForMounts				= pTheSkyXIn;
	m_pSleeper						= pSleeperIn;
	m_pIniUtil						= pIniUtilIn;
	m_pLogger						= pLoggerIn;	
	m_pIOMutex						= pIOMutexIn;
	m_pTickCount					= pTickCountIn;

	m_bLinked = false;
	m_nPosition = 0;
    m_fLastTemp = -273.15f; // aboslute zero :)

    // Read in settings
    if (m_pIniUtil) {
        m_PegasusController.setPosLimit(m_pIniUtil->readInt(PARENT_KEY, POS_LIMIT, 0));
        m_PegasusController.enablePosLimit(m_pIniUtil->readInt(PARENT_KEY, POS_LIMIT_ENABLED, false));
    }
	m_PegasusController.SetSerxPointer(m_pSerX);
	m_PegasusController.setLogger(m_pLogger);
}

X2Focuser::~X2Focuser()
{

	//Delete objects used through composition
	if (GetSerX())
		delete GetSerX();
	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetSimpleIniUtil())
		delete GetSimpleIniUtil();
	if (GetLogger())
		delete GetLogger();
	if (GetMutex())
		delete GetMutex();
}

#pragma mark - DriverRootInterface

int	X2Focuser::queryAbstraction(const char* pszName, void** ppVal)
{
    *ppVal = NULL;

    if (!strcmp(pszName, LinkInterface_Name))
        *ppVal = (LinkInterface*)this;

    else if (!strcmp(pszName, FocuserGotoInterface2_Name))
        *ppVal = (FocuserGotoInterface2*)this;

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);

    else if (!strcmp(pszName, FocuserTemperatureInterface_Name))
        *ppVal = dynamic_cast<FocuserTemperatureInterface*>(this);

    else if (!strcmp(pszName, LoggerInterface_Name))
        *ppVal = GetLogger();

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);

    return SB_OK;
}

#pragma mark - DriverInfoInterface
void X2Focuser::driverInfoDetailedInfo(BasicStringInterface& str) const
{
        str = "Focuser X2 plugin by Rodolphe Pineau";
}

double X2Focuser::driverInfoVersion(void) const							
{
	return DRIVER_VERSION;
}

void X2Focuser::deviceInfoNameShort(BasicStringInterface& str) const
{
    int deviceType;

    if(!m_bLinked) {
        str="NA";
    }
    else {
        X2Focuser* pMe = (X2Focuser*)this;

        X2MutexLocker ml(pMe->GetMutex());

        pMe->m_PegasusController.getDeviceType(deviceType);

        if(deviceType == DMFC)
            str = "DMFC";
        else if(deviceType == SMFC)
            str = "SMFC";
    }
}

void X2Focuser::deviceInfoNameLong(BasicStringInterface& str) const				
{
    deviceInfoNameShort(str);
}

void X2Focuser::deviceInfoDetailedDescription(BasicStringInterface& str) const		
{
	str = "Pegasus Focus Controller";
}

void X2Focuser::deviceInfoFirmwareVersion(BasicStringInterface& str)				
{
    if(!m_bLinked) {
        str="NA";
    }
    else {
    // get firmware version
        char cFirmware[SERIAL_BUFFER_SIZE];
        m_PegasusController.getFirmwareVersion(cFirmware, SERIAL_BUFFER_SIZE);
        str = cFirmware;
    }
}

void X2Focuser::deviceInfoModel(BasicStringInterface& str)							
{
    if(!m_bLinked) {
        str="NA";
    }
    else {
        X2MutexLocker ml(GetMutex());

        // get model version
        int deviceType;

        m_PegasusController.getDeviceType(deviceType);
        if(deviceType == DMFC)
            str = "DMFC";
        else if(deviceType == SMFC)
            str = "SMFC";
    }
}

#pragma mark - LinkInterface
int	X2Focuser::establishLink(void)
{
    char szPort[DRIVER_MAX_STRING];
    int err;

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    err = m_PegasusController.Connect(szPort);
    if(err)
        m_bLinked = false;
    else
        m_bLinked = true;

    return err;
}

int	X2Focuser::terminateLink(void)
{
    if(!m_bLinked)
        return SB_OK;

    X2MutexLocker ml(GetMutex());
    m_PegasusController.Disconnect();
    m_bLinked = false;
    return SB_OK;
}

bool X2Focuser::isLinked(void) const
{
	return m_bLinked;
}

#pragma mark - ModalSettingsDialogInterface
int	X2Focuser::initModalSettingsDialog(void)
{
    return SB_OK;
}

int	X2Focuser::execModalSettingsDialog(void)
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*					ui = uiutil.X2UI();
    X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;
    int nMaxSpeed = 0;
    int nPosition = 0;
    bool bLimitEnabled = false;
    int nPosLimit = 0;
    int nBacklashSteps = 0;
    bool bBacklashEnabled = false;
    bool bRotaryEnabled = false;
    int nPegasusDeviceType = NONE;
    int nMotorType = NONE;
    bool bStepperEnable = false;
    bool bReverse = false;

    mUiEnabled = false;

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("PegasusController.ui", deviceType(), m_nPrivateMulitInstanceIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    X2MutexLocker ml(GetMutex());

	// set controls values
    if(m_bLinked) {
        // get data from device
        m_PegasusController.getConsolidatedStatus();
        // enable all controls

        // motor max spped
        nErr = m_PegasusController.getMotoMaxSpeed(nMaxSpeed);
        if(nErr)
            return nErr;
        dx->setEnabled("maxSpeed", true);
        dx->setEnabled("pushButton", true);
        dx->setPropertyInt("maxSpeed", "value", nMaxSpeed);

        // new position (set to current )
        nErr = m_PegasusController.getPosition(nPosition);
        if(nErr)
            return nErr;
        dx->setEnabled("newPos", true);
        dx->setEnabled("pushButton_2", true);
        dx->setPropertyInt("newPos", "value", nPosition);

        // reverse  reverseDir
        nErr = m_PegasusController.getReverseEnable(bReverse);
        if(bReverse)
            dx->setChecked("reverseDir", true);
        else
            dx->setChecked("reverseDir", false);

        // backlash
        dx->setEnabled("backlashSteps", true);
        nErr = m_PegasusController.getBacklashComp(nBacklashSteps);
        if(nErr)
            return nErr;
        dx->setPropertyInt("backlashSteps", "value", nBacklashSteps);

        if(!nBacklashSteps)  // backlash = 0 means disabled.
            bBacklashEnabled = false;
        else
            bBacklashEnabled = true;
        if(bBacklashEnabled)
            dx->setChecked("backlashEnable", true);
        else
            dx->setChecked("backlashEnable", false);
        // rotary
        dx->setEnabled("checkBox", true);
        nErr = m_PegasusController.getEnableRotaryEncoder(bRotaryEnabled);
        if(nErr)
            return nErr;
        if(bRotaryEnabled)
            dx->setChecked("checkBox", true);
        else
            dx->setChecked("checkBox", false);

        if(nErr)
            return nErr;
        // device type
        nErr = m_PegasusController.getDeviceType(nPegasusDeviceType);
        switch(nPegasusDeviceType){
            case SMFC : // SMFC, disable this part
                dx->setEnabled("radioButton", false);
                dx->setEnabled("radioButton_2", false);
                break;

            case DMFC : // DMFC
                dx->setEnabled("radioButton", true);
                dx->setEnabled("radioButton_2", true);
                // motor type  for DMFC
                nErr = m_PegasusController.getMotorType(nMotorType);
                if(nMotorType == STEPPER)
                    dx->setChecked("radioButton", true);
                else
                    dx->setChecked("radioButton_2", true);
                break;

            default: // don't knpw, disable this part
                dx->setEnabled("radioButton", false);
                dx->setEnabled("radioButton_2", false);
                break;
        }
    }
    else {
        // disable all controls
        dx->setEnabled("maxSpeed", false);
        dx->setPropertyInt("maxSpeed", "value", 0);
        dx->setEnabled("pushButton", false);
        dx->setEnabled("newPos", false);
        dx->setPropertyInt("newPos", "value", 0);
        dx->setEnabled("reverseDir", false);
        dx->setEnabled("pushButton_2", false);
        dx->setEnabled("backlashSteps", false);
        dx->setPropertyInt("backlashSteps", "value", 0);
        dx->setEnabled("backlashEnable", false);
        dx->setEnabled("checkBox", false);
        dx->setEnabled("radioButton", false);
        dx->setEnabled("radioButton_2", false);
    }

    // linit is done in software so it's always enabled.
    dx->setEnabled("posLimit", true);
    dx->setEnabled("limitEnable", true);
    dx->setPropertyInt("posLimit", "value", m_PegasusController.getPosLimit());
    if(m_PegasusController.isPosLimitEnabled())
        dx->setChecked("limitEnable", true);
    else
        dx->setChecked("limitEnable", false);

    //Display the user interface
    mUiEnabled = true;
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;
    mUiEnabled = false;

    //Retreive values from the user interface
    if (bPressedOK) {
        nErr = SB_OK;
        // get limit option
        bLimitEnabled = dx->isChecked("limitEnable");
        dx->propertyInt("posLimit", "value", nPosLimit);
        if(bLimitEnabled && nPosLimit>0) { // a position limit of 0 doesn't make sense :)
            m_PegasusController.setPosLimit(nPosLimit);
            m_PegasusController.enablePosLimit(bLimitEnabled);
        } else {
            m_PegasusController.enablePosLimit(false);
        }
		if(m_bLinked) {
			// get reverse
			bReverse = dx->isChecked("reverseDir");
			nErr = m_PegasusController.setReverseEnable(bReverse);
			if(nErr)
				return nErr;

			// get backlash if enable, disbale if needed
			bBacklashEnabled = dx->isChecked("backlashEnable");
			if(bBacklashEnabled) {
				dx->propertyInt("backlashSteps", "value", nBacklashSteps);
				nErr = m_PegasusController.setBacklashComp(nBacklashSteps);
				if(nErr)
					return nErr;
			}
			else {
				nErr = m_PegasusController.setBacklashComp(0); // disable backlash comp
				if(nErr)
					return nErr;
			}
			// enable the knob if needed
			bRotaryEnabled = dx->isChecked("knobEnable");
			nErr = m_PegasusController.setEnableRotaryEncoder(bRotaryEnabled);
			if(nErr)
				return nErr;

			// set motor type
			bStepperEnable = dx->isChecked("radioButton"); // STEPPER
			if(bStepperEnable) {
				nErr = m_PegasusController.setMotorType(STEPPER);
			}
			else {
				nErr = m_PegasusController.setMotorType(DC);
			}
			if(nErr)
				return nErr;
		}
        // save values to config
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, POS_LIMIT, nPosLimit);
        nErr |= m_pIniUtil->writeInt(PARENT_KEY, POS_LIMIT_ENABLED, bLimitEnabled);
    }
    return nErr;
}

void X2Focuser::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    int nErr = SB_OK;
    int nTmpVal;
    char szErrorMessage[LOG_BUFFER_SIZE];
	
    // max speed
    if (!strcmp(pszEvent, "on_pushButton_clicked")) {
        uiex->propertyInt("maxSpeed", "value", nTmpVal);
        nErr = m_PegasusController.setMotoMaxSpeed(nTmpVal);
        if(nErr) {
            snprintf(szErrorMessage, LOG_BUFFER_SIZE, "Error setting max speed : Error %d", nErr);
            uiex->messageBox("Set Max Speed", szErrorMessage);
            return;
        }
    }
    // new position
    else if (!strcmp(pszEvent, "on_pushButton_2_clicked")) {
        uiex->propertyInt("newPos", "value", nTmpVal);
        nErr = m_PegasusController.syncMotorPosition(nTmpVal);
        if(nErr) {
            snprintf(szErrorMessage, LOG_BUFFER_SIZE, "Error setting new position : Error %d", nErr);
            uiex->messageBox("Set New Position", szErrorMessage);
            return;
        }
    }


}

#pragma mark - FocuserGotoInterface2
int	X2Focuser::focPosition(int& nPosition)
{
    int err;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());

    err = m_PegasusController.getPosition(nPosition);
    m_nPosition = nPosition;
    return err;
}

int	X2Focuser::focMinimumLimit(int& nMinLimit) 		
{
	nMinLimit = 0;
    return SB_OK;
}

int	X2Focuser::focMaximumLimit(int& nPosLimit)			
{
    if(m_PegasusController.isPosLimitEnabled()) {
        nPosLimit = m_PegasusController.getPosLimit();
    }
    else
        nPosLimit = 99999999;

    return SB_OK;
}

int	X2Focuser::focAbort()								
{   int err;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    err = m_PegasusController.haltFocuser();
    return err;
}

int	X2Focuser::startFocGoto(const int& nRelativeOffset)	
{
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    m_PegasusController.moveRelativeToPosision(nRelativeOffset);
    return SB_OK;
}

int	X2Focuser::isCompleteFocGoto(bool& bComplete) const
{
    int err;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2Focuser* pMe = (X2Focuser*)this;
    X2MutexLocker ml(pMe->GetMutex());

    err = pMe->m_PegasusController.isGoToComplete(bComplete);

    return err;
}

int	X2Focuser::endFocGoto(void)
{
    int err;
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    err = m_PegasusController.getPosition(m_nPosition);
    return err;
}

int X2Focuser::amountCountFocGoto(void) const					
{ 
	return 3;
}

int	X2Focuser::amountNameFromIndexFocGoto(const int& nZeroBasedIndex, BasicStringInterface& strDisplayName, int& nAmount)
{
	switch (nZeroBasedIndex)
	{
		default:
		case 0: strDisplayName="10 steps"; nAmount=10;break;
		case 1: strDisplayName="100 steps"; nAmount=100;break;
		case 2: strDisplayName="1000 steps"; nAmount=1000;break;
	}
	return SB_OK;
}

int	X2Focuser::amountIndexFocGoto(void)
{
	return 0;
}

#pragma mark - FocuserTemperatureInterface
int X2Focuser::focTemperature(double &dTemperature)
{
    int err;

    if(!m_bLinked) {
        dTemperature = -100.0;
        return NOT_CONNECTED;
    }

    // Taken from Richard's Robofocus plugin code.
    // this prevent us from asking the temperature too often
    static CStopWatch timer;

    if(timer.GetElapsedSeconds() > 30.0f || m_fLastTemp < -99.0f) {
        X2MutexLocker ml(GetMutex());
        err = m_PegasusController.getTemperature(m_fLastTemp);
        timer.Reset();
    }

    dTemperature = m_fLastTemp;

    return err;
}

#pragma mark - SerialPortParams2Interface

void X2Focuser::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;

}

void X2Focuser::setPortName(const char* pszPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort);

}


void X2Focuser::portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    snprintf(pszPort, nMaxSize, DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);
    
}




