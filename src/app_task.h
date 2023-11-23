/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */



/* ****************************************************************************
 *
 *  SENSORS MANAGEMENT - app_task.cpp
 * 
 * sSensorTimer: 2 second periodic software timer that trigger the events
 *               hot_ev, cold_ev and water_ev, which do the fetch of 
 *               corresponding sensor
 * 
 * hot_ev: app event that trigger the HotSensorMeasureHandler
 * cold_ev: app event that trigger the ColdSensorMeasureHandler
 * water_ev: app event that trigger the WaterTempSensorMeasureHandler
 *
 * HotSensorMeasureHandler: fetch the Hot-Spot sensor and update the endpoints
 *                          with temperature and humidity values
 * 
 * ColdSensorMeasureHandler: fetch the Cold-Zone sensor and update the relative
 *                           endpoints with temperature and humidity values
 * 
 * WaterTempSensorMeasureHandler: fetch the Water sensor and update the relative
 *                                endpoint with temperature value
 *  
 * ***************************************************************************/



/* ****************************************************************************
 *
 *  ACTUATORS MANAGEMENT - app_task.cpp
 * 
 * sFeederMonoTimer: software timer for monostable monostable actuation 
 *                   of the feeder
 * 
 * feeder_ev: event to internally trigger the FeederDeactivateHandler
 * 
 * FeederMonoTimerHandler: launch the feeder_ev event
 *  
 * HotLampActivateHandler: turn the hot amp On
 * HotLampDeactivateHandler: turn the hot lamp Off
 * 
 * UvbLampActivateHandler: turn the uvb lamp On
 * UvbLampDeactivateHandler: turn the uvb lamp Off
 * 
 * HeaterActivateHandler: turn the water heater On
 * HeaterDeactivateHandler: turn the water heater Off
 * 
 * FilterActivateHandler: turn the filter pump On
 * FilterDeactivateHandler: turn the filter pump Off
 * 
 * FeederActivateHandler: turn the feeder pwm-servo On
 * FeederDeactivateHandler: turn the feeder pwm-servo Off
 * 
 * ***************************************************************************/



/* ****************************************************************************
 *
 *  MATTER COMMANDS LISTNER - zcl_callbacks.cpp
 * 
 * MatterPostAttributeChangeCallback: callback of matter command is received
 * 
 * ***************************************************************************/



#pragma once

#include "app_event.h"
#include "led_widget.h"

#include <platform/CHIPDeviceLayer.h>

#if CONFIG_CHIP_FACTORY_DATA
#include <platform/nrfconnect/FactoryDataProvider.h>
#else
#include <platform/nrfconnect/DeviceInstanceInfoProviderImpl.h>
#endif

//using namespace ::chip;
//using namespace ::chip::app::Clusters;

struct k_timer;

class AppTask {
public:
        static AppTask &Instance()
        {
                static AppTask sAppTask;
                return sAppTask;
        };

        CHIP_ERROR StartApp();
        void UpdateClusterState();
        static void PostEvent(const AppEvent &event);

        static void LightingLedActivateHandler(const AppEvent &);
        static void LightingLedDeactivateHandler(const AppEvent &);

        static void HotSensorMeasureHandler(const AppEvent &);
        static void ColdSensorMeasureHandler(const AppEvent &);
        static void WaterTempSensorMeasureHandler(const AppEvent &);

        static void HotLampActivateHandler(const AppEvent &);
        static void HotLampDeactivateHandler(const AppEvent &);
        static void UvbLampActivateHandler(const AppEvent &);
        static void UvbLampDeactivateHandler(const AppEvent &);
        static void HeaterActivateHandler(const AppEvent &);
        static void HeaterDeactivateHandler(const AppEvent &);
        static void FilterActivateHandler(const AppEvent &);
        static void FilterDeactivateHandler(const AppEvent &);
        static void FeederActivateHandler(const AppEvent &);
        static void FeederDeactivateHandler(const AppEvent &);

private:
        CHIP_ERROR Init();

        void CancelTimer();
        void StartTimer(uint32_t timeoutInMs);

        static void DispatchEvent(const AppEvent &event);
        static void UpdateLedStateEventHandler(const AppEvent &event);
        static void FunctionHandler(const AppEvent &event);
        static void FunctionTimerEventHandler(const AppEvent &event);

        static void ChipEventHandler(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t arg);
        static void ButtonEventHandler(uint32_t buttonState, uint32_t hasChanged);
        static void LEDStateUpdateHandler(LEDWidget &ledWidget);
        static void FunctionTimerTimeoutCallback(k_timer *timer);
        static void UpdateStatusLED();

        FunctionEvent mFunction = FunctionEvent::NoneSelected;
        bool mFunctionTimerActive = false;

        bool mLightingLedState = false;
        bool mHoltLampState = false;
        bool mUvbLampState = false;
        bool mHeaterState = false;
        bool mFilterState = false;
        bool mFeederState = false;

#if CONFIG_CHIP_FACTORY_DATA
        chip::DeviceLayer::FactoryDataProvider<chip::DeviceLayer::InternalFlashFactoryData> mFactoryDataProvider;
#endif
};
