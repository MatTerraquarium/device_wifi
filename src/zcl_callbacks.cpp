#include "app_task.h"

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>

using namespace ::chip;
using namespace ::chip::app::Clusters;

/* MATTER COMMANDS LISTENER */
void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type,
                                       uint16_t size, uint8_t * value)
{
        AppEvent event;

        /* DK LED */
        if (attributePath.mEndpointId == 1) {
            return;
        }

        /* HOT-LAMP */
        /* Verify if the command receiver is for the endpoint 2 */
        if (attributePath.mEndpointId == 2) {
                /* Verify if the command receiver is managed [On/Off] */
                if (attributePath.mClusterId != OnOff::Id || attributePath.mAttributeId != OnOff::Attributes::OnOff::Id)
                        return;
                /* In case of On command turn on the hot lamp */
                if (*value) {
                        event.Type = AppEventType::HotLampActivate;
                        event.Handler = AppTask::HotLampActivateHandler;
                } 
                /* In case of Off command turn off the hot lamp */
                else {
                        event.Type = AppEventType::HotLampDeactivate;
                        event.Handler = AppTask::HotLampDeactivateHandler;
                }
                AppTask::Instance().PostEvent(event);
        }

        /* UVB LAMP */
        /* Verify if the command receiver is for the endpoint 3 */
        if (attributePath.mEndpointId == 3) {
                /* Verify if the command receiver is managed [On/Off] */
                if (attributePath.mClusterId != OnOff::Id || attributePath.mAttributeId != OnOff::Attributes::OnOff::Id)
                        return;
                // In case of On command turn on the uvb lamp
                if (*value) {
                        event.Type = AppEventType::UvbLampActivate;
                        event.Handler = AppTask::UvbLampActivateHandler;
                } 
                /* In case of On command turn on the uvb lamp */
                else {
                        event.Type = AppEventType::UvbLampDeactivate;
                        event.Handler = AppTask::UvbLampDeactivateHandler;
                }
                AppTask::Instance().PostEvent(event);
        }

        /* HEATER */
        /* Verify if the command receiver is for the endpoint 4 */
        if (attributePath.mEndpointId == 4) {
                /* Verify if the command receiver is managed [On/Off] */
                if (attributePath.mClusterId != OnOff::Id || attributePath.mAttributeId != OnOff::Attributes::OnOff::Id)
                        return;
                /* In case of On command turn on the water heater */
                if (*value) {
                        event.Type = AppEventType::HeaterActivate;
                        event.Handler = AppTask::HeaterActivateHandler;
                } 
                /* In case of Off command turn off the water heater */
                else {
                        event.Type = AppEventType::HeaterDeactivate;
                        event.Handler = AppTask::HeaterDeactivateHandler;
                }
                AppTask::Instance().PostEvent(event);
        }

        /* FILTER */
        /* Verify if the command receiver is for the endpoint 5 */
        if (attributePath.mEndpointId == 5) {
                /* Verify if the command receiver is managed [On/Off] */
                if (attributePath.mClusterId != OnOff::Id || attributePath.mAttributeId != OnOff::Attributes::OnOff::Id)
                        return;
                /* In case of On command turn on the water filter pump */
                if (*value) {
                        event.Type = AppEventType::FilterActivate;
                        event.Handler = AppTask::FilterActivateHandler;
                } 
                /* In case of Off command turn off the water filter pump */
                else {
                        event.Type = AppEventType::FilterDeactivate;
                        event.Handler = AppTask::FilterDeactivateHandler;
                }
                AppTask::Instance().PostEvent(event);
        }

        /* FEEDER */
        /* Verify if the command receiver is for the endpoint 5 */
        if (attributePath.mEndpointId == 6) {
                /* Verify if the command receiver is managed [On/Off] */
                if (attributePath.mClusterId != OnOff::Id || attributePath.mAttributeId != OnOff::Attributes::OnOff::Id)
                        return;
                /* In case of On command turn on the automatic feeder */
                if (*value) {
                        event.Type = AppEventType::FeederActivate;
                        event.Handler = AppTask::FeederActivateHandler;
                } 
                /* In case of Off command turn off the automatic feeder */
                else {
                        event.Type = AppEventType::FeederDeactivate;
                        event.Handler = AppTask::FeederDeactivateHandler;
                }
                AppTask::Instance().PostEvent(event);
        }
}
