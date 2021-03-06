/**
 *  Copyright (c) 2012 BMW
 *
 *  \author Aleksandar Donchev, aleksander.donchev@partner.bmw.de BMW 2013
 *
 *  \copyright
 *  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction,
 *  including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 *  subject to the following conditions:
 *  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 *  THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *  For further information see http://www.genivi.org/.
 */

#include "CAmCommandSenderCAPI.h"
#include <algorithm>
#include <string>
#include <vector>
#include <cassert>
#include <set>
#include <thread>
#include "CAmDltWrapper.h"
#include "CAmCommandSenderCommon.h"


DLT_DECLARE_CONTEXT(ctxCommandCAPI)

/**
 * factory for plugin loading
 */
extern "C" IAmCommandSend* PluginCommandInterfaceCAPIFactory()
{
    CAmDltWrapper::instance()->registerContext(ctxCommandCAPI, "CAPIP", "Common-API Plugin");
    return (new CAmCommandSenderCAPI());
}

/**
 * destroy instance of commandSendInterface
 */
extern "C" void destroyPluginCommandInterfaceCAPIFactory(IAmCommandSend* commandSendInterface)
{
    delete commandSendInterface;
}


const char * CAmCommandSenderCAPI::COMMAND_SENDER_INSTANCE = DBUS_SERVICE_PREFIX;
const char * CAmCommandSenderCAPI::DEFAULT_DOMAIN = "local";

#define RETURN_IF_NOT_READY() if(!mReady) return;

CAmCommandSenderCAPI::CAmCommandSenderCAPI() :
        mService(), //
        mpCAmCAPIWrapper(NULL), //
        mpIAmCommandReceive(NULL), //
        mReady(false),
        mIsServiceStarted(false)
{
    log(&ctxCommandCAPI, DLT_LOG_INFO, "CommandSenderCAPI constructor called");
}

CAmCommandSenderCAPI::~CAmCommandSenderCAPI()
{
    log(&ctxCommandCAPI, DLT_LOG_INFO, "CAPICommandSender freed");
    CAmDltWrapper::instance()->unregisterContext(ctxCommandCAPI);
    tearDownInterface(mpIAmCommandReceive);
}

/**
 * registers a service
 */
am_Error_e CAmCommandSenderCAPI::startService(IAmCommandReceive* commandreceiveinterface)
{
	if(!mpCAmCAPIWrapper)
		mpCAmCAPIWrapper=CAPI;
	assert(mpCAmCAPIWrapper!=NULL);
	if(!mIsServiceStarted)
	{
		assert(commandreceiveinterface);
		mService = std::make_shared<CAmCommandSenderService>(commandreceiveinterface);
		//Registers the service
		if( false == mpCAmCAPIWrapper->registerService(mService, CAmCommandSenderCAPI::DEFAULT_DOMAIN, CAmCommandSenderCAPI::COMMAND_SENDER_INSTANCE) )
		{
		    log(&ctxCommandCAPI, DLT_LOG_ERROR, "Can't register stub ", CAmCommandSenderCAPI::DEFAULT_DOMAIN, v1_1::org::genivi::am::commandinterface::CommandControl::getInterface(), CAmCommandSenderCAPI::COMMAND_SENDER_INSTANCE);
			return (E_NOT_POSSIBLE);
		}
		log(&ctxCommandCAPI, DLT_LOG_INFO, "Stub has been successful registered!", CAmCommandSenderCAPI::DEFAULT_DOMAIN, v1_1::org::genivi::am::commandinterface::CommandControl::getInterface(), CAmCommandSenderCAPI::COMMAND_SENDER_INSTANCE);
		mIsServiceStarted = true;
	}
    return (E_OK);
}

/**
 * sets a command receiver object and registers a service
 */
am_Error_e CAmCommandSenderCAPI::startupInterface(IAmCommandReceive* commandreceiveinterface)
{
    log(&ctxCommandCAPI, DLT_LOG_INFO, "startupInterface called");
    mpIAmCommandReceive = commandreceiveinterface;
    return startService(commandreceiveinterface);
}

/**
 * stops the service
 */
am_Error_e CAmCommandSenderCAPI::tearDownInterface(IAmCommandReceive*)
{
    if(mpCAmCAPIWrapper)
    {
    	if(mIsServiceStarted)
    	{
    		mIsServiceStarted = false;
			mpCAmCAPIWrapper->unregisterService(CAmCommandSenderCAPI::DEFAULT_DOMAIN, v1_1::org::genivi::am::commandinterface::CommandControl::getInterface(), CAmCommandSenderCAPI::COMMAND_SENDER_INSTANCE);
			mService.reset();
    	}
   		return (E_OK);
    }
    return (E_NOT_POSSIBLE);
}

void CAmCommandSenderCAPI::setCommandReady(const uint16_t handle)
{
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbCommunicationReady called");
    mReady = true;
    mpIAmCommandReceive->confirmCommandReady(handle,E_OK);
}

void CAmCommandSenderCAPI::setCommandRundown(const uint16_t handle)
{
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbCommunicationRundown called");
    mReady = false;
    mpIAmCommandReceive->confirmCommandRundown(handle,E_OK);
}

void CAmCommandSenderCAPI::cbNewMainConnection(const am_MainConnectionType_s& mainConnectionType)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbNumberOfMainConnectionsChanged called");
    am_types::am_MainConnectionType_s mainConnection(mainConnectionType.mainConnectionID,mainConnectionType.sourceID,mainConnectionType.sinkID,mainConnectionType.delay,CAmConvert2CAPIType(mainConnectionType.connectionState));
    mService->fireNewMainConnectionEvent(mainConnection);
}

void CAmCommandSenderCAPI::cbRemovedMainConnection(const am_mainConnectionID_t mainConnection)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbNumberOfMainConnectionsChanged called");
    mService->fireRemovedMainConnectionEvent(mainConnection);
}

void CAmCommandSenderCAPI::cbNewSink(const am_SinkType_s& sink)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbNewSink called");
    am_types::am_Availability_s convAvailability;
    CAmConvertAvailablility(sink.availability, convAvailability);
    am_types::am_SinkType_s ciSink(sink.sinkID, sink.name, convAvailability, sink.volume, CAmConvert2CAPIType(sink.muteState), sink.sinkClassID);
    mService->fireNewSinkEvent(ciSink);
}

void CAmCommandSenderCAPI::cbRemovedSink(const am_sinkID_t sinkID)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbRemovedSink called");
    mService->fireRemovedSinkEvent(sinkID);
}

void CAmCommandSenderCAPI::cbNewSource(const am_SourceType_s& source)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbNewSource called");
    am_types::am_Availability_s convAvailability;
    CAmConvertAvailablility(source.availability, convAvailability);
    am_types::am_SourceType_s ciSource(source.sourceID, source.name, convAvailability, source.sourceClassID);
    mService->fireNewSourceEvent(ciSource);
}

void CAmCommandSenderCAPI::cbRemovedSource(const am_sourceID_t source)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbRemovedSource called");
    mService->fireRemovedSourceEvent(source);
}

void CAmCommandSenderCAPI::cbNumberOfSinkClassesChanged()
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbNumberOfSinkClassesChanged called");
    mService->fireNumberOfSinkClassesChangedEvent();
}

void CAmCommandSenderCAPI::cbNumberOfSourceClassesChanged()
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbNumberOfSourceClassesChanged called");
    mService->fireNumberOfSourceClassesChangedEvent();
}

void CAmCommandSenderCAPI::cbMainConnectionStateChanged(const am_mainConnectionID_t connectionID, const am_ConnectionState_e connectionState)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbMainConnectionStateChanged called, connectionID=", connectionID, "connectionState=", connectionState);
    mService->fireMainConnectionStateChangedEvent(connectionID, CAmConvert2CAPIType(connectionState));
}

void CAmCommandSenderCAPI::cbMainSinkSoundPropertyChanged(const am_sinkID_t sinkID, const am_MainSoundProperty_s & soundProperty)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbMainSinkSoundPropertyChanged called, sinkID", sinkID, "SoundProperty.type", soundProperty.type, "SoundProperty.value", soundProperty.value);
    am_types::am_MainSoundProperty_s mainSoundProp(soundProperty.type, soundProperty.value);
	mService->fireMainSinkSoundPropertyChangedEvent(sinkID, mainSoundProp);
}

void CAmCommandSenderCAPI::cbMainSourceSoundPropertyChanged(const am_sourceID_t sourceID, const am_MainSoundProperty_s & SoundProperty)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbMainSourceSoundPropertyChanged called, sourceID", sourceID, "SoundProperty.type", SoundProperty.type, "SoundProperty.value", SoundProperty.value);
    am_types::am_MainSoundProperty_s convValue;
    CAmConvertMainSoundProperty(SoundProperty, convValue);
    mService->fireMainSourceSoundPropertyChangedEvent(sourceID, convValue);
}

void CAmCommandSenderCAPI::cbSinkAvailabilityChanged(const am_sinkID_t sinkID, const am_Availability_s & availability)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbSinkAvailabilityChanged called, sinkID", sinkID, "availability.availability", availability.availability, "SoundProperty.reason", availability.availabilityReason);
    am_types::am_Availability_s convAvailability;
    CAmConvertAvailablility(availability, convAvailability);
    mService->fireSinkAvailabilityChangedEvent(sinkID, convAvailability);
}

void CAmCommandSenderCAPI::cbSourceAvailabilityChanged(const am_sourceID_t sourceID, const am_Availability_s & availability)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbSourceAvailabilityChanged called, sourceID", sourceID, "availability.availability", availability.availability, "SoundProperty.reason", availability.availabilityReason);
    am_types::am_Availability_s convAvailability;
    CAmConvertAvailablility(availability, convAvailability);
    mService->fireSourceAvailabilityChangedEvent(sourceID, convAvailability);
}

void CAmCommandSenderCAPI::cbVolumeChanged(const am_sinkID_t sinkID, const am_mainVolume_t volume)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbVolumeChanged called, sinkID", sinkID, "volume", volume);
    mService->fireVolumeChangedEvent(sinkID, volume);
}

void CAmCommandSenderCAPI::cbSinkMuteStateChanged(const am_sinkID_t sinkID, const am_MuteState_e muteState)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbSinkMuteStateChanged called, sinkID", sinkID, "muteState", muteState);
    am_types::am_MuteState_e ciMuteState = CAmConvert2CAPIType(muteState);
    mService->fireSinkMuteStateChangedEvent(sinkID, ciMuteState);
}

void CAmCommandSenderCAPI::cbSystemPropertyChanged(const am_SystemProperty_s & SystemProperty)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbSystemPropertyChanged called, SystemProperty.type", SystemProperty.type, "SystemProperty.value", SystemProperty.value);
    am_types::am_SystemProperty_s convValue;
    CAmConvertSystemProperty(SystemProperty, convValue);
    mService->fireSystemPropertyChangedEvent(convValue);
}

void CAmCommandSenderCAPI::cbTimingInformationChanged(const am_mainConnectionID_t mainConnectionID, const am_timeSync_t time)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbTimingInformationChanged called, mainConnectionID=", mainConnectionID, "time=", time);
    am_types::am_mainConnectionID_t ciMainConnection = mainConnectionID;
    mService->fireTimingInformationChangedEvent(ciMainConnection, time);
}

void CAmCommandSenderCAPI::getInterfaceVersion(std::string & version) const
{
    version = CommandVersion;
}

void CAmCommandSenderCAPI::cbSinkUpdated(const am_sinkID_t sinkID, const am_sinkClass_t sinkClassID, const std::vector<am_MainSoundProperty_s>& listMainSoundProperties)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbSinkUpdated called, sinkID", sinkID);
    am_types::am_MainSoundProperty_L list;
    std::for_each(listMainSoundProperties.begin(), listMainSoundProperties.end(), [&](const am_MainSoundProperty_s & ref) {
				am_types::am_MainSoundProperty_s prop(ref.type, ref.value);
				list.push_back(prop);
    });
    mService->fireSinkUpdatedEvent(sinkID, sinkClassID, list);
}

void CAmCommandSenderCAPI::cbSourceUpdated(const am_sourceID_t sourceID, const am_sourceClass_t sourceClassID, const std::vector<am_MainSoundProperty_s>& listMainSoundProperties)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbSourceUpdated called, sourceID", sourceID);
    am_types::am_MainSoundProperty_L list;
    std::for_each(listMainSoundProperties.begin(), listMainSoundProperties.end(), [&](const am_MainSoundProperty_s & ref) {
    			am_types::am_MainSoundProperty_s prop(ref.type, ref.value);
				list.push_back(prop);
    });
    mService->fireSourceUpdatedEvent(sourceID, sourceClassID, list);
}

void CAmCommandSenderCAPI::cbSinkNotification(const am_sinkID_t sinkID, const am_NotificationPayload_s& notification)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbSinkNotification called, sinkID", sinkID);
    am_types::am_NotificationPayload_s ciNnotif(notification.type, notification.value);
    mService->fireSinkNotificationEvent(sinkID, ciNnotif);
}

void CAmCommandSenderCAPI::cbSourceNotification(const am_sourceID_t sourceID, const am_NotificationPayload_s& notification)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbSourceNotification called, sourceID", sourceID);
    am_types::am_NotificationPayload_s ciNnotif(notification.type, notification.value);
    mService->fireSourceNotificationEvent(sourceID, ciNnotif);
}

void CAmCommandSenderCAPI::cbMainSinkNotificationConfigurationChanged(const am_sinkID_t sinkID, const am_NotificationConfiguration_s& mainNotificationConfiguration)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbSinkMainNotificationConfigurationChanged called, sinkID", sinkID);
    am_types::am_NotificationConfiguration_s ciNotifConfig(mainNotificationConfiguration.type,
    															  static_cast<am_types::am_NotificationStatus_e::Literal>(mainNotificationConfiguration.status),
																				mainNotificationConfiguration.parameter);
    mService->fireMainSinkNotificationConfigurationChangedEvent(sinkID, ciNotifConfig);
}

void CAmCommandSenderCAPI::cbMainSourceNotificationConfigurationChanged(const am_sourceID_t sourceID, const am_NotificationConfiguration_s& mainNotificationConfiguration)
{
	RETURN_IF_NOT_READY()
	assert((bool)mService);
    log(&ctxCommandCAPI, DLT_LOG_INFO, "cbSourceMainNotificationConfigurationChanged called, sourceID", sourceID);
    am_types::am_NotificationConfiguration_s ciNotifConfig(mainNotificationConfiguration.type,
    															  static_cast<am_types::am_NotificationStatus_e::Literal>(mainNotificationConfiguration.status),
																				mainNotificationConfiguration.parameter);
    mService->fireMainSourceNotificationConfigurationChangedEvent(sourceID, ciNotifConfig);
}
