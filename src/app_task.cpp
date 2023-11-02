/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "app_task.h"
#include "app_config.h"
#include "led_util.h"

#include <platform/CHIPDeviceLayer.h>

#include "board_util.h"
#include <app/server/OnboardingCodesUtil.h>
#include <app/server/Server.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>
#include <system/SystemError.h>

#ifdef CONFIG_CHIP_WIFI
#include <app/clusters/network-commissioning/network-commissioning.h>
#include <platform/nrfconnect/wifi/NrfWiFiDriver.h>
#endif

#ifdef CONFIG_CHIP_OTA_REQUESTOR
#include "ota_util.h"
#endif

#include <stdio.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/pwm.h>
#include <app-common/zap-generated/attributes/Accessors.h>

LOG_MODULE_DECLARE(app, CONFIG_CHIP_APP_LOG_LEVEL);

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::Credentials;
using namespace ::chip::DeviceLayer;

namespace
{
constexpr size_t kAppEventQueueSize = 10;
constexpr uint32_t kFactoryResetTriggerTimeout = 6000;

K_MSGQ_DEFINE(sAppEventQueue, sizeof(AppEvent), kAppEventQueueSize, alignof(AppEvent));
k_timer sFunctionTimer;

LEDWidget sStatusLED;
#if NUMBER_OF_LEDS == 2
FactoryResetLEDsWrapper<1> sFactoryResetLEDs{ { FACTORY_RESET_SIGNAL_LED } };
#else
FactoryResetLEDsWrapper<3> sFactoryResetLEDs{ { FACTORY_RESET_SIGNAL_LED, FACTORY_RESET_SIGNAL_LED1,
						FACTORY_RESET_SIGNAL_LED2 } };
#endif

bool sIsNetworkProvisioned = false;
bool sIsNetworkEnabled = false;
bool sHaveBLEConnections = false;
} /* namespace */

namespace LedConsts
{
namespace StatusLed
{
	namespace Unprovisioned
	{
		constexpr uint32_t kOn_ms{ 100 };
		constexpr uint32_t kOff_ms{ kOn_ms };
	} /* namespace Unprovisioned */
	namespace Provisioned
	{
		constexpr uint32_t kOn_ms{ 50 };
		constexpr uint32_t kOff_ms{ 950 };
	} /* namespace Provisioned */

} /* namespace StatusLed */
} /* namespace LedConsts */

#ifdef CONFIG_CHIP_WIFI
app::Clusters::NetworkCommissioning::Instance
	sWiFiCommissioningInstance(0, &(NetworkCommissioning::NrfWiFiDriver::Instance()));
#endif

k_timer sSensorTimer;

static const struct gpio_dt_spec ls1 = GPIO_DT_SPEC_GET(DT_ALIAS(level_shifter1), gpios);
static const struct gpio_dt_spec ls2 = GPIO_DT_SPEC_GET(DT_ALIAS(level_shifter2), gpios);
static const struct gpio_dt_spec ls3 = GPIO_DT_SPEC_GET(DT_ALIAS(level_shifter3), gpios);

static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(DT_ALIAS(servo));
static const uint32_t min_pulse = DT_PROP(DT_ALIAS(servo), min_pulse);
static const uint32_t max_pulse = DT_PROP(DT_ALIAS(servo), max_pulse);

static const struct device *const dht11 = DEVICE_DT_GET(DT_ALIAS(dht11));
static const struct device *const dht22 = DEVICE_DT_GET(DT_ALIAS(dht22));

//static const struct device *const ds18b20 = DEVICE_DT_GET(DT_ALIAS(ds18b20));

static const struct  gpio_dt_spec rel1 = GPIO_DT_SPEC_GET(DT_ALIAS(relay1), gpios);
static const struct  gpio_dt_spec rel2 = GPIO_DT_SPEC_GET(DT_ALIAS(relay2), gpios);
static const struct  gpio_dt_spec rel3 = GPIO_DT_SPEC_GET(DT_ALIAS(relay3), gpios);
static const struct  gpio_dt_spec rel4 = GPIO_DT_SPEC_GET(DT_ALIAS(relay4), gpios);

void SensorTimerHandler(k_timer *timer)
{
        AppEvent event;
        event.Type = AppEventType::SensorMeasure;
        event.Handler = AppTask::SensorMeasureHandler;
        AppTask::Instance().PostEvent(event);
}

void StartSensorTimer(uint32_t aTimeoutMs)
{
        k_timer_start(&sSensorTimer, K_MSEC(aTimeoutMs), K_MSEC(aTimeoutMs));
}

void StopSensorTimer()
{
        k_timer_stop(&sSensorTimer);
}

CHIP_ERROR AppTask::Init()
{
	/* Initialize CHIP stack */
	LOG_INF("Init CHIP stack");

	CHIP_ERROR err = chip::Platform::MemoryInit();
	if (err != CHIP_NO_ERROR) {
		LOG_ERR("Platform::MemoryInit() failed");
		return err;
	}

	err = PlatformMgr().InitChipStack();
	if (err != CHIP_NO_ERROR) {
		LOG_ERR("PlatformMgr().InitChipStack() failed");
		return err;
	}

#if defined(CONFIG_NET_L2_OPENTHREAD)
	err = ThreadStackMgr().InitThreadStack();
	if (err != CHIP_NO_ERROR) {
		LOG_ERR("ThreadStackMgr().InitThreadStack() failed: %s", ErrorStr(err));
		return err;
	}

#ifdef CONFIG_OPENTHREAD_MTD_SED
	err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_SleepyEndDevice);
#elif CONFIG_OPENTHREAD_MTD
	err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_MinimalEndDevice);
#else
	err = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_Router);
#endif /* CONFIG_OPENTHREAD_MTD_SED */
	if (err != CHIP_NO_ERROR) {
		LOG_ERR("ConnectivityMgr().SetThreadDeviceType() failed: %s", ErrorStr(err));
		return err;
	}

#elif defined(CONFIG_CHIP_WIFI)
	sWiFiCommissioningInstance.Init();
#else
	return CHIP_ERROR_INTERNAL;
#endif /* CONFIG_NET_L2_OPENTHREAD */

	/* Initialize LEDs */
	LEDWidget::InitGpio();
	LEDWidget::SetStateUpdateCallback(LEDStateUpdateHandler);

	sStatusLED.Init(SYSTEM_STATE_LED);

	UpdateStatusLED();

	/* Initialize buttons */
	int ret = dk_buttons_init(ButtonEventHandler);
	if (ret) {
		LOG_ERR("dk_buttons_init() failed");
		return chip::System::MapErrorZephyr(ret);
	}

	/* Initialize level_shifter output enable */
	ret = gpio_is_ready_dt(&ls1);
	if (!ret) {
		LOG_ERR("gpio_is_ready_dt(&ls1) failed");
		return chip::System::MapErrorZephyr(ret);
	}
	gpio_pin_configure_dt(&ls1, GPIO_OUTPUT_INACTIVE);

	ret = gpio_is_ready_dt(&ls2);
	if (!ret) {
		LOG_ERR("gpio_is_ready_dt(&ls2) failed");
		return chip::System::MapErrorZephyr(ret);
	}
	gpio_pin_configure_dt(&ls2, GPIO_OUTPUT_INACTIVE);

	ret = gpio_is_ready_dt(&ls3);
	if (!ret) {
		LOG_ERR("gpio_is_ready_dt(&ls3) failed");
		return chip::System::MapErrorZephyr(ret);
	}
	gpio_pin_configure_dt(&ls3, GPIO_OUTPUT_INACTIVE);

	/* Initialize SERVO */
	ret = device_is_ready(servo.dev);
	if (!ret) {
		LOG_ERR("Device %s is not ready\n", servo.dev->name);
		return chip::System::MapErrorZephyr(ret);
	}

	/* Initialize DHT11 */
	ret = device_is_ready(dht11);
	if (!ret) {
		LOG_ERR("Device %s is not ready\n", dht11->name);
		return chip::System::MapErrorZephyr(ret);
	}

	/* Initialize DHT22 */
	ret = device_is_ready(dht22);
	if (!ret) {
		LOG_ERR("Device %s is not ready\n", dht22->name);
		return chip::System::MapErrorZephyr(ret);
	}

	/* Initialize DS18B20 */
//	ret = device_is_ready(ds18b20);
//	if (!ret) {
//		LOG_ERR("Device %s is not ready\n", ds18b20->name);
//		return chip::System::MapErrorZephyr(ret);
//	}

	/* Initialize RELAYs */
	ret = gpio_is_ready_dt(&rel1);
	if (!ret) {
		LOG_ERR("gpio_is_ready_dt(&rel1) failed");
		return chip::System::MapErrorZephyr(ret);
	}
	gpio_pin_configure_dt(&rel1, GPIO_OUTPUT_INACTIVE);

	ret = gpio_is_ready_dt(&rel2);
	if (!ret) {
		LOG_ERR("gpio_is_ready_dt(&rel2) failed");
		return chip::System::MapErrorZephyr(ret);
	}
	gpio_pin_configure_dt(&rel2, GPIO_OUTPUT_INACTIVE);

	ret = gpio_is_ready_dt(&rel3);
	if (!ret) {
		LOG_ERR("gpio_is_ready_dt(&rel3) failed");
		return chip::System::MapErrorZephyr(ret);
	}
	gpio_pin_configure_dt(&rel3, GPIO_OUTPUT_INACTIVE);
	
	ret = gpio_is_ready_dt(&rel4);
	if (!ret) {
		LOG_ERR("gpio_is_ready_dt(&rel4) failed");
		return chip::System::MapErrorZephyr(ret);
	}
	gpio_pin_configure_dt(&rel4, GPIO_OUTPUT_INACTIVE);

	/* Test Level Shifters */
	gpio_pin_set_dt(&ls1, 1);
	gpio_pin_set_dt(&ls2, 1);
	gpio_pin_set_dt(&ls3, 1);

	/* Test Realays */
	gpio_pin_toggle_dt(&rel1);
	gpio_pin_toggle_dt(&rel1);
	gpio_pin_toggle_dt(&rel2);
	gpio_pin_toggle_dt(&rel2);
	gpio_pin_toggle_dt(&rel3);
	gpio_pin_toggle_dt(&rel3);
	gpio_pin_toggle_dt(&rel4);
	gpio_pin_toggle_dt(&rel4);

	/* Test Servo */
	uint32_t pulse_width = (uint32_t)((min_pulse + max_pulse) / 2);
	ret = pwm_set_pulse_dt(&servo, pulse_width);
	ret = pwm_set_pulse_dt(&servo, 0);

	/* Test DHT11 */
	struct sensor_value temperature_dht11;
	struct sensor_value humidity_dht11;
	ret = sensor_sample_fetch(dht11);
	if (ret != 0) {
		LOG_ERR("Sensor fetch failed: %d\n", ret);
		return chip::System::MapErrorZephyr(ret);
	}
	ret = sensor_channel_get(dht22, SENSOR_CHAN_AMBIENT_TEMP, &temperature_dht11);
	if (ret == 0) {
		ret = sensor_channel_get(dht22, SENSOR_CHAN_HUMIDITY, &humidity_dht11);
	}
	if (ret != 0) {
		LOG_ERR("get failed: %d\n", ret);
		return chip::System::MapErrorZephyr(ret);
	}
	double temperature_dht11_d = sensor_value_to_double(&temperature_dht11);
	double humidity_dht11_d = sensor_value_to_double(&humidity_dht11);

	/* Test DHT22 */
	struct sensor_value temperature_dht22;
	struct sensor_value humidity_dht22;
	ret = sensor_sample_fetch(dht22);
	if (ret != 0) {
		LOG_ERR("Sensor fetch failed: %d\n", ret);
		return chip::System::MapErrorZephyr(ret);
	}
	ret = sensor_channel_get(dht22, SENSOR_CHAN_AMBIENT_TEMP, &temperature_dht22);
	if (ret == 0) {
		ret = sensor_channel_get(dht22, SENSOR_CHAN_HUMIDITY, &humidity_dht22);
	}
	if (ret != 0) {
		LOG_ERR("get failed: %d\n", ret);
		return chip::System::MapErrorZephyr(ret);
	}
	double temperature_dht22_d = sensor_value_to_double(&temperature_dht22);
	double humidity_dht22_d = sensor_value_to_double(&humidity_dht22);

	/* Test DS18B20 */
//	struct sensor_value temperature_ds18b20;
//	ret = sensor_sample_fetch(ds18b20);
//		if (ret != 0) {
//		LOG_ERR("Sensor fetch failed: %d\n", ret);
//		return chip::System::MapErrorZephyr(ret);
//	}
//	ret = sensor_channel_get(ds18b20, SENSOR_CHAN_AMBIENT_TEMP, &temperature_ds18b20);
//	if (ret != 0) {
//		LOG_ERR("get failed: %d\n", ret);
//		return chip::System::MapErrorZephyr(ret);
//	}
//	double temperature_ds18b20_d = sensor_value_to_double(&temperature_ds18b20);



	/* Initialize function timer */
	k_timer_init(&sFunctionTimer, &AppTask::FunctionTimerTimeoutCallback, nullptr);
	k_timer_user_data_set(&sFunctionTimer, this);

	/* Initialize CHIP server */
#if CONFIG_CHIP_FACTORY_DATA
	ReturnErrorOnFailure(mFactoryDataProvider.Init());
	SetDeviceInstanceInfoProvider(&mFactoryDataProvider);
	SetDeviceAttestationCredentialsProvider(&mFactoryDataProvider);
	SetCommissionableDataProvider(&mFactoryDataProvider);
#else
	SetDeviceInstanceInfoProvider(&DeviceInstanceInfoProviderMgrImpl());
	SetDeviceAttestationCredentialsProvider(Examples::GetExampleDACProvider());
#endif

	static chip::CommonCaseDeviceServerInitParams initParams;
	(void)initParams.InitializeStaticResourcesBeforeServerInit();

	ReturnErrorOnFailure(chip::Server::GetInstance().Init(initParams));
	ConfigurationMgr().LogDeviceConfig();
	PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

	/*
	 * Add CHIP event handler and start CHIP thread.
	 * Note that all the initialization code should happen prior to this point to avoid data races
	 * between the main and the CHIP threads.
	 */
	PlatformMgr().AddEventHandler(ChipEventHandler, 0);

	err = PlatformMgr().StartEventLoopTask();
	if (err != CHIP_NO_ERROR) {
		LOG_ERR("PlatformMgr().StartEventLoopTask() failed");
		return err;
	}

	k_timer_init(&sSensorTimer, &SensorTimerHandler, nullptr);
	k_timer_user_data_set(&sSensorTimer, this);

	return CHIP_NO_ERROR;
}

CHIP_ERROR AppTask::StartApp()
{
	ReturnErrorOnFailure(Init());

	AppEvent event = {};

	while (true) {
		k_msgq_get(&sAppEventQueue, &event, K_FOREVER);
		DispatchEvent(event);
	}

	return CHIP_NO_ERROR;
}

void AppTask::ButtonEventHandler(uint32_t buttonState, uint32_t hasChanged)
{
	AppEvent button_event;
	button_event.Type = AppEventType::Button;

	if (FUNCTION_BUTTON_MASK & hasChanged) {
		button_event.ButtonEvent.PinNo = FUNCTION_BUTTON;
		button_event.ButtonEvent.Action =
			static_cast<uint8_t>((FUNCTION_BUTTON_MASK & buttonState) ? AppEventType::ButtonPushed :
										    AppEventType::ButtonReleased);
		button_event.Handler = FunctionHandler;
		PostEvent(button_event);
	}
}

void AppTask::FunctionTimerTimeoutCallback(k_timer *timer)
{
	if (!timer) {
		return;
	}

	AppEvent event;
	event.Type = AppEventType::Timer;
	event.TimerEvent.Context = k_timer_user_data_get(timer);
	event.Handler = FunctionTimerEventHandler;
	PostEvent(event);
}

void AppTask::FunctionTimerEventHandler(const AppEvent &)
{
	if (Instance().mFunction == FunctionEvent::FactoryReset) {
		Instance().mFunction = FunctionEvent::NoneSelected;
		LOG_INF("Factory Reset triggered");

		sStatusLED.Set(true);
		sFactoryResetLEDs.Set(true);

		chip::Server::GetInstance().ScheduleFactoryReset();
	}
}

void AppTask::FunctionHandler(const AppEvent &event)
{
	if (event.ButtonEvent.PinNo != FUNCTION_BUTTON)
		return;

	if (event.ButtonEvent.Action == static_cast<uint8_t>(AppEventType::ButtonPushed)) {
		Instance().StartTimer(kFactoryResetTriggerTimeout);
		Instance().mFunction = FunctionEvent::FactoryReset;
	} else if (event.ButtonEvent.Action == static_cast<uint8_t>(AppEventType::ButtonReleased)) {
		if (Instance().mFunction == FunctionEvent::FactoryReset) {
			sFactoryResetLEDs.Set(false);
			UpdateStatusLED();
			Instance().CancelTimer();
			Instance().mFunction = FunctionEvent::NoneSelected;
			LOG_INF("Factory Reset has been Canceled");
		}
	}
}

void AppTask::LEDStateUpdateHandler(LEDWidget &ledWidget)
{
	AppEvent event;
	event.Type = AppEventType::UpdateLedState;
	event.Handler = UpdateLedStateEventHandler;
	event.UpdateLedStateEvent.LedWidget = &ledWidget;
	PostEvent(event);
}

void AppTask::UpdateLedStateEventHandler(const AppEvent &event)
{
	if (event.Type == AppEventType::UpdateLedState) {
		event.UpdateLedStateEvent.LedWidget->UpdateState();
	}
}

void AppTask::UpdateStatusLED()
{
	/* Update the status LED.
	 *
	 * If IPv6 networking and service provisioned, keep the LED On constantly.
	 *
	 * If the system has BLE connection(s) uptill the stage above, THEN blink the LED at an even
	 * rate of 100ms.
	 *
	 * Otherwise, blink the LED for a very short time. */
	if (sIsNetworkProvisioned && sIsNetworkEnabled) {
		sStatusLED.Set(true);
	} else if (sHaveBLEConnections) {
		sStatusLED.Blink(LedConsts::StatusLed::Unprovisioned::kOn_ms,
				 LedConsts::StatusLed::Unprovisioned::kOff_ms);
	} else {
		sStatusLED.Blink(LedConsts::StatusLed::Provisioned::kOn_ms, LedConsts::StatusLed::Provisioned::kOff_ms);
	}
}

void AppTask::SensorActivateHandler(const AppEvent &)
{
        StartSensorTimer(500);
}

void AppTask::SensorDeactivateHandler(const AppEvent &)
{
        StopSensorTimer();
}

void AppTask::SensorMeasureHandler(const AppEvent &)
{
		int rc = sensor_sample_fetch(dht22);
		if (rc != 0) {
			LOG_ERR("Sensor fetch failed: %d\n", rc);
			return;
		}

		struct sensor_value temperature;
		struct sensor_value humidity;

		rc = sensor_channel_get(dht22, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
		if (rc == 0) {
			rc = sensor_channel_get(dht22, SENSOR_CHAN_HUMIDITY, &humidity);
		}
		if (rc != 0) {
			LOG_ERR("get failed: %d\n", rc);
			return;
		}
		sensor_value_to_double(&temperature);
		sensor_value_to_double(&humidity);
		chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(
                /* endpoint ID */ 1, /* temperature in 0.01*C */ int16_t(sensor_value_to_double(&temperature)));
        // chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(
        //         /* endpoint ID */ 1, /* temperature in 0.01*C */ int16_t(rand() % 5000));
}

void AppTask::ChipEventHandler(const ChipDeviceEvent *event, intptr_t /* arg */)
{
	switch (event->Type) {
	case DeviceEventType::kCHIPoBLEAdvertisingChange:
		sHaveBLEConnections = ConnectivityMgr().NumBLEConnections() != 0;
		UpdateStatusLED();
		break;
#if defined(CONFIG_NET_L2_OPENTHREAD)
	case DeviceEventType::kDnssdInitialized:
#if CONFIG_CHIP_OTA_REQUESTOR
		InitBasicOTARequestor();
#endif /* CONFIG_CHIP_OTA_REQUESTOR */
		break;
	case DeviceEventType::kThreadStateChange:
		sIsNetworkProvisioned = ConnectivityMgr().IsThreadProvisioned();
		sIsNetworkEnabled = ConnectivityMgr().IsThreadEnabled();
#elif defined(CONFIG_CHIP_WIFI)
	case DeviceEventType::kWiFiConnectivityChange:
		sIsNetworkProvisioned = ConnectivityMgr().IsWiFiStationProvisioned();
		sIsNetworkEnabled = ConnectivityMgr().IsWiFiStationEnabled();
#if CONFIG_CHIP_OTA_REQUESTOR
		if (event->WiFiConnectivityChange.Result == kConnectivity_Established) {
			InitBasicOTARequestor();
		}
#endif /* CONFIG_CHIP_OTA_REQUESTOR */
#endif
		UpdateStatusLED();
		break;
	default:
		break;
	}
}

void AppTask::CancelTimer()
{
	k_timer_stop(&sFunctionTimer);
}

void AppTask::StartTimer(uint32_t timeoutInMs)
{
	k_timer_start(&sFunctionTimer, K_MSEC(timeoutInMs), K_NO_WAIT);
}

void AppTask::PostEvent(const AppEvent &event)
{
	if (k_msgq_put(&sAppEventQueue, &event, K_NO_WAIT) != 0) {
		LOG_INF("Failed to post event to app task event queue");
	}
}

void AppTask::DispatchEvent(const AppEvent &event)
{
	if (event.Handler) {
		event.Handler(event);
	} else {
		LOG_INF("Event received with no handler. Dropping event.");
	}
}
