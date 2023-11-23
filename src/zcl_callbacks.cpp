/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <lib/support/logging/CHIPLogging.h>

#include "app_task.h"

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>

using namespace ::chip;
using namespace ::chip::app::Clusters;
using namespace ::chip::app::Clusters::OnOff;

/* MATTER COMMANDS LISTENER */
void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type,
                                       uint16_t size, uint8_t * value)
{
        AppEvent event;

        /* DK LED */
        /* Verify if the command receiver is for the endpoint 2 */
        if (attributePath.mEndpointId == 1) {
                /* Verify if the command receiver is managed [On/Off] */
                if (attributePath.mClusterId != OnOff::Id || attributePath.mAttributeId != OnOff::Attributes::OnOff::Id)
                        return;
                /* In case of On command turn on the DK led2 */
                if (*value) {
                        event.Type = AppEventType::LightingLedActivate;
                        event.Handler = AppTask::LightingLedActivateHandler;
                } 
                /* In case of Off command turn off the DK led2 */
                else {
                        event.Type = AppEventType::LightingLedDeactivate;
                        event.Handler = AppTask::LightingLedDeactivateHandler;
                }
        }
        /* HOT-LAMP */
        /* Verify if the command receiver is for the endpoint 2 */
        else if (attributePath.mEndpointId == 2) {
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
        }
        /* UVB LAMP */
        /* Verify if the command receiver is for the endpoint 3 */
        else if (attributePath.mEndpointId == 3) {
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
        }

        /* HEATER */
        /* Verify if the command receiver is for the endpoint 4 */
        else if (attributePath.mEndpointId == 4) {
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
        }
        /* FILTER */
        /* Verify if the command receiver is for the endpoint 5 */
        else if (attributePath.mEndpointId == 5) {
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
        }
        /* FEEDER */
        /* Verify if the command receiver is for the endpoint 5 */
        else if (attributePath.mEndpointId == 6) {
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
        }
        
        AppTask::Instance().PostEvent(event);
}

/** @brief OnOff Cluster Init
 *
 * This function is called when a specific cluster is initialized. It gives the
 * application an opportunity to take care of cluster initialization procedures.
 * It is called exactly once for each endpoint where cluster is present.
 *
 * @param endpoint   Ver.: always
 *
 * TODO Issue #3841
 * emberAfOnOffClusterInitCallback happens before the stack initialize the cluster
 * attributes to the default value.
 * The logic here expects something similar to the deprecated Plugins callback
 * emberAfPluginOnOffClusterServerPostInitCallback.
 *
 */
void emberAfOnOffClusterInitCallback(EndpointId endpoint)
{
        AppEvent event;
	EmberAfStatus status;
	bool storedValue;

        /* DK LED */
        if (endpoint == 1) {
                /* Read storedValue on/off value */
                status = Attributes::OnOff::Get(endpoint, &storedValue);
                if (status == EMBER_ZCL_STATUS_SUCCESS) {
                        /* Set actual state to the cluster state that was last persisted */
                        if (storedValue) {
                                event.Type = AppEventType::LightingLedActivate;
                                event.Handler = AppTask::LightingLedActivateHandler;
                        } else {
                                event.Type = AppEventType::LightingLedDeactivate;
                                event.Handler = AppTask::LightingLedDeactivateHandler;
                        }
                }
        }
        /* HOT LAMP */
        else if (endpoint == 2) {
                /* Read storedValue on/off value */
                status = Attributes::OnOff::Get(endpoint, &storedValue);
                if (status == EMBER_ZCL_STATUS_SUCCESS) {
                        /* Set actual state to the cluster state that was last persisted */
                        if (storedValue) {
                                event.Type = AppEventType::HotLampActivate;
                                event.Handler = AppTask::HotLampActivateHandler;
                        } else {
                                event.Type = AppEventType::HotLampDeactivate;
                                event.Handler = AppTask::HotLampDeactivateHandler;
                        }
                }
        }
        /* UVB LAMP */
        else if (endpoint == 3) {
                /* Read storedValue on/off value */
                status = Attributes::OnOff::Get(endpoint, &storedValue);
                if (status == EMBER_ZCL_STATUS_SUCCESS) {
                        /* Set actual state to the cluster state that was last persisted */
                        if (storedValue) {
                                event.Type = AppEventType::UvbLampActivate;
                                event.Handler = AppTask::UvbLampActivateHandler;
                        } else {
                                event.Type = AppEventType::UvbLampDeactivate;
                                event.Handler = AppTask::UvbLampDeactivateHandler;
                        }
                }
        }
        /* WATER HEATER */
        else if (endpoint == 4) {
                /* Read storedValue on/off value */
                status = Attributes::OnOff::Get(endpoint, &storedValue);
                if (status == EMBER_ZCL_STATUS_SUCCESS) {
                        /* Set actual state to the cluster state that was last persisted */
                        if (storedValue) {
                                event.Type = AppEventType::HeaterActivate;
                                event.Handler = AppTask::HeaterActivateHandler;
                        } else {
                                event.Type = AppEventType::HeaterDeactivate;
                                event.Handler = AppTask::HeaterDeactivateHandler;
                        }
                }
        }
        /* WATER FILTER PUMP */
        else if (endpoint == 5) {
                /* Read storedValue on/off value */
                status = Attributes::OnOff::Get(endpoint, &storedValue);
                if (status == EMBER_ZCL_STATUS_SUCCESS) {
                        /* Set actual state to the cluster state that was last persisted */
                        if (storedValue) {
                                event.Type = AppEventType::FilterActivate;
                                event.Handler = AppTask::FilterActivateHandler;
                        } else {
                                event.Type = AppEventType::FilterDeactivate;
                                event.Handler = AppTask::FilterDeactivateHandler;
                        }
                }
        }
        /* FEEDER */
        else if (endpoint == 6) {
                /* Read storedValue on/off value */
                status = Attributes::OnOff::Get(endpoint, &storedValue);
                if (status == EMBER_ZCL_STATUS_SUCCESS) {
                        /* Set actual state to the cluster state that was last persisted */
                        if (storedValue) {
                                event.Type = AppEventType::FeederActivate;
                                event.Handler = AppTask::FeederActivateHandler;
                        } else {
                                event.Type = AppEventType::FeederDeactivate;
                                event.Handler = AppTask::FeederDeactivateHandler;
                        }
                }
        }

        AppTask::Instance().PostEvent(event);
        AppTask::Instance().UpdateClusterState();
}
