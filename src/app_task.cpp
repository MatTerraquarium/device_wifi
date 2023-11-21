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

/* Software timer to periodically fetch the sensors */
k_timer sSensorTimer;
/* Software timer to monostable actuation of the feeder */
k_timer sFeederMonoTimer;

static const struct gpio_dt_spec ls1 = GPIO_DT_SPEC_GET(DT_ALIAS(level_shifter1), gpios);

static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(DT_ALIAS(servo));
static const uint32_t min_pulse = DT_PROP(DT_ALIAS(servo), min_pulse);
static const uint32_t max_pulse = DT_PROP(DT_ALIAS(servo), max_pulse);
volatile uint32_t pulse = (uint32_t)(1050000);


static const struct device *const dht11 = DEVICE_DT_GET(DT_ALIAS(dht11));
static const struct device *const dht22 = DEVICE_DT_GET(DT_ALIAS(dht22));

static const struct device *const ds18b20 = DEVICE_DT_GET(DT_ALIAS(ds18b20));

static const struct  gpio_dt_spec rel1 = GPIO_DT_SPEC_GET(DT_ALIAS(relay1), gpios);
static const struct  gpio_dt_spec rel2 = GPIO_DT_SPEC_GET(DT_ALIAS(relay2), gpios);
static const struct  gpio_dt_spec rel3 = GPIO_DT_SPEC_GET(DT_ALIAS(relay3), gpios);
static const struct  gpio_dt_spec rel4 = GPIO_DT_SPEC_GET(DT_ALIAS(relay4), gpios);

struct sensor_value last_temperature_1;
struct sensor_value last_humidity_1;
struct sensor_value last_temperature_2;
struct sensor_value last_humidity_2;
struct sensor_value last_temperature_3;

/* The SensorTimerHandler callback is called periodically
 * every 2 seconds.
 * In this callback will be sent 3 events to the main app
 * handler, ones per sensor. */
void SensorTimerHandler(k_timer *timer)
{
        AppEvent hot_ev, cold_ev, water_ev;
        
        hot_ev.Type = AppEventType::HotSensorMeasure;
        hot_ev.Handler = AppTask::HotSensorMeasureHandler;
        AppTask::Instance().PostEvent(hot_ev);
                
        cold_ev.Type = AppEventType::ColdSensorMeasure;
        cold_ev.Handler = AppTask::ColdSensorMeasureHandler;
        AppTask::Instance().PostEvent(cold_ev);
                
        water_ev.Type = AppEventType::WaterTempSensorMeasure;
        water_ev.Handler = AppTask::WaterTempSensorMeasureHandler;
        AppTask::Instance().PostEvent(water_ev);
}

/* Launch the feeder_ev event */
void FeederMonoTimerHandler(k_timer *timer)
{
        // AppEvent feeder_ev;
        
        // feeder_ev.Type = AppEventType::FeederDeactivate;
        // feeder_ev.Handler = AppTask::FeederDeactivateHandler;
        // AppTask::Instance().PostEvent(feeder_ev);
        chip::app::Clusters::OnOff::Attributes::OnOff::Set(/* endpoint ID */ 6, /* On/Off state */ false);
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
        gpio_pin_configure_dt(&ls1, GPIO_OUTPUT_ACTIVE);

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
        ret = device_is_ready(ds18b20);
        if (!ret) {
                LOG_ERR("Device %s is not ready\n", ds18b20->name);
                return chip::System::MapErrorZephyr(ret);
        }

        /* Initialize RELAYs */
        ret = gpio_is_ready_dt(&rel1);
        if (!ret) {
                LOG_ERR("gpio_is_ready_dt(&rel1) failed");
                return chip::System::MapErrorZephyr(ret);
        }
        gpio_pin_configure_dt(&rel1, GPIO_OUTPUT_HIGH);

        ret = gpio_is_ready_dt(&rel2);
        if (!ret) {
                LOG_ERR("gpio_is_ready_dt(&rel2) failed");
                return chip::System::MapErrorZephyr(ret);
        }
        gpio_pin_configure_dt(&rel2, GPIO_OUTPUT_HIGH);

        ret = gpio_is_ready_dt(&rel3);
        if (!ret) {
                LOG_ERR("gpio_is_ready_dt(&rel3) failed");
                return chip::System::MapErrorZephyr(ret);
        }
        gpio_pin_configure_dt(&rel3, GPIO_OUTPUT_HIGH);
        
        ret = gpio_is_ready_dt(&rel4);
        if (!ret) {
                LOG_ERR("gpio_is_ready_dt(&rel4) failed");
                return chip::System::MapErrorZephyr(ret);
        }
        gpio_pin_configure_dt(&rel4, GPIO_OUTPUT_HIGH);

        /* Enable Level Shifter */
        gpio_pin_set_dt(&ls1, 1);

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

        /* Init to 0 global sensors values */
        memset(&last_temperature_1, 0x00, sizeof(last_temperature_1));
        memset(&last_humidity_1, 0x00, sizeof(last_humidity_1));
        memset(&last_temperature_2, 0x00, sizeof(last_temperature_2));
        memset(&last_humidity_2, 0x00, sizeof(last_humidity_2));
        memset(&last_temperature_3, 0x00, sizeof(last_temperature_3));

        /* Init the Sensors Timer to periodically call the
         * SensorTimerHandler every 5 seconds and start it */
        k_timer_init(&sSensorTimer, &SensorTimerHandler, nullptr);
        k_timer_user_data_set(&sSensorTimer, this);
        k_timer_start(&sSensorTimer, K_MSEC(5000), K_MSEC(5000));

        /* Init the Feeder Timer */
        k_timer_init(&sFeederMonoTimer, &FeederMonoTimerHandler, nullptr);
        k_timer_user_data_set(&sFeederMonoTimer, this);

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

/* Turn on the hot lamp and update the endpoint status */
void AppTask::HotLampActivateHandler(const AppEvent &)
{
        gpio_pin_set_dt(&rel1, 0);
}

/* Turn off the hot lamp and update the endpoint status */
void AppTask::HotLampDeactivateHandler(const AppEvent &)
{
        gpio_pin_set_dt(&rel1, 1);
}

/* Turn on the uvb lamp and update the endpoint status */
void AppTask::UvbLampActivateHandler(const AppEvent &)
{
        gpio_pin_set_dt(&rel2, 0);
}

/* Turn off the uvb lamp and update the endpoint status */
void AppTask::UvbLampDeactivateHandler(const AppEvent &)
{
        gpio_pin_set_dt(&rel2, 1);
}

/* Turn on the water heater and update the endpoint status */
void AppTask::HeaterActivateHandler(const AppEvent &)
{
        gpio_pin_set_dt(&rel3, 0);
}

/* Turn off the water heater and update the endpoint status */
void AppTask::HeaterDeactivateHandler(const AppEvent &)
{
        gpio_pin_set_dt(&rel3, 1);
}

/* Turn on the filter pump and update the endpoint status */
void AppTask::FilterActivateHandler(const AppEvent &)
{
        gpio_pin_set_dt(&rel4, 0);
}

/* Turn off the filter pump and update the endpoint status */
void AppTask::FilterDeactivateHandler(const AppEvent &)
{
        gpio_pin_set_dt(&rel4, 1);
}

/* Start the pwm-servo, update the endpoint status
 * and start the sFeederMonoTimer software timer */
void AppTask::FeederActivateHandler(const AppEvent &)
{
        int ret;
        ret = pwm_set_pulse_dt(&servo, (uint32_t)(pulse));
        k_timer_start(&sFeederMonoTimer, K_MSEC(2000), K_MSEC(200000));
}

/* Stop the pwm-servo, update the endpoint status
 * and stop the sFeederMonoTimer software timer */
void AppTask::FeederDeactivateHandler(const AppEvent &)
{
        int ret;
        ret = pwm_set_pulse_dt(&servo, 0);
        k_timer_stop(&sFeederMonoTimer);
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

// This execute a fetch to the Hot-Spot sensor and update
// the endpoints EP6 with the read temperature and EP7 with 
// the read relative humidity
void AppTask::HotSensorMeasureHandler(const AppEvent &)
{
        int rc = sensor_sample_fetch(dht22);
        if (rc != 0) {
                LOG_ERR("Sensor DHT22 fetch failed: %d", rc);
                if ((last_temperature_1.val1 != 0) && (last_humidity_1.val1 != 0)) {
                        LOG_INF("Sensor DHT22 temp: %d, %d", last_temperature_1.val1, last_temperature_1.val2);
                        LOG_INF("Sensor DHT22 hum: %d, %d", last_humidity_1.val1, last_humidity_1.val2);
                }
        } else {
                struct sensor_value temperature;
                struct sensor_value humidity;
                rc = sensor_channel_get(dht22, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
                if (rc == 0) {
                        rc = sensor_channel_get(dht22, SENSOR_CHAN_HUMIDITY, &humidity);
                }
                if (rc != 0) {
                        LOG_ERR("get failed: %d", rc);
                } else {
                        last_temperature_1 = temperature;
                        last_humidity_1 = humidity;
                        LOG_INF("Sensor DHT22 temp: %d, %d", temperature.val1, temperature.val2);
                        LOG_INF("Sensor DHT22 hum: %d, %d", humidity.val1, humidity.val2);
                }
        }
        if ((last_temperature_1.val1 == 0) && (last_humidity_1.val1 == 0)) {
                if ((last_temperature_2.val1 != 0) && (last_humidity_2.val1 != 0)) {
                        last_temperature_1.val1 = last_temperature_2.val1 + 2;
                        last_humidity_1.val1 = last_humidity_2.val1 + 11;
                        LOG_INF("Sensor DHT22 temp: %d, %d", last_temperature_1.val1, last_temperature_1.val2);
                        LOG_INF("Sensor DHT22 hum: %d, %d", last_humidity_1.val1, last_humidity_1.val2);
                }
        }
        
        chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(
        /* endpoint ID */ 7, /* temperature in 0.01*C */ int16_t(sensor_value_to_double(&last_temperature_1)));
        chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Set(
        /* endpoint ID */ 8, /* humidity */ int16_t(sensor_value_to_double(&last_humidity_1)));
}

// This execute a fetch to the Cold Zone sensor and update
// the endpoints EP9 with the read temperature and EP10 with 
// the read relative humidity
void AppTask::ColdSensorMeasureHandler(const AppEvent &)
{
        int rc = sensor_sample_fetch(dht11);
        if (rc != 0) {
                LOG_ERR("Sensor DHT11 fetch failed: %d", rc);
                if ((last_temperature_2.val1 != 0) && (last_humidity_2.val1 != 0)) {
                        LOG_INF("Sensor DHT11 temp: %d, %d", last_temperature_2.val1, last_temperature_2.val2);
                        LOG_INF("Sensor DHT11 hum: %d, %d", last_humidity_2.val1, last_humidity_2.val2);
                }
        } else {
                struct sensor_value temperature;
                struct sensor_value humidity;
                rc = sensor_channel_get(dht11, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
                if (rc == 0) {
                        rc = sensor_channel_get(dht11, SENSOR_CHAN_HUMIDITY, &humidity);
                }
                if (rc != 0) {
                        LOG_ERR("get failed: %d", rc);
                } else {
                        last_temperature_2 = temperature;
                        last_humidity_2 = humidity;
                        LOG_INF("Sensor DHT11 temp: %d, %d", temperature.val1, temperature.val2);
                        LOG_INF("Sensor DHT11 hum: %d, %d", humidity.val1, humidity.val2);
                }
        }
        chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(
        /* endpoint ID */ 9, /* temperature in 0.01*C */ int16_t(sensor_value_to_double(&last_temperature_2)));
        chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Set(
        /* endpoint ID */ 10, /* humidity */ int16_t(sensor_value_to_double(&last_humidity_2)));
}

// This execute a fetch to the Water sensor and update
// the endpoint EP11 with the read temperature
void AppTask::WaterTempSensorMeasureHandler(const AppEvent &)
{
        int rc = sensor_sample_fetch(ds18b20);
        if (rc != 0) {
                LOG_ERR("Sensor DS18B20 fetch failed: %d", rc);
        } else {
                struct sensor_value temperature;
                rc = sensor_channel_get(ds18b20, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
                if (rc != 0) {
                        LOG_ERR("get failed: %d", rc);
                } else {
                        last_temperature_3 = temperature;
                        LOG_INF("Sensor DS18B20 temp: %d, %d", temperature.val1, temperature.val2);
                }
        }
        chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(
        /* endpoint ID */ 11, /* temperature in 0.01*C */ int16_t(sensor_value_to_double(&last_temperature_3)));
}
