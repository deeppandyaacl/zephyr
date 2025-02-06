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

#ifndef __PTX30_THREAD_H__
#define __PTX30_THREAD_H__

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <sh/sh_ptx30.h>
#include "ptx30w.h"

ZBUS_CHAN_DECLARE(ptx30_chan);

#define SH_TDC_MSG_VERSION          1

#define MAX_BAT_VOLTAGE             4150
#define MIN_BAT_VOLTAGE             3300
#define BATTERY_RANGE               (MAX_BAT_VOLTAGE - MIN_BAT_VOLTAGE)             

#define LOW_BATTERY_THRESHOLD       ( MIN_BAT_VOLTAGE + (BATTERY_RANGE * 0.30) )    //3555
#define LOW_BATTERY_RECOVERY        ( MIN_BAT_VOLTAGE + (BATTERY_RANGE * 0.35) )    //3598
#define DEAD_BATTERY_THRESHOLD      ( MIN_BAT_VOLTAGE )                             //3000
#define DEAD_BATTERY_RECOVERY       ( MIN_BAT_VOLTAGE + (BATTERY_RANGE * 0.10) )    //3385
#define MEDIUM_BAT_THRESHOLD        ( MIN_BAT_VOLTAGE + (BATTERY_RANGE * 0.50) )    //3725

#if defined (CONFIG_KTD2026)
#define LED_CHARING_ON_TIME_MS              1000    // 1 Second on time when sensor is charging 
#define LED_CHARING_OFF_TIME_MS             4000    // 4 Second off time when sensor is charging 
#define LED_GENERIC_BLINK_INTERVAL          10      // This is the on time of LED 
#define LED_LOW_BAT_BLINK_INTERVAL          3000    // Blink LED every 3 second
#define LED_TIME_SYNC_INTERVAL              1000    // Blink blue LED every 1 second
#define LED_SHIPPING_MODE_INTERVAL          1000    // Blink LED every 1 second
#else
#define LED_CHARING_ON_TIME_MS              0x06    // 1.07s Second on time when sensor is charging for LP5813 LED 
#define LED_CHARING_OFF_TIME_MS             0x0B    // 4.02s Second off time when sensor is charging for LP5813 LED 
#define LED_GENERIC_BLINK_INTERVAL          0x02    // 0.180s This is the on time of LED for Low battery and BLE adv
#define LED_LOW_BAT_BLINK_INTERVAL          0x0A    // 3.04s This is the off time of LED for Low battery and BLE adv
#define LED_TIME_SYNC_INTERVAL              0x06    // Blink blue LED every 1 second
#define LED_SHIPPING_MODE_INTERVAL          0x0A    // Blink LED every 3 second
#endif


#define LED_LOW_BAT_CHARG_STEPS             60      // While having critical low battery and sensor put on charger, it shoud fade for 3 second on and off interval
#define CMD_HAPTIC_COMMAND_LEN      ( 8 )

typedef enum {
    LED_CMD_GREEN_ON = 1,
    LED_CMD_GREEN_OFF,
    LED_CMD_BLUE_ON,
    LED_CMD_BLUE_OFF,
    LED_CMD_RED_ON,
    LED_CMD_RED_OFF
}cmd_led_command_t;

#ifdef __cplusplus
namespace powermngr {

    /**
     * @brief Initializes the power manager thread 
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int thread_init();

    /**
     * @brief returns the battery percentage of sensor based on the provided voltage
     * @param voltage : voltage received from get_battery_voltage.
     * @return uint8_t Success indicates 0 and failure is non-zero.
     */
    uint8_t determine_power_level(uint16_t voltage);

    /**
     * @brief Reads the battery voltage of sensoe
     *
     * @return uint16_t battery voltage received from PMIC.
     */
    uint16_t get_battery_voltage();

    /**
     * @brief Enables the sensor failure led operation
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int handle_sensor_failure(void);

    /**
     * @brief reset the sensor failure led status 
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int reset_sensor_failure(void);
}


#endif /* __cplusplus */
#endif /* __PTX30_THREAD_H__ */
#endif /* CONFIG_PTX30 */
