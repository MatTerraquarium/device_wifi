/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include <cstdint>

#include "event_types.h"

class LEDWidget;

enum class AppEventType : uint8_t { None = 0, Button, ButtonPushed, ButtonReleased, Timer, UpdateLedState, 
									HotLampActivate, 
									HotLampDeactivate, 
									UvbLampActivate, 
									UvbLampDeactivate, 
									HeaterActivate, 
									HeaterDeactivate, 
									FilterActivate, 
									FilterDeactivate, 
									FeederActivate, 
									FeederDeactivate, 
									HotSensorMeasure, 
									ColdSensorMeasure, 
									WaterTempSensorMeasure, };

enum class FunctionEvent : uint8_t { NoneSelected = 0, FactoryReset };

struct AppEvent {
	union {
		struct {
			uint8_t PinNo;
			uint8_t Action;
		} ButtonEvent;
		struct {
			void *Context;
		} TimerEvent;
		struct {
			LEDWidget *LedWidget;
		} UpdateLedStateEvent;
	};

	AppEventType Type{ AppEventType::None };
	EventHandler Handler;
};
