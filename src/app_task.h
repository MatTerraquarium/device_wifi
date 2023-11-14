/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include "app_event.h"
#include "led_widget.h"

#include <platform/CHIPDeviceLayer.h>

#if CONFIG_CHIP_FACTORY_DATA
#include <platform/nrfconnect/FactoryDataProvider.h>
#else
#include <platform/nrfconnect/DeviceInstanceInfoProviderImpl.h>
#endif

struct k_timer;

class AppTask {
public:
	static AppTask &Instance()
	{
		static AppTask sAppTask;
		return sAppTask;
	};

	CHIP_ERROR StartApp();

	static void PostEvent(const AppEvent &event);

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

#if CONFIG_CHIP_FACTORY_DATA
	chip::DeviceLayer::FactoryDataProvider<chip::DeviceLayer::InternalFlashFactoryData> mFactoryDataProvider;
#endif
};
