#include "app_task.h"

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>

using namespace ::chip;
using namespace ::chip::app::Clusters;

void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type,
                                       uint16_t size, uint8_t * value)
{
        AppEvent event;
        // LED
        if (attributePath.mEndpointId == 1) {
            return;
        }
        // HOT-LAMP
        if (attributePath.mEndpointId == 2) {
                if (attributePath.mClusterId != OnOff::Id || attributePath.mAttributeId != OnOff::Attributes::OnOff::Id)
                        return;
                if (*value) {
                        event.Type = AppEventType::HotLampActivate;
                        event.Handler = AppTask::HotLampActivateHandler;
                } else {
                        event.Type = AppEventType::HotLampDeactivate;
                        event.Handler = AppTask::HotLampDeactivateHandler;
                }
                AppTask::Instance().PostEvent(event);
        }
        // UVB LAMP
        if (attributePath.mEndpointId == 3) {
                if (attributePath.mClusterId != OnOff::Id || attributePath.mAttributeId != OnOff::Attributes::OnOff::Id)
                        return;
                if (*value) {
                        event.Type = AppEventType::UvbLampActivate;
                        event.Handler = AppTask::UvbLampActivateHandler;
                } else {
                        event.Type = AppEventType::UvbLampDeactivate;
                        event.Handler = AppTask::UvbLampDeactivateHandler;
                }
                AppTask::Instance().PostEvent(event);
        }
        // HEATER
        if (attributePath.mEndpointId == 4) {
                if (attributePath.mClusterId != OnOff::Id || attributePath.mAttributeId != OnOff::Attributes::OnOff::Id)
                        return;
                if (*value) {
                        event.Type = AppEventType::HeaterActivate;
                        event.Handler = AppTask::HeaterActivateHandler;
                } else {
                        event.Type = AppEventType::HeaterDeactivate;
                        event.Handler = AppTask::HeaterDeactivateHandler;
                }
                AppTask::Instance().PostEvent(event);
        }
        // FILTER
        if (attributePath.mEndpointId == 5) {
                if (attributePath.mClusterId != OnOff::Id || attributePath.mAttributeId != OnOff::Attributes::OnOff::Id)
                        return;
                if (*value) {
                        event.Type = AppEventType::FilterActivate;
                        event.Handler = AppTask::FilterActivateHandler;
                } else {
                        event.Type = AppEventType::FilterDeactivate;
                        event.Handler = AppTask::FilterDeactivateHandler;
                }
                AppTask::Instance().PostEvent(event);
        }
        // FEEDER
        if (attributePath.mEndpointId == 6) {
                if (attributePath.mClusterId != OnOff::Id || attributePath.mAttributeId != OnOff::Attributes::OnOff::Id)
                        return;
                if (*value) {
                        event.Type = AppEventType::FeederActivate;
                        event.Handler = AppTask::FeederActivateHandler;
                } else {
                        event.Type = AppEventType::FeederDeactivate;
                        event.Handler = AppTask::FeederDeactivateHandler;
                }
                AppTask::Instance().PostEvent(event);
        }
}
