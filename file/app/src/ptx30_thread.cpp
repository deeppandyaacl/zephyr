/**
* SIBEL INC ("SIBEL HEALTH") CONFIDENTIAL
*
* Copyright 2018-2024 [Sibel Inc.], All Rights Reserved.
*
* NOTICE: All information contained herein is, and remains the property of SIBEL
* INC. The intellectual and technical concepts contained herein are proprietary
* to SIBEL INC and may be covered by U.S. and Foreign Patents, patents in
* process, and are protected by trade secret or copyright law. Dissemination of
* this information or reproduction of this material is strictly forbidden unless
* prior written permission is obtained from SIBEL INC. Access to the source code
* contained herein is hereby forbidden to anyone except current SIBEL INC
* employees, managers or contractors who have executed Confidentiality and
* Non-disclosure agreements explicitly covering such access.
* The copyright notice above does not evidence any actual or intended
* publication or disclosure of this source code, which includes information that
* is confidential and/or proprietary, and is a trade secret of SIBEL INC.
*
* ANY REPRODUCTION, MODIFICATION, DISTRIBUTION, PUBLIC PERFORMANCE, OR PUBLIC
* DISPLAY OF OR THROUGH USE OF THIS SOURCE CODE WITHOUT THE EXPRESS WRITTEN
* CONSENT OF COMPANY IS STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE
* LAWS AND INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
* CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS TO
* REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE, USE, OR
* SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
*
*/

#if defined(CONFIG_PTX30)
#include "ptx30_thread.h"
#include "session_thread.h"
#include "cmd_service/ble_cmd.h"
#include <sh/drv2624.h>
#include <sh/sh_data_logger.h>
#include <ble_base/ble.h>
#include <stdio.h>
#include <cmd_service/ble_cmd.h>
#include <dl_service/ble_dl.h>
#include "pd_service/ble_pd.h"
#include <sh/sh_common.h>
#include <plat/ptxPlat.h>
#include "memfault/core/trace_event.h"

#include <stdint.h>
#include <math.h>


#ifdef CONFIG_WATCHDOG
#include "watchdog.h"
#endif

#include <fault_handler.h>

#if defined (CONFIG_KTD2026)
#include <sh/sh_ktd2026_led.h>
#endif

#if defined (CONFIG_LP5813)
#include <sh/sh_lp5813_led.h>
#endif 

#include "ptx30w_zephyr.h"
/* =========================================================================
 * Logging Registration
 * ========================================================================= */
#include <zephyr/logging/log.h>
#include "haptic_motor_handler.h"

LOG_MODULE_REGISTER(ptx30_thread, LOG_LEVEL_ERR);

// #define PTX30_SEM_TIMEOUT_SEC       2//5
//Note: set both value in such way that multiplication is 2000
#define PTX30_SEM_TIMEOUT_MSEC       100
#define PTX30_SEM_COUNTER_CHECK      20
/* =========================================================================
 * Zbus Declarations/References
 * ========================================================================= */
ZBUS_CHAN_DEFINE(
        ptx30_chan,           /// Name
        ptxSystemStatus_t,    /// Message type
        NULL,                 /// Validator (usually NULL)
        NULL,                 /// User data
        ZBUS_OBSERVERS_EMPTY, /// Observers
        ZBUS_MSG_INIT(0)      /// Initial values
);

/* =========================================================================
 * Zbus Observer Declarations/References
 * ========================================================================= */
#if defined(CONFIG_DRV2624) || defined(CONFIG_PTX30)
/* Define subscriber to Service Commands channel */
ZBUS_MSG_SUBSCRIBER_DEFINE(service_cmd_sub);
#endif

typedef enum states_t{
    CHARGING = 0,
    LOW_BATTERY,
    DEAD_BATTERY,
    FULLY_CHARGED,
}pmic_state_t;

typedef enum ledstates_t {
    LED_RESOURCE_IDLE = 0x00,
    LED_RESOURCE_OCCUPIED_BY_PTX_BAT_FULL,
    LED_RESOURCE_OCCUPIED_BY_PTX_PULSE_CHARGING,
    LED_RESOURCE_OCCUPIED_BY_PTX_CHARGING,
    LED_RESOURCE_OCCUPIED_BY_PTX_LOWBATTERY,
    LED_RESOURCE_OCCUPIED_BY_PTX_DEAD,
    LED_RESOURCE_OCCUPIED_BY_ADV,
    LED_RESOURCE_OCCUPIED_BY_CONN,
    LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD,
    LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR
}led_state_t;

#define VALIDATION_CNT    5

static led_state_t g_led_resource_state = LED_RESOURCE_IDLE;
static pmic_state_t pmic_state = CHARGING;

/* =========================================================================
 * BLE
 * ========================================================================= */
#include <bas_service/ble_bas.h>

/* =========================================================================
 * Thread Variables and Definitions
 * ========================================================================= */
#include "zephyr/kernel.h"
#include "zephyr/kernel/thread.h"
#include "zephyr/kernel/thread_stack.h"
#include "PTX30W_TDC_HLP.h"
#include "microphone_thread.h"

// The PTX30 SDK uses a lot of memory, allocate 2kB to not crash the stack
#define STACKSIZE 2048
K_THREAD_STACK_DEFINE(powermngr_stack, STACKSIZE);
struct k_thread powermngr_thread;
static uint8_t battery_level_led = 0;

#ifdef CONFIG_WATCHDOG
/* Watchdog is creating issue while performing DFU so disable for this thread */
#if 0
static int wdt_ptx_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
#endif 
#endif

/* =========================================================================
 * Functions
 * ========================================================================= */

#define PTX_ON_DEBOUNCE_CNT     2//40//20
#define PTX_OFF_DEBOUNCE_CNT    20//40//20
#define VOLT_BUF_SIZE           5


#ifdef CONFIG_WATCHDOG

/* Watchdog is creating issue while performing DFU so disable for this thread */

#if 0
/** 
 * @brief Start Stop watchdog for PTX threads
 *
 * This function starts the watchdog for thread when sensor has been started to 
 * monitor its health. Once the session has been stopped, watchdog of the threads
 * will be stopped.
 * 
 * @param start_watchdog: Start/Stop watchdog flag
 */
static void start_stop_wdt(bool start_watchdog) 
{
    LOG_INF("PTX Receiving start watchdog flag : %d and channel id is : %d", start_watchdog, wdt_ptx_channel_id);

    if(start_watchdog) {
        // Starting watchdog for all sensor threads 
        if(wdt_ptx_channel_id < 0) {
            wdt_ptx_channel_id = task_wdt_add_thread(WATCHDOG_LOW_TIMEOUT_MSEC);
            LOG_INF("PTX watchdog enabled with channel id :%d", wdt_ptx_channel_id);
        } else {
            LOG_INF("PTX watchdog already enabled");
        }
    } else {
        // Stop watchdog to monitor all sensor threads 
        if(wdt_ptx_channel_id >= 0) {
            LOG_INF("PTX watchdog disabled with channel id :%d", wdt_ptx_channel_id);
            task_wdt_delete_thread(wdt_ptx_channel_id);
            wdt_ptx_channel_id = WATCHDOG_CHANNEL_DEFAULT_ID;
        } else {
            LOG_INF("PTX watchdog already disabled ");
        }
    }
}
#endif // 0
#endif // CONFIG_WATCHDOG


/**
* @brief Determine the BLE battery level for the given voltage
*
* @param voltage (mV)
*
* @return uint8_t Battery Percentage
*/
uint8_t powermngr::determine_power_level(uint16_t voltage)
{
    // MIN and MAX boundary checks
    if (voltage <= MIN_BAT_VOLTAGE) {
        LOG_DBG("Battery Level 0");
        return 0;
    } else if (voltage >= MAX_BAT_VOLTAGE) {
        LOG_DBG("Battery Level 100");
        return 100;
    }

    uint8_t battery_level;

    const double coefficient = 281.806;
    const double base = -29705.0;
    const double power = -7.18;
    double Vbat = voltage / 1000.0;

    double percentage = coefficient * exp(base * pow(Vbat, power));
    battery_level = (uint8_t)percentage;

    if (battery_level <= 0) {
        battery_level = 0;
    }
    else if (battery_level < 20) {
        battery_level = 10;
    }
    else if (battery_level < 40) {
        battery_level = 20;
    }
    else if (battery_level < 60) {
        battery_level = 40;
    }
    else if (battery_level < 80) {
        battery_level = 60;
    }
    else if (battery_level < 95) {
        battery_level = 80;
    }
    else if (battery_level <= 255) {
        battery_level = 100;
    }

    LOG_DBG("Battery Level %d", battery_level);
    return battery_level;
}

/**
* @brief Test the determine_power_level_function 
*/
void determine_power_level_test()
{
    uint8_t level;
    uint16_t battery_range = MAX_BAT_VOLTAGE - MIN_BAT_VOLTAGE;
    uint16_t battery_10 = MIN_BAT_VOLTAGE + .1 * battery_range;
    uint16_t battery_20 = MIN_BAT_VOLTAGE + .2 * battery_range;
    uint16_t battery_40 = MIN_BAT_VOLTAGE + .4 * battery_range;
    uint16_t battery_60 = MIN_BAT_VOLTAGE + .6 * battery_range;
    uint16_t battery_80 = MIN_BAT_VOLTAGE + .8 * battery_range;

    LOG_DBG("===== Testing Battery Level Functionality =====");
    level = powermngr::determine_power_level(2400);
    LOG_DBG("Input %4d mV | Expected:   0%% | Calculated %d%%", 2400, level);

    level = powermngr::determine_power_level(MIN_BAT_VOLTAGE);
    LOG_DBG("Input %4d mV | Expected:   0%% | Calculated %d%%", MIN_BAT_VOLTAGE, level);

    level = powermngr::determine_power_level(battery_10);
    LOG_DBG("Input %4d mV | Expected:  10%% | Calculated %d%%", battery_10, level);

    level = powermngr::determine_power_level(battery_20);
    LOG_DBG("Input %4d mV | Expected:  20%% | Calculated %d%%", battery_20, level);

    level = powermngr::determine_power_level(battery_40);
    LOG_DBG("Input %4d mV | Expected:  40%% | Calculated %d%%", battery_40, level);

    level = powermngr::determine_power_level(battery_60);
    LOG_DBG("Input %4d mV | Expected:  60%% | Calculated %d%%", battery_60, level);

    level = powermngr::determine_power_level(battery_80);
    LOG_DBG("Input %4d mV | Expected:  80%% | Calculated %d%%", battery_80, level);
    
    level = powermngr::determine_power_level(MAX_BAT_VOLTAGE);
    LOG_DBG("Input %4d mV | Expected: 100%% | Calculated %d%%", MAX_BAT_VOLTAGE, level);

    LOG_DBG("===============================================");
}

void ptx30_shipmode_pattern(void)
{
#if defined (CONFIG_DRV2624)
    haptic_params_t haptic_param;
    memset(&haptic_param, 0, sizeof(haptic_params_t));
#endif // CONFIG_DRV2624

#ifdef CONFIG_WATCHDOG
    /* Watchdog is creating issue while performing DFU so disable for this thread */
    #if 0
        // Turn on watchdog channel 
        start_stop_wdt(false);
    #endif
#endif


    if(g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR)
    {
        g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_DEAD;

#if defined (CONFIG_KTD2026)
    // Orange LED 3 seconds pulsing LED pattern
    ktd_led_set_clr(LED_KTD_COLOR_YELLOW, SINGLE_LED_BRIGHTNESS);
    led_select_operation(1, LED_KTD_ON); 
    led_select_operation(2, LED_KTD_BLINK, 1);
    k_sleep(K_SECONDS(5));
    led_select_operation(1, LED_KTD_OFF); 
#endif

#if defined (CONFIG_LP5813)
    k_msleep(2000);
    // Setting Yellow color and blink every second for indicating ship mode
    led_lp5813_select_operation(2, LED_LP_SEL_COLOR, LED_LP_COLOR_YELLOW);
    // Yellow is the combination of red and green so need to enable both LED and blinking of thoes
    led_lp5813_select_operation(4, LED_LP_BLINK, LED_LP_COLOR_RED, LED_TIME_SYNC_INTERVAL, LED_TIME_SYNC_INTERVAL);
    led_lp5813_select_operation(4, LED_LP_BLINK, LED_LP_COLOR_GREEN, LED_TIME_SYNC_INTERVAL, LED_TIME_SYNC_INTERVAL);
    k_sleep(K_SECONDS(5));
    led_lp5813_select_operation(1, LED_LP_OFF);
#endif // CONFIG_LP5813
    }

#if defined (CONFIG_DRV2624)
	// Stop the haptic pattern before going into shipmode
    if(sh_haptic_motor_handler::get_haptic_pattern_status()) {
        if(sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t::OFF, NULL)){
            LOG_ERR("Failed to stop the Haptic pattern");
        }
    }
#endif // CONFIG_DRV2624

#if CONFIG_BT_BONDING_FEATURE
    //Now delete the ble bond information
    sh_ble::erase_bond();
#endif

    uint8_t ret = sh_ble::disable_ble();
    if(ret < 0) {
        LOG_ERR("Failed to disabled the BLE in session thread");
    }
    
    LOG_WRN("Attempting to enter SHIP_MODE");
    ret = ptx30w::enter_ship_mode();
    LOG_INF("SHIP_MODE Status: %d", ret);
}

void ptx30_publish_IPC(struct ptxSystemStatus ptx_info)
{
    static uint8_t bat_cnt = 0;
    static uint8_t Pervious_RFFieldStatus = 0;
    static uint8_t Pervious_RFFieldStatus_LED =3;
    static uint8_t bat_full_cnt = 0;

#if defined (CONFIG_DRV2624)
    haptic_params_t haptic_param;
    memset(&haptic_param, 0, sizeof(haptic_params_t));
#endif // CONFIG_DRV2624
    static uint8_t RFField_on_cnt = 0;
    static uint8_t RFField_off_cnt = 0;

    //conditions for RF Field detected and not detected
    if(Pervious_RFFieldStatus != ptx_info.RfFieldDetected){

        if(ptx_info.RfFieldDetected == 1)
        {
        #if defined (CONFIG_DRV2624)
            // Stop the haptic pattern when sensor is charging
            if(sh_haptic_motor_handler::get_haptic_pattern_status()) {
                if(sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t::OFF, NULL)){
                    LOG_ERR("Failed to stop the Haptic pattern");
                }
            }
        #endif // CONFIG_DRV2624
            
            RFField_off_cnt = 0;
            RFField_on_cnt++;
            if(RFField_on_cnt >= PTX_ON_DEBOUNCE_CNT)
            {
                int ret =0;
                ret = zbus_chan_pub(&ptx30_chan, &ptx_info, K_SECONDS(1));
                LOG_ERR("ptx30 zbus pub ret %d",ret);
                //Release the semaphore to session thread
                k_sem_give(&dl_session_sem);
                Pervious_RFFieldStatus = ptx_info.RfFieldDetected;
                RFField_on_cnt = 0;
            }
            else{
                ptx_info.RfFieldDetected = Pervious_RFFieldStatus;
            }
        } else {
            RFField_on_cnt = 0;
            RFField_off_cnt++;
            if(RFField_off_cnt >= PTX_OFF_DEBOUNCE_CNT)
            {
                int ret =0;
                ret = zbus_chan_pub(&ptx30_chan, &ptx_info, K_SECONDS(1));
                LOG_ERR("ptx30 zbus pub ret %d",ret);
                //Release the semaphore to session thread
                k_sem_give(&dl_session_sem);
                Pervious_RFFieldStatus = ptx_info.RfFieldDetected;
                RFField_off_cnt = 0;

                if( (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD) && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR)
                && (g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_LOWBATTERY) && (g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_DEAD))
                {
                    if(sh_ble::get_state() == sh_ble::ADVERTISING || sh_ble::get_state() == sh_ble::PENDING_TIMESYNC)
                    {       
                        LOG_DBG("BLE CONNECTED having state without timesync:: 0x%02x", g_led_resource_state);
                        #if defined (CONFIG_KTD2026)
                            // Blink blue LED at rate of 1 seconds and brighness at 20%
                            ktd_led_set_clr(LED_KTD_COLOR_BLUE, STARTUP_BRIGHTNESS); 
                            led_select_operation(2, LED_KTD_BRIGHTNESS, LED_SYNC_TIME_BRIGHTNESS);
                            led_select_operation(2, LED_KTD_BLINK, LED_TIME_SYNC_INTERVAL);
                        #endif

                        #if defined (CONFIG_LP5813)
                                led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_BLUE);
                                led_lp5813_select_operation(4, LED_LP_BLINK, LED_LP_COLOR_BLUE, LED_TIME_SYNC_INTERVAL, LED_TIME_SYNC_INTERVAL);
                        #endif // CONFIG_LP5813
                        g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_ADV;
                    }
                }
            }
            else{
                ptx_info.RfFieldDetected = Pervious_RFFieldStatus;
            }
        }

    }
    else{
        RFField_off_cnt = 0;
        RFField_on_cnt = 0;
    }

    if(fault_handler::get_modulefauilreforled() && g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD 
        && g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR)
    {
        LOG_INF("LED %d", __LINE__);
        powermngr::handle_sensor_failure();
    }
    else if(g_led_resource_state == LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD 
        && g_led_resource_state == LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR)
    {
        LOG_INF("LED %d", __LINE__);
        //Do nothing
    }
    else if((sh_ble::get_state() != sh_ble::CONNECTED) && (g_led_resource_state == LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD))
    {
        LOG_INF("LED %d", __LINE__);
        g_led_resource_state = LED_RESOURCE_IDLE;
#if defined (CONFIG_KTD2026)
        led_select_operation(1, LED_KTD_OFF);
#endif

#if defined (CONFIG_LP5813)
        led_lp5813_select_operation(1, LED_LP_OFF);
#endif // CONFIG_LP5813
    }
    else if ((sh_ble::get_state() == sh_ble::CONNECTED) && 
        ((g_led_resource_state == LED_RESOURCE_IDLE) || (g_led_resource_state == LED_RESOURCE_OCCUPIED_BY_ADV)))
    {
        g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_CONN;
        LOG_INF("LED %d", __LINE__);
#if defined (CONFIG_KTD2026)
        led_select_operation(1, LED_KTD_OFF);
#endif

#if defined (CONFIG_LP5813)
        led_lp5813_select_operation(1, LED_LP_OFF);
#endif // CONFIG_LP5813
    }
    else if(Pervious_RFFieldStatus)
    {   //PTX detected
        if((battery_level_led <= 10)
            && (g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_LOWBATTERY || Pervious_RFFieldStatus_LED != Pervious_RFFieldStatus)
            && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD &&
            g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR ))   //low battery and idle
        {
            LOG_INF("LED %d , %d", __LINE__ , g_led_resource_state);
            
            //turn off led
#if defined (CONFIG_KTD2026)
            led_select_operation(1, LED_KTD_OFF);
#endif

#if defined (CONFIG_LP5813)
            led_lp5813_select_operation(1, LED_LP_OFF);
#endif // CONFIG_LP5813
            g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_LOWBATTERY;
        }
        else if(sh_ble::get_state() != sh_ble::CONNECTED && 
                (g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_ADV || Pervious_RFFieldStatus_LED != Pervious_RFFieldStatus)
                && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD &&
                g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR ))   //NOT CONNECTED and idle
        {
            LOG_INF("LED %d", __LINE__);
            //blink blue led
#if defined(CONFIG_BT_CONFIG_BLUE_LED)
#if defined (CONFIG_KTD2026)
            // Blink blue LED at rate of 1 seconds and brighness at 20%
            ktd_led_set_clr(LED_KTD_COLOR_BLUE, STARTUP_BRIGHTNESS); 
            led_select_operation(2, LED_KTD_BRIGHTNESS, LED_SYNC_TIME_BRIGHTNESS);
            led_select_operation(2, LED_KTD_BLINK, LED_TIME_SYNC_INTERVAL);
#endif

#if defined (CONFIG_LP5813)
            led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_BLUE);
            led_lp5813_select_operation(4, LED_LP_BLINK, LED_LP_COLOR_BLUE, LED_TIME_SYNC_INTERVAL, LED_TIME_SYNC_INTERVAL);
#endif // CONFIG_LP5813

            g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_ADV;
#endif  
        }
        else if (sh_ble::get_state() == sh_ble::CONNECTED && (battery_level_led > 10)
        && (( g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD
        && g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_CONN
        && g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR) 
        || (g_led_resource_state == LED_RESOURCE_OCCUPIED_BY_PTX_LOWBATTERY)))
        {
            LOG_INF("LED %d , %d", __LINE__ , g_led_resource_state);
            g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_CONN;
#if defined (CONFIG_KTD2026)
            led_select_operation(1, LED_KTD_OFF);
#endif

#if defined (CONFIG_LP5813)
            led_lp5813_select_operation(1, LED_LP_OFF);
#endif // CONFIG_LP5813
        }
        Pervious_RFFieldStatus_LED = Pervious_RFFieldStatus;
    }
    else
    {   //PTX not detected
        if((battery_level_led <= 10) 
            && (g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_LOWBATTERY || Pervious_RFFieldStatus_LED != Pervious_RFFieldStatus)
            && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD &&
            g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR ))   //low battery and idle
        {
            LOG_INF("LED %d", __LINE__);
            //RED LED BLINK
#if defined (CONFIG_KTD2026)
            // Red LED 3 seconds pulsing LED pattern
            ktd_led_set_clr(LED_KTD_COLOR_RED, STARTUP_BRIGHTNESS);
            led_select_operation(3, LED_KTD_PWM_BLINK, LED_GENERIC_BLINK_INTERVAL, LED_SHIPPING_MODE_INTERVAL);  
#endif

#if defined (CONFIG_LP5813)
            led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_RED);
            led_lp5813_select_operation(4, LED_LP_BLINK, LED_LP_COLOR_RED, LED_GENERIC_BLINK_INTERVAL, LED_SHIPPING_MODE_INTERVAL);
#endif // CONFIG_LP5813
            g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_LOWBATTERY;
        }
        else if(sh_ble::get_state() != sh_ble::CONNECTED && battery_level_led > 10 &&
                (g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_ADV || Pervious_RFFieldStatus_LED != Pervious_RFFieldStatus)
                && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD &&
                g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR ))   //NOT CONNECTED and idle
        {
            //blink blue led
#if defined(CONFIG_BT_CONFIG_BLUE_LED)
            LOG_INF("LED %d", __LINE__);
#if defined (CONFIG_KTD2026)
            // Blink blue LED at rate of 1 seconds and brighness at 20%
            ktd_led_set_clr(LED_KTD_COLOR_BLUE, STARTUP_BRIGHTNESS); 
            led_select_operation(2, LED_KTD_BRIGHTNESS, LED_SYNC_TIME_BRIGHTNESS);
            led_select_operation(2, LED_KTD_BLINK, LED_TIME_SYNC_INTERVAL);
#endif

#if defined (CONFIG_LP5813)
            led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_BLUE);
            led_lp5813_select_operation(4, LED_LP_BLINK, LED_LP_COLOR_BLUE, LED_TIME_SYNC_INTERVAL, LED_TIME_SYNC_INTERVAL);
#endif // CONFIG_LP5813

            g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_ADV;
#endif  
        }
        else if (sh_ble::get_state() == sh_ble::CONNECTED && (battery_level_led > 10) 
                && (( g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD
                && g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_CONN
                && g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR) 
                || (g_led_resource_state == LED_RESOURCE_OCCUPIED_BY_PTX_LOWBATTERY)))
        {
            LOG_INF("LED %d , %d", __LINE__ , g_led_resource_state);
            g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_CONN;
            
#if defined (CONFIG_KTD2026)
            led_select_operation(1, LED_KTD_OFF);
#endif

#if defined (CONFIG_LP5813)
            led_lp5813_select_operation(1, LED_LP_OFF);
#endif // CONFIG_LP5813
        }
        Pervious_RFFieldStatus_LED = Pervious_RFFieldStatus;
    }

    switch(pmic_state){
        case FULLY_CHARGED:
        {
            if( (ptx_info.RfFieldDetected) && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD) && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR)){
                
                if(g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_BAT_FULL){
                    if(ptx_info.VddBat >= MAX_BAT_VOLTAGE) {
                        bat_full_cnt++;
                        if(bat_full_cnt > VALIDATION_CNT){
                            LOG_INF("Bat full detected, turning on green LED");
                        
                        // #if defined (CONFIG_KTD2026)
                        //     led_select_operation(1, LED_KTD_BAT_FULL);
                        // #endif

                        // #if defined (CONFIG_LP5813)
                        //     led_lp5813_select_operation(1, LED_LP_BAT_FULL);
                        // #endif // CONFIG_LP5813
                                
                            // g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_BAT_FULL;

                            pmic_state = FULLY_CHARGED;

                            bat_full_cnt = 0;
                        }
                    }
                    else{
                        bat_full_cnt = 0;
                    }
                }

                if(g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_PULSE_CHARGING){ 
                    if(ptx_info.VddBat < MEDIUM_BAT_THRESHOLD) {
                        LOG_INF("State :: Fully charge , Battery Level is critically low");
                    
                    // #if defined (CONFIG_KTD2026)
                    //     // Green LED 3 seconds pulsing LED pattern
                    //     led_select_operation(1, LED_KTD_CHARGING);
                    // #endif
                    
                    // #if defined (CONFIG_LP5813)
                    //     // Green LED pulsing 
                    //     led_lp5813_select_operation(1, LED_LP_CHARGING);
                    // #endif // CONFIG_LP5813

                        // g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_PULSE_CHARGING;
                      
                        pmic_state = CHARGING;
                      
                        bat_full_cnt = 0;
                    }
                }

                if(g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_CHARGING){ 
                    if( (ptx_info.VddBat >= MEDIUM_BAT_THRESHOLD) && (ptx_info.VddBat < MAX_BAT_VOLTAGE) ){
                        LOG_INF("Battery Level is better now");

                        // Turn on charging LED pattern 1 sec ON 4 sec OFF
                        pmic_state = CHARGING;
                        bat_full_cnt = 0;
                    }
                }
            }
            else if( !ptx_info.RfFieldDetected && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD) && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR) ){
                if(ptx_info.ChargerStatus == BcStatus_ChargingDone) {
                    if(ptx_info.VddBat >= MAX_BAT_VOLTAGE)
                    {
                        bat_full_cnt ++;
                        if(bat_full_cnt >= VALIDATION_CNT+5 ) {
                            if(g_led_resource_state == LED_RESOURCE_OCCUPIED_BY_PTX_BAT_FULL) {
                                LOG_INF("Bat full Counter overflow, turning off green LED");
                            
                            // #if defined (CONFIG_KTD2026)
                            //     led_select_operation(1, LED_KTD_OFF);
                            // #endif

                            // #if defined (CONFIG_LP5813)
                            //     led_lp5813_select_operation(1, LED_LP_OFF);
                            // #endif // CONFIG_LP5813

                                // g_led_resource_state = LED_RESOURCE_IDLE;
                            }
                        }
                        else if(bat_full_cnt >= VALIDATION_CNT) {
                            LOG_INF("Bat full detected, turning on green LED");
                        
                        // #if defined (CONFIG_KTD2026)
                        //     led_select_operation(1, LED_KTD_BAT_FULL);
                        // #endif

                        // #if defined (CONFIG_LP5813)
                        //     led_lp5813_select_operation(1, LED_LP_BAT_FULL);
                        // #endif // CONFIG_LP5813

                            // if(g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_BAT_FULL)
                            //     g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_BAT_FULL;
                        }
                    }  
                    else if(g_led_resource_state == LED_RESOURCE_OCCUPIED_BY_ADV)
                    {
                        //Do nothing
                    }
                    else{
                        LOG_INF("PTX Indicating full charged but voltage is not full");
                    
                    // #if defined (CONFIG_KTD2026)
                    //     led_select_operation(1, LED_KTD_OFF);
                    // #endif

                    // #if defined (CONFIG_LP5813)
                    //     led_lp5813_select_operation(1, LED_LP_OFF);
                    // #endif // CONFIG_LP5813

                        // g_led_resource_state = LED_RESOURCE_IDLE;
                    }
                }
                else if(g_led_resource_state == LED_RESOURCE_OCCUPIED_BY_ADV)
                {
                    //Do nothing
                }
                else{
                // #if defined (CONFIG_KTD2026)
                //     led_select_operation(1, LED_KTD_OFF);
                // #endif

                // #if defined (CONFIG_LP5813)
                //     led_lp5813_select_operation(1, LED_LP_OFF);
                // #endif // CONFIG_LP5813

                    // g_led_resource_state = LED_RESOURCE_IDLE;
                }
            }

            
            if(ptx_info.VddBat < LOW_BATTERY_THRESHOLD){
                bat_cnt++;
                if(bat_cnt >= VALIDATION_CNT){
                    pmic_state = LOW_BATTERY;
                    {
                        LOG_INF("Battery is Low");
                        int ret =0;
                        ret = zbus_chan_pub(&ptx30_chan, &ptx_info, K_SECONDS(1));
                        LOG_ERR("ptx30 zbus pub ret %d",ret);
                        //Release the semaphore to session thread
                        k_sem_give(&dl_session_sem);
                    }

                    bat_cnt = 0;
                }
            }
            else{
                bat_cnt = 0;
            }    
        }
        case CHARGING:
        {
            if( ptx_info.RfFieldDetected && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD) && 
                (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR) ){
                
                if(g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_BAT_FULL){
                    if(ptx_info.VddBat >= MAX_BAT_VOLTAGE) {
                        bat_full_cnt++;
                        if(bat_full_cnt > VALIDATION_CNT){
                            LOG_INF("Bat full detected, turning on green LED");
                        // #if defined (CONFIG_KTD2026)
                        //     led_select_operation(1, LED_KTD_BAT_FULL);
                        // #endif

                        // #if defined (CONFIG_LP5813)
                        //     led_lp5813_select_operation(1, LED_LP_BAT_FULL);
                        // #endif // CONFIG_LP5813
                                
                            // g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_BAT_FULL;

                            pmic_state = FULLY_CHARGED;

                            bat_full_cnt = 0;
                        }
                    }
                    else{
                        bat_full_cnt = 0;
                    }
                }

                if(g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_PULSE_CHARGING){ 
                    if(ptx_info.VddBat < MEDIUM_BAT_THRESHOLD) {
                        LOG_INF("State :: Charging, Battery Level is critically low");
                        // Green LED 3 seconds pulsing LED pattern
                    // #if defined (CONFIG_KTD2026)
                    //     // Green LED 3 seconds pulsing LED pattern
                    //     led_select_operation(1, LED_KTD_CHARGING);
                    // #endif
                    
                    // #if defined (CONFIG_LP5813)
                    //     // Green LED pulsing 
                    //     led_lp5813_select_operation(1, LED_LP_CHARGING);
                    // #endif // CONFIG_LP5813
                        // g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_PULSE_CHARGING;
                        bat_full_cnt = 0;
                    }
                }

                if(g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_CHARGING){ 
                    if( (ptx_info.VddBat >= MEDIUM_BAT_THRESHOLD) && (ptx_info.VddBat < MAX_BAT_VOLTAGE) ){
                        LOG_INF("Battery Level is better now");
                    // #if defined (CONFIG_KTD2026)
                    //     // Turn on charging LED pattern 1 sec ON 4 sec OFF

                    //     ktd_led_set_clr(LED_KTD_COLOR_GREEN, STARTUP_BRIGHTNESS);
                    //     led_select_operation(3, LED_KTD_PWM_BLINK, LED_CHARING_ON_TIME_MS, LED_CHARING_OFF_TIME_MS);
                    // #endif

                    // #if defined (CONFIG_LP5813)
                    //     led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_GREEN);
                    //     led_lp5813_select_operation(4, LED_LP_BLINK, LED_LP_COLOR_GREEN, LED_CHARING_ON_TIME_MS, LED_CHARING_OFF_TIME_MS);
                    // #endif // CONFIG_LP5813

                        // g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_CHARGING;
                        bat_full_cnt = 0;
                    }
                }
            }
            else if( !ptx_info.RfFieldDetected && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD) 
                        && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR) ){
                if(ptx_info.ChargerStatus == BcStatus_ChargingDone) {
                    if(ptx_info.VddBat >= MAX_BAT_VOLTAGE)
                    {
                        bat_full_cnt ++;
                        if(bat_full_cnt >= VALIDATION_CNT) {
                            LOG_INF("Bat full detected, turning on green LED");
                            // #if defined (CONFIG_KTD2026)
                            //     led_select_operation(1, LED_KTD_OFF);
                            // #endif

                            // #if defined (CONFIG_LP5813)
                            //     led_lp5813_select_operation(1, LED_LP_OFF);
                            // #endif // CONFIG_LP5813

                            pmic_state = FULLY_CHARGED;

                            // if(g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_BAT_FULL)
                            //     g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_BAT_FULL;
                        }
                    }  
                    else if(g_led_resource_state == LED_RESOURCE_OCCUPIED_BY_ADV)
                    {
                        //Do nothing
                    }
                    else{
                        LOG_INF("PTX Indicating full charged but voltage is not full");
                        // #if defined (CONFIG_KTD2026)
                        //     led_select_operation(1, LED_KTD_OFF);
                        // #endif

                        // #if defined (CONFIG_LP5813)
                        //     led_lp5813_select_operation(1, LED_LP_OFF);
                        // #endif // CONFIG_LP5813
                        // g_led_resource_state = LED_RESOURCE_IDLE;
                    }
                }
                else if(g_led_resource_state == LED_RESOURCE_OCCUPIED_BY_ADV)
                {
                    //Do nothing
                }
                else{
                    // #if defined (CONFIG_KTD2026)
                    //     led_select_operation(1, LED_KTD_OFF);
                    // #endif

                    // #if defined (CONFIG_LP5813)
                    //     led_lp5813_select_operation(1, LED_LP_OFF);
                    // #endif // CONFIG_LP5813
                    // g_led_resource_state = LED_RESOURCE_IDLE;
                }
            }

            
            if(ptx_info.VddBat < LOW_BATTERY_THRESHOLD){
                bat_cnt++;
                if(bat_cnt >= VALIDATION_CNT){
                    pmic_state = LOW_BATTERY;
                    {
                        LOG_INF("Battery is Low");
                        int ret =0;
                        ret = zbus_chan_pub(&ptx30_chan, &ptx_info, K_SECONDS(1));
                        LOG_ERR("ptx30 zbus pub ret %d",ret);
                        //Release the semaphore to session thread
                        k_sem_give(&dl_session_sem);
                    }

                    bat_cnt = 0;
                }
            }
            else{
                bat_cnt = 0;
            }        

            break;
        }
        case LOW_BATTERY:
        {
            if(ptx_info.RfFieldDetected && (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD) && 
                (g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_CHARGING) && 
                (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR)){
                // g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_CHARGING; 

                // LOG_INF("Turning pulsing GREEN LED for charging");
            
            //As of now we have turn off the LED indication for charging cases. 
            //TODO: Add LED profile in future for differnt customer

            // #if defined (CONFIG_KTD2026)
            //     // Green LED 3 seconds pulsing LED pattern
            //     led_select_operation(1, LED_KTD_CHARGING);
            // #endif
            
            // #if defined (CONFIG_LP5813)
            //     // Green LED pulsing 
            //     led_lp5813_select_operation(1, LED_LP_CHARGING);
            // #endif // CONFIG_LP5813

                // #if defined (CONFIG_KTD2026)
                // led_select_operation(1, LED_KTD_OFF);
                // #endif

                // #if defined (CONFIG_LP5813)
                //         led_lp5813_select_operation(1, LED_LP_OFF);
                // #endif // CONFIG_LP5813

            } 
            else if( (!ptx_info.RfFieldDetected) && (g_led_resource_state != LED_RESOURCE_OCCUPIED_BY_PTX_LOWBATTERY) && 
                        (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD) &&
                        (g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR) ){

                // g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_PTX_LOWBATTERY; 

                LOG_INF("Turning RED Warning LED");

            // #if defined (CONFIG_KTD2026)
            //     // Red LED 3 seconds pulsing LED pattern
            //     ktd_led_set_clr(LED_KTD_COLOR_RED, STARTUP_BRIGHTNESS);
            //     led_select_operation(3, LED_KTD_PWM_BLINK, LED_GENERIC_BLINK_INTERVAL, LED_SHIPPING_MODE_INTERVAL);  
            // #endif

            // #if defined (CONFIG_LP5813)
            //     led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_RED);
            //     led_lp5813_select_operation(4, LED_LP_BLINK, LED_LP_COLOR_RED, LED_GENERIC_BLINK_INTERVAL, LED_SHIPPING_MODE_INTERVAL);
            // #endif // CONFIG_LP5813

            }

            if(ptx_info.VddBat > LOW_BATTERY_RECOVERY){
                bat_cnt++;
                if(bat_cnt >= VALIDATION_CNT){
                    pmic_state = CHARGING;
                    {
                        LOG_INF("Battery above threshold");
                        int ret =0;
                        ret = zbus_chan_pub(&ptx30_chan, &ptx_info, K_SECONDS(1));
                        LOG_ERR("ptx30 zbus pub ret %d",ret);
                        //Release the semaphore to session thread
                        k_sem_give(&dl_session_sem);
                    }

                    bat_cnt = 0;
                }
            }
            else if(ptx_info.VddBat < DEAD_BATTERY_THRESHOLD){
                bat_cnt++;
                if(bat_cnt >= VALIDATION_CNT){
                    pmic_state = DEAD_BATTERY;
                    {
                        LOG_INF("Dead Battery Detected");
                        int ret =0;
                        ret = zbus_chan_pub(&ptx30_chan, &ptx_info, K_SECONDS(1));
                        LOG_ERR("ptx30 zbus pub ret %d",ret);
                        //Release the semaphore to session thread
                        k_sem_give(&dl_session_sem);
                    } 

                    bat_cnt = 0;

                    if( !ptx_info.RfFieldDetected ){
                        ptx30_shipmode_pattern();
                    }

                    
                }
            }
            else{
                bat_cnt = 0;
            }
            break;
        }
        case DEAD_BATTERY:
        {
            if( !ptx_info.RfFieldDetected ){
                ptx30_shipmode_pattern();
            }
            if(ptx_info.VddBat > DEAD_BATTERY_RECOVERY){
                bat_cnt++;
                if(bat_cnt >= VALIDATION_CNT){
                    pmic_state = LOW_BATTERY;
                    {
                        LOG_INF("Battery goes above threshold");
                        int ret =0;
                        ret = zbus_chan_pub(&ptx30_chan, &ptx_info, K_SECONDS(1));
                        LOG_ERR("ptx30 zbus pub ret %d",ret);
                        //Release the semaphore to session thread
                        k_sem_give(&dl_session_sem);
                    }
                    bat_cnt = 0;
                }
            }
            else{
                bat_cnt = 0;
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

void ptx30_publish_ble(struct ptxSystemStatus ptx_info, uint8_t *batt_lvl)
{
    uint8_t battery_level = *batt_lvl;
    bool is_charging;
    static uint8_t bat_cnt = 0;
    static bool last_charging_status = false;
    static uint8_t last_published_battery_level = 0;
    static uint8_t previous_battery_level = 0;
    static uint8_t charging_cnt = 0;
    static uint8_t not_charging_cnt = 0;
    sh_ble::adv_data_t adv_batt_data;

    // Determine if charging
    is_charging = ptx_info.RfFieldDetected;
    
    // Convert the battery level to an integer percentage
    // battery_level = powermngr::determine_power_level(ptx_info.VddBat);

    LOG_DBG("Battery level %d, battery voltage %d",battery_level, ptx_info.VddBat);

    if(last_published_battery_level == 0){
        LOG_INF("updating battery level");
        sh_bas_set_battery_level(battery_level | (is_charging << 7));

        adv_batt_data.batt.batt.charging_status = is_charging;
        adv_batt_data.batt.batt.percentage = battery_level;
        sh_ble::update_advertising_data(sh_ble::BLE_ADV_BATT_LEVEL_STATUS, adv_batt_data);
        
        last_published_battery_level = battery_level;
        battery_level_led = last_published_battery_level;
        last_charging_status = is_charging;
    }

    // Charging and battery level increases, publish update to BLE
    if (is_charging != last_charging_status) {        
        if(is_charging == 1)
        {
            not_charging_cnt = 0;
            charging_cnt++;
            if(charging_cnt >= PTX_ON_DEBOUNCE_CNT) {
                sh_bas_set_battery_level(battery_level | (is_charging << 7));

                adv_batt_data.batt.batt.charging_status = is_charging;
                adv_batt_data.batt.batt.percentage = battery_level;
                sh_ble::update_advertising_data(sh_ble::BLE_ADV_BATT_LEVEL_STATUS, adv_batt_data);

                last_charging_status = is_charging;
                charging_cnt = 0;
            } else{
                is_charging = last_charging_status;
            }
        } else {
            charging_cnt = 0;
            not_charging_cnt++;
            if(not_charging_cnt >= PTX_OFF_DEBOUNCE_CNT) {
                sh_bas_set_battery_level(battery_level | (is_charging << 7));

                adv_batt_data.batt.batt.charging_status = is_charging;
                adv_batt_data.batt.batt.percentage = battery_level;
                sh_ble::update_advertising_data(sh_ble::BLE_ADV_BATT_LEVEL_STATUS, adv_batt_data);

                last_charging_status = is_charging;
                not_charging_cnt = 0;
            } else{
                is_charging = last_charging_status;
            }
        }
    }
    else{
        charging_cnt = 0;
        not_charging_cnt = 0;
    }

    // Not charging and battery level drops, publish update to BLE
    // also check is same value has been received since last 5 times.
    if ( (battery_level != last_published_battery_level ) && (previous_battery_level == battery_level) ) {
            bat_cnt++;
            if(bat_cnt >= VALIDATION_CNT){
                LOG_INF("updating battery level");
                sh_bas_set_battery_level(battery_level | (is_charging << 7));

                adv_batt_data.batt.batt.charging_status = is_charging;
                adv_batt_data.batt.batt.percentage = battery_level;
                sh_ble::update_advertising_data(sh_ble::BLE_ADV_BATT_LEVEL_STATUS, adv_batt_data);

                // Mask out the 8th bit since it is the charger status
                last_published_battery_level = battery_level;
                battery_level_led = last_published_battery_level;

                bat_cnt = 0;
            }
    }
    else{
        bat_cnt = 0;
    }

    previous_battery_level = battery_level;

}

/**
* @brief Initialize the PTX30 and start the zbus monitor thread
*/
uint16_t powermngr::get_battery_voltage(void)
{
    struct ptxSystemStatus ptx_status;
    ptx30w::get_system_status(&ptx_status);
    return ptx_status.VddBat;
}

/** 
 * @brief Handling of BLE LED indication
 *
 * This function reads the BLE state and update the LED accoding to the 
 * current operation. 
 */
void handle_ble_led()
{    
    if((sh_ble::get_state() != sh_ble::CONNECTED) && (g_led_resource_state == LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD))
    {
        LOG_DBG("BLE disconnected and LED previous led state: %d ", g_led_resource_state);
        g_led_resource_state = LED_RESOURCE_IDLE;
#if defined (CONFIG_KTD2026)
        led_select_operation(1, LED_KTD_OFF);
#endif

#if defined (CONFIG_LP5813)
        led_lp5813_select_operation(1, LED_LP_OFF);
#endif // CONFIG_LP5813
    }

    switch (sh_ble::get_state()) {
    
	case sh_ble::NOT_INITIALIZED:
	case sh_ble::IDLE:
        
		break;
    
    //As per the Maroho the LED should be blinks in blue not green please create a profile in future for
    //differnt customer
	
	// case sh_ble::ADVERTISING:
          
    //     if((g_led_resource_state == LED_RESOURCE_IDLE) || (g_led_resource_state == LED_RESOURCE_OCCUPIED_BY_CONN))
    //     {
    //         g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_ADV;
    //         LOG_DBG("BLE ADVERTISING having state :: 0x%02x", g_led_resource_state);
    //     #if defined (CONFIG_KTD2026)
    //         // Green LED Blink every 3 seconds
    //         ktd_led_set_clr(LED_KTD_COLOR_GREEN, STARTUP_BRIGHTNESS);
    //         led_select_operation(3, LED_KTD_PWM_BLINK, LED_GENERIC_BLINK_INTERVAL, LED_LOW_BAT_BLINK_INTERVAL);
    //     #endif

    //     #if defined (CONFIG_LP5813)
    //             led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_GREEN);
    //             led_lp5813_select_operation(4, LED_LP_BLINK, LED_LP_COLOR_GREEN, LED_GENERIC_BLINK_INTERVAL, LED_LOW_BAT_BLINK_INTERVAL);
    //     #endif // CONFIG_LP5813
    //     }
    //     break;

    case sh_ble::ADVERTISING:
    case sh_ble::PENDING_TIMESYNC:
#if defined(CONFIG_BT_CONFIG_BLUE_LED)
     if (((g_led_resource_state == LED_RESOURCE_IDLE) || (g_led_resource_state == LED_RESOURCE_OCCUPIED_BY_CONN)) )
        {
            LOG_DBG("BLE CONNECTED having state without timesync:: 0x%02x", g_led_resource_state);
        #if defined (CONFIG_KTD2026)
            // Blink blue LED at rate of 1 seconds and brighness at 20%
            ktd_led_set_clr(LED_KTD_COLOR_BLUE, STARTUP_BRIGHTNESS); 
            led_select_operation(2, LED_KTD_BRIGHTNESS, LED_SYNC_TIME_BRIGHTNESS);
            led_select_operation(2, LED_KTD_BLINK, LED_TIME_SYNC_INTERVAL);
        #endif

        #if defined (CONFIG_LP5813)
                led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_BLUE);
                led_lp5813_select_operation(4, LED_LP_BLINK, LED_LP_COLOR_BLUE, LED_TIME_SYNC_INTERVAL, LED_TIME_SYNC_INTERVAL);
        #endif // CONFIG_LP5813

            g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_ADV;
        }
#endif        
        break;


    case sh_ble::CONNECTED:
           
        if((g_led_resource_state == LED_RESOURCE_IDLE) || (g_led_resource_state == LED_RESOURCE_OCCUPIED_BY_ADV))
        {
            g_led_resource_state = LED_RESOURCE_OCCUPIED_BY_CONN;
            LOG_DBG("BLE CONNECTED having state with time sync:: 0x%02x", g_led_resource_state);
        #if defined (CONFIG_KTD2026)
            led_select_operation(1, LED_KTD_OFF);
        #endif

        #if defined (CONFIG_LP5813)
            led_lp5813_select_operation(1, LED_LP_OFF);
        #endif // CONFIG_LP5813
        }
        break;
    }
}

static int32_t abort_session_activity(void)
{
    int32_t ret;

    //Send can

    //Send stop session command to session thread
    session_msg_t msg;

    msg.cmd_type = SESSION_DOWNLOAD_CANCEL;
    msg.data_len = 0;

    ret = zbus_chan_pub(&ble_session_cmd_chan, &msg, K_NO_WAIT);
    if(ret != 0) {
        LOG_ERR("Failed to publish session data = %d", ret);
    }
#if defined(CONFIG_STORAGE_APP)
    //Publish the semaphore
    k_sem_give(&dl_session_sem);
#endif

    // Wait to allow session thread to cancel download.
    k_sleep(K_SECONDS(1));

    msg.cmd_type = SESSION_STOP;
    msg.data_len = 0;

    ret = zbus_chan_pub(&ble_session_cmd_chan, &msg, K_NO_WAIT);
    if(ret != 0) {
        LOG_ERR("Failed to publish session data = %d", ret);
    }
#if defined(CONFIG_STORAGE_APP)
    //Publish the semaphore
    k_sem_give(&dl_session_sem);
#endif

    // Wait to allow session thread to stop the activity.
    k_sleep(K_SECONDS(1));

    return ret;
}

/**
 * @brief Handling of BLE service commands
 * 
 * This function reads teh BLE service commands and perform actions based
 * on that.
 */
static void handle_ble_service_cmd(void)
{
    int32_t ret;
    const struct zbus_channel *chan;
    cmd_msg_t service_cmd_msg;
    uint8_t haptic_status = false;

#if defined (CONFIG_DRV2624)
    haptic_params_t haptic_param;
    memset(&haptic_param, 0, sizeof(haptic_params_t));
#endif // CONFIG_DRV2624

        if(!zbus_sub_wait_msg(&service_cmd_sub, &chan, &service_cmd_msg, K_NO_WAIT)) {

            switch(service_cmd_msg.cmd_type)
            {
                case CMD_SHIP_MODE:
                    // Abort all the activity going on in the session thread.
                    ret = abort_session_activity();
                    if(ret != 0) {
                        LOG_ERR("Failed to abort session activity = %d", ret);
                        break;
                    }
                    // Enter into ship mode
                    // LOG_WRN("Attempting to enter SHIP_MODE");
                    // ret = ptx30w::enter_ship_mode();

                    // LOG_INF("SHIP_MODE Status: %d", ret);

                    ptx30_shipmode_pattern();
                    break;
                    
                case CMD_LED_COMMANDS:
                    if(g_led_resource_state != LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR)
                    {
                        switch(service_cmd_msg.data[0])
                        {
                        case LED_CMD_GREEN_ON:
                            LOG_INF("Turning ON Green LED");

                        #if defined(CONFIG_KTD2026)
                            ktd_led_set_clr(LED_KTD_COLOR_GREEN, STARTUP_BRIGHTNESS);
                            led_select_operation(1, LED_KTD_ON);
                        #endif
                        
                        #if defined (CONFIG_LP5813)
                            led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_GREEN);
                        #endif // CONFIG_LP5813
                        
                            g_led_resource_state = LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD;
                            break;
                        case LED_CMD_BLUE_ON:
                            LOG_INF("Turning ON Blue LED");

                        #if defined(CONFIG_KTD2026)
                            ktd_led_set_clr(LED_KTD_COLOR_BLUE, STARTUP_BRIGHTNESS);
                            led_select_operation(1, LED_KTD_ON);
                        #endif 

                        #if defined (CONFIG_LP5813)
                                led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_BLUE);
                        #endif // CONFIG_LP5813

                            g_led_resource_state = LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD;
                            break;
                        case LED_CMD_RED_ON:
                            LOG_INF("Turning ON Red LED");
                        
                        #if defined(CONFIG_KTD2026)
                            ktd_led_set_clr(LED_KTD_COLOR_RED, STARTUP_BRIGHTNESS);
                            led_select_operation(1, LED_KTD_ON);
                        #endif

                        #if defined (CONFIG_LP5813)
                                led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_RED);
                        #endif // CONFIG_LP5813

                            g_led_resource_state = LED_RESOURCE_OCCUPUED_BY_SERVICE_CMD;
                            break;
                        case LED_CMD_GREEN_OFF:
                        case LED_CMD_BLUE_OFF:
                        case LED_CMD_RED_OFF:
                            LOG_INF("Turning OFF LED");
                        #if defined(CONFIG_KTD2026)
                            led_select_operation(1, LED_KTD_OFF);
                        #endif

                        #if defined (CONFIG_LP5813)
                                led_lp5813_select_operation(1, LED_LP_OFF);
                        #endif // CONFIG_LP5813
                            g_led_resource_state = LED_RESOURCE_IDLE;
                            break;
                        default:
                            LOG_ERR("Invalid LED Command");
                            break;
                        }
                        break;
                    }
                    break;
                    
                case CMD_HAPTIC_COMMANDS:
#if defined(CONFIG_DRV2624)
                    haptic_params_t haptic_param;
                    memset(&haptic_param, 0, sizeof(haptic_params_t));
                    if(service_cmd_msg.data_len != CMD_HAPTIC_COMMAND_LEN) {
                        LOG_ERR("Invalid haptic commands command");
                    }
                    else {
                        memcpy(&haptic_param, &service_cmd_msg.data, service_cmd_msg.data_len);
                        LOG_INF("Haptic intensity = %d", haptic_param.intensity);
                        LOG_INF("Haptic index = %d", haptic_param.index);
                        LOG_INF("Haptic pulse width = %dms", haptic_param.pulse_width);
                        LOG_INF("Haptic interval = %ds", haptic_param.interval);
                        LOG_INF("Haptic duration = %ds", haptic_param.duration);
                        ret = sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t::CUSTOM, &haptic_param);
                        if(ret) {
                            LOG_ERR("Failed to set the Haptic pattern (err = %d)", ret);
                        }
                    }
#else
                    LOG_INF("Haptic Driver DRV2624 is not enabled.");
#endif /* CONFIG_DRV2624 */
                    break;
                case CMD_READ_HAPTIC_STATUS:
#if defined(CONFIG_DRV2624)
                    uint8_t haptic_status_buff[2];
                    haptic_status = sh_haptic_motor_handler::get_haptic_status();
                    LOG_INF("Haptic status = %d", haptic_status);
                    haptic_status_buff[0] = SH_DATA_TYPE_HAPTIC_STATUS;
                    haptic_status_buff[1] = haptic_status;
                    ret = sh_ble_pd_meas_send(haptic_status_buff, 2);
                    if(ret < 0) {
                        LOG_ERR("Failed send the haptic status on BLE");
                    }
#else
                    LOG_INF("Haptic Driver DRV2624 is not enabled.");
#endif
                    break;
                case CMD_DELETE_FDS_MODULE_ERROR: 

                    LOG_ERR("Module Reset Command Received");
                    ret = fault_handler::reset_modulestatus();
                    if(ret < 0) {
                        LOG_ERR("Failed to reset module");
                    }

                    break;
                default:
                    break;
            }
        }
}

/**
* @brief Get and publish the ptx30 system status every second to the zbus
*/
void powermngr_thread_func(void *, void *, void*)
{
    struct ptxSystemStatus ptx_status;
    ptxStatus_t status = ptxStatus_InternalError;
    uint8_t tdc_buffer[31];
    uint8_t battery_level = 0;
    const uint8_t tdcTimeout = 35U;
    const uint8_t getSysStsDataCmd = 0xAA; // command sent by the poller requesting system status data
    static bool is_tdc_sent = false;
    struct ptx30_data *data = (struct ptx30_data *)ptx30->data;
    static uint16_t prvs_vdbat = 0;
    static uint8_t charger_reset_count = 0;
    uint8_t delay_counter = 0;
    static uint8_t vdbat_range_counter = 0;
    static uint32_t vdbat_accu = 0;
    static bool is_first_vdbat = false;


    // determine_power_level_test();

    while (1) {

#ifdef CONFIG_WATCHDOG
    /* Watchdog is creating issue while performing DFU so disable for this thread */
    #if 0
        if(wdt_ptx_channel_id >= 0) {
            task_wdt_fed_from_thread(wdt_ptx_channel_id);
        }
    #endif 
#endif 
        if( k_sem_take(&data->gpio_sem, K_MSEC(PTX30_SEM_TIMEOUT_MSEC) ) == 0 ) {
        // if( k_sem_take(&data->gpio_sem, K_SECONDS(PTX30_SEM_TIMEOUT_SEC) ) == 0 ) {
            LOG_INF("Semaphore received");
            delay_counter = PTX30_SEM_COUNTER_CHECK;

            (void) ptx30w::handle_pmic_interrupt(false);

            ptx30w::get_system_status(&ptx_status);

            //Reset the integrated battery charger in case of battery full indication
            if(ptx_status.ChargerStatus == BcStatus_ChargingDone)
            {
                charger_reset_count++;
                if(charger_reset_count >= 10)
                {
                    ptx30w::restart_charger();
                    charger_reset_count = 0;
                }
            } else {
                charger_reset_count = 0;
            }
            

            if (ptx_status.VddBat == 2400) {
                if(ptx_status.VddC < 4300) {
                    ptx_status.VddBat = ptx_status.VddC;
                } else {
                    ptx_status.VddBat = 4250;
                }
            }

			uint8_t tdc_buffer_len = sizeof(tdc_buffer);
			memset(tdc_buffer, 0U, tdc_buffer_len);
		    // If the IRQ was high, it means that poller sent a message. We must read it and reply 
			status = ptx30w_TDC_Read(tdc_buffer, &tdc_buffer_len, tdcTimeout); // Use normal TDC as we send packets <=31 bytes . HLP shall beused if the packet length is >31 bytes
            LOG_INF("Reading TDC status :%d, buffer length : %d", status, tdc_buffer_len);
            /*Check if there is a request, at least 1 byte*/
            if (ptxStatus_Success == status && tdc_buffer_len){
                switch(tdc_buffer[0])
                {
                    case getSysStsDataCmd: // if system status was requested, send the last data that was read previously
                        // you can read data also here but this will extend the communication cycle 
                        LOG_INF("preparing TDC");
                        tdc_buffer_len = 0;
                        tdc_buffer[tdc_buffer_len++] = SH_TDC_MSG_VERSION;
                        tdc_buffer[tdc_buffer_len++] = battery_level;
                        tdc_buffer[tdc_buffer_len++] = (ptx_status.VddC & 0xFF);
                        tdc_buffer[tdc_buffer_len++] = ((ptx_status.VddC >> 8) & 0xFF);
                        tdc_buffer[tdc_buffer_len++] = (ptx_status.VddBat & 0xFF);
                        tdc_buffer[tdc_buffer_len++] = ((ptx_status.VddBat >> 8) & 0xFF);
                        tdc_buffer[tdc_buffer_len++] = (uint8_t)ptx_status.RfFieldDetected;
                        tdc_buffer[tdc_buffer_len++] = ptx_status.NtcStatus;
                        tdc_buffer[tdc_buffer_len++] = ptx_status.WlcpStatus;
                        tdc_buffer[tdc_buffer_len++] = ptx_status.ChargerStatus; // Charger status can tell you if battry is full or not. 
                        tdc_buffer[tdc_buffer_len++] = (uint8_t)pmic_state;
                    break;
                    
                    default: // Implement other commands here. Currently reply to the poller with 0 
                        memset(tdc_buffer, 0U, tdc_buffer_len);
                    break;
                }
                /*Write back data to the poller*/
                tdc_buffer_len = sizeof(tdc_buffer);
                status = ptx30w_TDC_Write(tdc_buffer, tdc_buffer_len, tdcTimeout);
                LOG_INF("TDC sent");
                is_tdc_sent = true;
			}

            (void) ptx30w::handle_pmic_interrupt(true);
        }
        else{
            delay_counter++;
        }
        handle_ble_service_cmd();
        
        if(delay_counter < PTX30_SEM_COUNTER_CHECK){
            continue;
        }
        else{
            delay_counter = 0;
        }

        if(is_tdc_sent) {
            is_tdc_sent = false;
        } else {
            ptx30w::get_system_status(&ptx_status);
        }

        LOG_DBG("ChargerEnabled     = %d",ptx_status.ChargerEnabled);
        LOG_INF("RfFieldDetected    = %d",ptx_status.RfFieldDetected);
        LOG_DBG("Error              = %d",ptx_status.Error);
        LOG_INF("VddBat             = %d",ptx_status.VddBat);
        LOG_DBG("VddC               = %d",ptx_status.VddC);
        LOG_DBG("NtcStatus          = %d",ptx_status.NtcStatus);
        LOG_DBG("WlcpStatus         = %d",ptx_status.WlcpStatus);
        LOG_DBG("ChargerStatus      = %d",ptx_status.ChargerStatus);

        if(ptx_status.VddBat < 4300)        // validate if values are not invalid
        {
            if ( ptx_status.VddBat == 2400 ) {
                ptx_status.VddBat = prvs_vdbat;
            } else if ((ptx_status.VddBat >= (prvs_vdbat - 85)) && (ptx_status.VddBat <= (prvs_vdbat + 85))) {
                vdbat_accu += ptx_status.VddBat;
                vdbat_range_counter++;

                if (vdbat_range_counter >= 10) {
                    prvs_vdbat = vdbat_accu / 10;
                    vdbat_range_counter = 0;  
                    vdbat_accu = 0;
                }
            }else if ((ptx_status.VddBat != 2400) && (!is_first_vdbat)) {
                prvs_vdbat = ptx_status.VddBat;
                is_first_vdbat = true;
            } else {
                vdbat_range_counter = 0;  
                vdbat_accu = 0;
                ptx_status.VddBat = prvs_vdbat;
            }
            
            // This is calculated here to constantly get and publish the battery percentage
            // instead of calculating prior to TDC. This is because the TDC is only published 
            // when sensor is on the charger.
            battery_level = powermngr::determine_power_level(ptx_status.VddBat);

            ptx30_publish_IPC(ptx_status);

            ptx30_publish_ble(ptx_status, &battery_level);

            // if(!ptx_status.RfFieldDetected){
            // handle_ble_led();
            // }

            // NOTE: One of the scenarios that are unaccounted for is when the 
            // battery is being charged but the voltage is still dropping.
            // This could happen if more power is being consumed than is being
            // charged. It's possible this scenario may never happen but in the 
            // event it does, it wouldn't be handled.

        }
        k_msleep(1);
    }
}

int powermngr::handle_sensor_failure(void)
{
    int ret = 0;
    LOG_INF("Turning ON Red LED");
    g_led_resource_state = LED_RESOURCE_OCCUPUED_BY_SENSOR_FAILUR;
                        
    #if defined (CONFIG_KTD2026)
        // Red LED 3 seconds pulsing LED pattern
        ktd_led_set_clr(LED_KTD_COLOR_RED, STARTUP_BRIGHTNESS);
        led_select_operation(3, LED_KTD_PWM_BLINK, LED_GENERIC_BLINK_INTERVAL, LED_SHIPPING_MODE_INTERVAL);  
    #endif

    #if defined (CONFIG_LP5813)
        led_lp5813_select_operation(1, LED_LP_ON, LED_LP_COLOR_RED);
        led_lp5813_select_operation(4, LED_LP_BLINK, LED_LP_COLOR_RED, LED_GENERIC_BLINK_INTERVAL, LED_SHIPPING_MODE_INTERVAL);
    #endif // CONFIG_LP5813
    return ret;
}

int powermngr::reset_sensor_failure(void)
{
    int ret = 0;
    LOG_INF("Resetting LED for sensor Failure");
    g_led_resource_state = LED_RESOURCE_IDLE;
                        
    #if defined(CONFIG_KTD2026)
        ret = led_select_operation(1, LED_KTD_OFF);
    #endif

    #if defined (CONFIG_LP5813)
        ret = led_lp5813_select_operation(1, LED_LP_OFF);
    #endif // CONFIG_LP5813
    return ret;
}

/**
* @brief Initialize the PTX30 and start the zbus monitor thread
*/
int powermngr::thread_init()
{
    int32_t ret = 0;

    //Initialize PTX30 to check if responsive
    ret = ptx30w::init();
    if (ret < 0) {
        LOG_ERR("Failed to initalize PTX30 with error %d", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to initalize PTX30 with error %d", ret);
#endif          
        fault_handler::update_fault(PMIC_FAULT);
    }

#if defined (CONFIG_KTD2026)
    /* KTD2026 LED driver initialization */
    ret = led_driver_init();
    if (ret < 0) {
        LOG_ERR("LED Driver init failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"LED Driver init failed (err %d)\n", ret);
#endif          
        fault_handler::update_fault(LED_FAULT);
    }

    /* LED start up sequence */
    ret = led_select_operation(1, LED_KTD_START_DEV);
    if (ret < 0) {
        LOG_ERR("LED Driver init failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"LED Driver init failed (err %d)\n", ret);
#endif          
        fault_handler::update_fault(LED_FAULT);
    }
#endif

#if defined (CONFIG_LP5813)
    ret = led_lp5813_driver_init();
    if (ret < 0) {
        LOG_ERR("LED Driver LP5813 init failed (err %d)\n", ret);
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"LED Driver LP5813 init failed (err %d)\n", ret);
#endif          
        fault_handler::update_fault(LED_FAULT);
    }

    /* LED start up sequence */
    led_lp5813_select_operation(1, LED_LP_START_DEV);

    /* For testing purpose only */
#if 0
    led_lp5813_suspend_power();
#endif // Enable this section to suspend the LED driver power

#endif // CONFIG_LP5813

#ifdef CONFIG_WATCHDOG
    /* Watchdog is creating issue while performing DFU so disable for this thread */
    #if 0
        // Turn on watchdog channel 
        start_stop_wdt(true);
    #endif 
#endif


    // Configure charging parameters
    ptxChargingParams charging_params;
    charging_params.ChargeCurrent = 50;
    charging_params.TerminationVoltage = ptx30wVTerm_4V24;//ptx30wVTerm_4V18;//ptx30wVTerm_3V70;
    charging_params.TrickleVoltage = ptx30wVTrickle_3V0;
    charging_params.RechargeVoltage = ptx30wVRecharge_3V87;
    charging_params.EnableCharging = true;

    ret = ptx30w::set_charging_parameters(charging_params);
    if (ret) {
        LOG_ERR("Failed to configure PTX30 charging parameters");
#if defined(CONFIG_MEMFAULT) 
        // MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log,"Failed to configure PTX30 charging parameters");
#endif          
        fault_handler::update_fault(PMIC_FAULT);
    }

    // Add ZBUS observer to Commands channel
    ret = zbus_chan_add_obs(&cmd_chan, &service_cmd_sub, K_MSEC(200));
    if(ret) {
        LOG_ERR("Failed to add thread as ZBUS observer to command chan (err: %d)", ret);
    }
    static k_tid_t power_manager_tid;
    // Start Zbus monitor thread if everything went well
    power_manager_tid = k_thread_create(&powermngr_thread,
                          powermngr_stack,
                          K_THREAD_STACK_SIZEOF(powermngr_stack),
                          powermngr_thread_func,
                          NULL, NULL, NULL,
                          7,
                          0,
                          K_NO_WAIT);

    k_thread_name_set(power_manager_tid, "power_manager");

    return ret;
}


#endif
