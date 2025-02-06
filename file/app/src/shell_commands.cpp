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

#if defined (CONFIG_SHELL)

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <shell_ble/shell_ble.h>
#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include <sh/zcbor_api.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined (CONFIG_KTD2026)
#include <sh/sh_ktd2026_led.h>
#endif

#if defined (CONFIG_LP5813)
#include <sh/sh_lp5813_led.h>
#endif

#if defined (CONFIG_DRV2624)
#include <sh/drv2624.h>
#endif // CONFIG_DRV2624

#if defined (CONFIG_PTX30)
#include <sh/sh_ptx30.h>
#include <haptic_motor_handler.h>
#include <sh/sh_data_logger.h>
#include <ble_base/ble.h>
#endif // CONFIG_PTX30

#if defined (CONFIG_NVS)
#include <sh/sh_nvs_handler.h>
#include "data_storage.h"
#endif // CONFIG_NVS
#include <sh/sh_data_logger.h>
/* This needs to be enabled when adding the BLE console app */
#if 0
/**
 * @brief Command handler for version command.
 */
static int cmd_g_get_version(const struct shell *shell)
{
    shell_fprintf(shell, SHELL_NORMAL, "The current git version: %s\n", CONFIG_APP_VERSION);
    return 0;
}

/* Registering the version command.*/
SHELL_CMD_ARG_REGISTER(version, NULL, "Get the current git version.", cmd_g_get_version, 1, 0);
#endif

#if defined(CONFIG_PTX30)

/**
* @brief Helper function to check if a value is in range
*
* @param start : start of range
* @param value : value to check in range
* @param end   : end of range 
* @param inclusive : determines if the range is inclusive
*
* @return 
*/
bool value_in_range(int start, int value, int end, bool inclusive)
{
    if (inclusive) {
        return (value >= start) && (value <= end);
    } else {
        return (value > start) && (value < end);
    }
}

/**
* @brief Callback for 'ptx30' shell commands
*
* @param shell
* @param argc
* @param argv[]
*
* @return 
*/
static int cmd_ptx_ctrl(const struct shell *shell, size_t argc,
                        char *argv[])
{
    int ret=0;
    // Init to default parameters
    static ptxChargingParams_t charge_params;
    
    if(0 == strcmp("ship", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Entering ship mode... \n");
        int ret = ptx30w::enter_ship_mode();
        if (ret) {
            shell_fprintf(shell, SHELL_ERROR, "Failed to enter ship mode: %d\n", ret);
        }
    }
    else if(0 == strcmp("set_default_params", argv[0])) {
        charge_params.ChargeCurrent = 50;
        charge_params.TerminationVoltage = ptx30wVTerm_3V70;
        charge_params.TrickleVoltage = ptx30wVTrickle_3V2;
        charge_params.RechargeVoltage = ptx30wVRecharge_3V55;
        charge_params.EnableCharging = true;
    }
    else if(0 == strcmp("charge_current", argv[0])) {
        if (value_in_range(5, atoi(argv[1]), 250, 1)) {
            charge_params.ChargeCurrent = (uint8_t)atoi(argv[1]);
        } else {
            shell_fprintf(shell, SHELL_ERROR, "Value out of range\n");
        }
    }
    else if(0 == strcmp("trickle_voltage", argv[0])) {
        if (value_in_range(0x00, atoi(argv[1]), 0x1F, 1)) {
            charge_params.TrickleVoltage = (ptx30wVTrickle_t)atoi(argv[1]);
        } else {
            shell_fprintf(shell, SHELL_ERROR, "Value out of range\n");
        }

    }
    else if(0 == strcmp("recharge_voltage", argv[0])) {
        if (value_in_range(0x00, atoi(argv[1]), 0x0F, 1)) {
            charge_params.RechargeVoltage = (ptx30wVRecharge_t)atoi(argv[1]);
        } else {
            shell_fprintf(shell, SHELL_ERROR, "Value out of range\n");
        }
    }
    else if(0 == strcmp("enable_charging", argv[0])) {
        if (value_in_range(0, atoi(argv[1]), 1, 1)) {
            charge_params.EnableCharging = (bool)atoi(argv[1]);
        } else {
            shell_fprintf(shell, SHELL_ERROR, "Value out of range\n");
        }
    }
    else if(0 == strcmp("termination_voltage", argv[0])) {
        if (value_in_range(0, atoi(argv[1]), 0x1F, 1)) {
            charge_params.TerminationVoltage = (ptx30wVTerm_t)atoi(argv[1]);
        } else {
            shell_fprintf(shell, SHELL_ERROR, "Value out of range\n");
        }
    }
    else if(0 == strcmp("update_charging_params", argv[0])) {
        if (ptx30w::set_charging_parameters(charge_params)) {
            shell_fprintf(shell, SHELL_ERROR, "Failed to update charge parameters\n");
        } else {
            shell_fprintf(shell, SHELL_NORMAL, "Charge parameters updated.\n");
        }
    }
    else if(0 == strcmp("display_oem", argv[0])) {
        ptxOemConfigParam_t oem_values;
        if (ptx30w::read_oem_values(&oem_values)) {
            shell_fprintf(shell, SHELL_ERROR, "Failed to read OEM values\n");
        } else {
            ptx30w::print_oem_values(oem_values);
            shell_fprintf(shell, SHELL_NORMAL, "PTX30 OEM Values:\n");
            shell_fprintf(shell, SHELL_NORMAL, "\tCAP_WT_INT: 0x%02x\n", oem_values.CAP_WT_INT);
            shell_fprintf(shell, SHELL_NORMAL, "\tNFC_ICHG: 0x%02x\n", oem_values.NFC_ICHG);
            shell_fprintf(shell, SHELL_NORMAL, "\tVDBAT_OFFSET_HIGH: 0x%02x\n", oem_values.VDBAT_OFFSET_HIGH);
            shell_fprintf(shell, SHELL_NORMAL, "\tVDBAT_OFFSET_LOW: 0x%02x\n", oem_values.VDBAT_OFFSET_LOW);
            shell_fprintf(shell, SHELL_NORMAL, "\tRFU1: 0x%02x\n", oem_values.RFU1);
            shell_fprintf(shell, SHELL_NORMAL, "\tRFU2: 0x%02x\n", oem_values.RFU2);
            shell_fprintf(shell, SHELL_NORMAL, "\tCURSENS_TH_SEL: 0x%02x\n", oem_values.CURSENS_TH_SEL);
            shell_fprintf(shell, SHELL_NORMAL, "\tNFC_RESISTIVE_MODSET: 0x%02x\n", oem_values.NFC_RESISTIVE_MODSET);
            shell_fprintf(shell, SHELL_NORMAL, "\tBC_COMB1: 0x%02x\n", oem_values.BC_COMB1);
            shell_fprintf(shell, SHELL_NORMAL, "\tBC_ENABLE: 0x%02x\n", oem_values.BC_ENABLE);
            shell_fprintf(shell, SHELL_NORMAL, "\tBC_VTERM_OFFSET_COLD: 0x%02x\n", oem_values.BC_VTERM_OFFSET_COLD);
            shell_fprintf(shell, SHELL_NORMAL, "\tBC_VTERM_OFFSET_HOT: 0x%02x\n", oem_values.BC_VTERM_OFFSET_HOT);
            shell_fprintf(shell, SHELL_NORMAL, "\tBC_ICHG_PCT_COLD: 0x%02x\n", oem_values.BC_ICHG_PCT_COLD);
            shell_fprintf(shell, SHELL_NORMAL, "\tBC_ICHG_PCT_HOT: 0x%02x\n", oem_values.BC_ICHG_PCT_HOT);
            shell_fprintf(shell, SHELL_NORMAL, "\tBC_ITERM_CTRL: 0x%02x\n", oem_values.BC_ITERM_CTRL);
            shell_fprintf(shell, SHELL_NORMAL, "\tBC_COMB0: 0x%02x\n", oem_values.BC_COMB0);
            shell_fprintf(shell, SHELL_NORMAL, "\tBC_VRCHG_CTRL: 0x%02x\n", oem_values.BC_VRCHG_CTRL);
            shell_fprintf(shell, SHELL_NORMAL, "\tWPT_RESISTIVE_MODSET: 0x%02x\n", oem_values.WPT_RESISTIVE_MODSET);
            shell_fprintf(shell, SHELL_NORMAL, "\tOEM_VDMCU_MODE: 0x%02x\n", oem_values.OEM_VDMCU_MODE);
            shell_fprintf(shell, SHELL_NORMAL, "\tI2C_SETTINGS: 0x%02x\n", oem_values.I2C_SETTINGS);
            shell_fprintf(shell, SHELL_NORMAL, "\tGPIO_1_CONFIG: 0x%02x\n", oem_values.GPIO_1_CONFIG);
            shell_fprintf(shell, SHELL_NORMAL, "\tGPIO_0_CONFIG: 0x%02x\n", oem_values.GPIO_0_CONFIG);
            shell_fprintf(shell, SHELL_NORMAL, "\tVDDC_TH_LOW: 0x%02x\n", oem_values.VDDC_TH_LOW);
            shell_fprintf(shell, SHELL_NORMAL, "\tWPT_REQ_SEL: 0x%02x\n", oem_values.WPT_REQ_SEL);
            shell_fprintf(shell, SHELL_NORMAL, "\tOSC_EN_NTC_MODE: 0x%02x\n", oem_values.OSC_EN_NTC_MODE);
            shell_fprintf(shell, SHELL_NORMAL, "\tADJ_WPT_DURATION_INT: 0x%02x\n", oem_values.ADJ_WPT_DURATION_INT);
            shell_fprintf(shell, SHELL_NORMAL, "\tTCM_WPT_DURATION_INT: 0x%02x\n", oem_values.TCM_WPT_DURATION_INT);
            shell_fprintf(shell, SHELL_NORMAL, "\tCCM_WPT_DURATION_INT: 0x%02x\n", oem_values.CCM_WPT_DURATION_INT);
            shell_fprintf(shell, SHELL_NORMAL, "\tCVM_WPT_DURATION_INT: 0x%02x\n", oem_values.CVM_WPT_DURATION_INT);
            shell_fprintf(shell, SHELL_NORMAL, "\tTCM_TIMEOUT: 0x%02x\n", oem_values.TCM_TIMEOUT);
            shell_fprintf(shell, SHELL_NORMAL, "\tCCM_TIMEOUT: 0x%02x\n", oem_values.CCM_TIMEOUT);
            shell_fprintf(shell, SHELL_NORMAL, "\tCVM_TIMEOUT: 0x%02x\n", oem_values.CVM_TIMEOUT);
        }
    }
    else if(0 == strcmp("display_params", argv[0])) {

        shell_fprintf(shell, SHELL_NORMAL, "==========================\n");
        shell_fprintf(shell, SHELL_NORMAL, "     Charge Current: %d\n", charge_params.ChargeCurrent);
        shell_fprintf(shell, SHELL_NORMAL, "    Trickle Voltage: 0x%02x\n", charge_params.TrickleVoltage);
        shell_fprintf(shell, SHELL_NORMAL, "   Recharge Current: 0x%02x\n", charge_params.RechargeVoltage);
        shell_fprintf(shell, SHELL_NORMAL, "Termination Voltage: 0x%02x\n", charge_params.TerminationVoltage);
        shell_fprintf(shell, SHELL_NORMAL, "   Charging Enabled: %d\n", charge_params.EnableCharging);
        shell_fprintf(shell, SHELL_NORMAL, "==========================\n");
    }

    return ret;
}
SHELL_STATIC_SUBCMD_SET_CREATE( 
    ptx30_ctrl_cmd,
    SHELL_CMD_ARG(ship, NULL,
        "Enter ship mode\n"
        "usage: $ ptx30 ship\n",
        cmd_ptx_ctrl, 1, 0),
    SHELL_CMD_ARG(charge_current, NULL,
        "Set charge current\n"
        "usage: $ ptx30 charge_current <50-250mA>\n",
        cmd_ptx_ctrl, 2, 0),
    SHELL_CMD_ARG(termination_voltage, NULL,
        "Set termination voltage\n"
        "usage: $ ptx30 termination_voltage <0x00-0x1F>\n",
        cmd_ptx_ctrl, 2, 0),
    SHELL_CMD_ARG(trickle_voltage, NULL,
        "Set trickle voltage\n"
        "usage: $ ptx30 trickle_voltage <0x00-0x07>\n",
        cmd_ptx_ctrl, 2, 0),
    SHELL_CMD_ARG(recharge_voltage, NULL,
        "Set recharge voltage\n"
        "usage: $ ptx30 recharge_voltage <0x00-0x0F>\n",
        cmd_ptx_ctrl, 2, 0),
    SHELL_CMD_ARG(enable_charging, NULL,
        "Set charging enabled\n"
        "usage: $ ptx30 enable_charging <0/1>\n",
        cmd_ptx_ctrl, 2, 0),
    SHELL_CMD_ARG(update_charging_params, NULL,
        "Update the PTX30 to use current charging params\n"
        "usage: $ ptx30 update_charging_params\n",
        cmd_ptx_ctrl, 1, 0),
    SHELL_CMD_ARG(display_params, NULL,
        "Display current charging parameters (set/unset)\n"
        "usage: $ ptx30 display_params\n",
        cmd_ptx_ctrl, 1, 0),
    SHELL_CMD_ARG(set_default_params, NULL,
        "Set charging params to their defaults\n"
        "usage: $ ptx30 set_default_params\n",
        cmd_ptx_ctrl, 1, 0),
    SHELL_CMD_ARG(display_oem, NULL,
        "Display current oem values\n"
        "usage: $ ptx30 display_oem\n",
        cmd_ptx_ctrl, 1, 0),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(ptx30, &ptx30_ctrl_cmd, "Control PTX30", NULL);
#endif //CONFIG_PTX30

#if defined(CONFIG_LP5813)
/**
 * @brief Function to handle the shell command to change LED 
 * patterns and colors.
 */
static int cmd_led_ctrl(const struct shell *shell, size_t argc,
        char *argv[])
{
    if(0 == strcmp("on", argv[0])) {
        uint8_t led_clr = atoi(argv[1]);
        if(led_clr == 0 && led_clr > 3) {
            shell_fprintf(shell, SHELL_ERROR, "Invalid LED line number\n");
            return -EINVAL;
        }
        shell_fprintf(shell, SHELL_NORMAL, "Turning on LED\n");
        led_lp5813_select_operation(2, LED_LP_ON, led_clr);
    }
    else if(0 == strcmp("off", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Turning off LED\n");
        led_lp5813_select_operation(1, LED_LP_OFF);
    }
    else if(0 == strcmp("blink", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Blinking LED\n");
        uint8_t led_clr = atoi(argv[1]);
        uint8_t on_time = atoi(argv[2]);
        uint8_t off_time = atoi(argv[3]);
        if(led_clr == 0 && led_clr > 3) {
            shell_fprintf(shell, SHELL_ERROR, "Invalid LED line number\n");
            return -EINVAL;
        }
        if((on_time < 0 || on_time > 15) || (off_time < 0 || off_time > 15)) {
            shell_fprintf(shell, SHELL_ERROR, "On and Off time must be in the range of 0-15 seconds\n");
            return -EINVAL;
        }
        led_lp5813_select_operation(4, LED_LP_BLINK, led_clr, on_time, off_time);
    }
    else if(0 == strcmp("fade", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Fading LED\n");
        uint8_t led_clr = atoi(argv[1]);
        uint8_t fade_time = atoi(argv[2]);
        if(led_clr == 0 && led_clr > 3) {
            shell_fprintf(shell, SHELL_ERROR, "Invalid LED line number\n");
            return -EINVAL;
        }
        if((fade_time < 0) || (fade_time > 15)) {
            shell_fprintf(shell, SHELL_ERROR, "Fade In/Out time must be in the range of 0-15 seconds\n");
            return -EINVAL;
        }
        led_lp5813_select_operation(3, LED_LP_FADE, led_clr, fade_time);
    }
    else if(0 == strcmp("brightness", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Changing LED brightness\n");
        uint8_t led_clr = atoi(argv[1]);
        uint8_t led_bright = atoi(argv[2]);

        if(led_bright >= 0 && led_bright <= 100) {
            led_lp5813_select_operation(3, LED_LP_BRIGHTNESS, led_clr, led_bright);
        }
        else { 
            shell_fprintf(shell, SHELL_ERROR, "Please enter brightness in range 0 to 100\n");
            return -EINVAL;
        }
    }

    return 0;
}

/**
 * @brief Creating and Registering the shell commands for 
 * changing LED patterns and colors.
 */
SHELL_STATIC_SUBCMD_SET_CREATE( 
    led_ctrl_cmd,
    SHELL_CMD_ARG(on, NULL,
        "Turn on the led\n"
        "usage: $ led on <LED number>\n",
        cmd_led_ctrl, 2, 0),
    SHELL_CMD_ARG(off, NULL,
        "Turn on the led\n"
        "usage: $ led off\n",
        cmd_led_ctrl, 1, 0),
    SHELL_CMD_ARG(blink, NULL,
        "Blink the led\n"
        "usage: $ led blink <LED Color> <Blink ON time> <Blink OFF time>\n",
        cmd_led_ctrl, 4, 0),
    SHELL_CMD_ARG(fade, NULL,
        "Blink the led\n"
        "usage: $ led fade <LED Color> <Fade time>\n",
        cmd_led_ctrl, 3, 0),
    SHELL_CMD_ARG(brightness, NULL,
        "Set LED brightness\n"
        "usage: $ led brightness <LED number> <brightness>\n"
        "brightness is in range 0 to 100\n",
        cmd_led_ctrl, 3, 0),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(led, &led_ctrl_cmd, "Control LED pattern", NULL);

#endif

#if defined(CONFIG_KTD2026)
/**
 * @brief Function to handle the shell command to change LED 
 * patterns and colors.
 */
static int cmd_led_ctrl(const struct shell *shell, size_t argc,
        char *argv[])
{
    int ret=0;
    
    if(0 == strcmp("on", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Turning on LED\n");
        led_select_operation(1, LED_KTD_ON);
    }
    else if(0 == strcmp("off", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Turning off LED\n");
        led_select_operation(1, LED_KTD_OFF);
    }
    else if(0 == strcmp("blink", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Blinking LED\n");
        uint16_t blink_timer = atoi(argv[1]);
        if (blink_timer > 0 && blink_timer <= 20000) {
            led_select_operation(2, LED_KTD_BLINK,blink_timer);
        }
        else {
            shell_error(shell, "Enter valid time interval.");
        }
    }
    else if(0 == strcmp("blink_pwm", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Blinking PWM LED\n");
        uint16_t blink_timer = atoi(argv[1]);
        uint16_t on_duty_cycle = atoi(argv[2]);
        uint16_t pwm_on_time = 0;
        uint16_t pwm_off_time = 0;
        
        if (blink_timer > 0 && blink_timer <= 20000) {
            if (on_duty_cycle > 0 && on_duty_cycle <= 100) {
                pwm_on_time = (blink_timer * on_duty_cycle) / 100;
                pwm_off_time = blink_timer - pwm_on_time;
                led_select_operation(3, LED_KTD_PWM_BLINK, pwm_on_time,pwm_off_time);
            }
            else {
                shell_error(shell, "Enter duty cycle in range 0 to 100.\n");
            }
        }
        else {
            shell_error(shell, "Enter valid time interval.\n");
        }
    }
    else if(0 == strcmp("fade", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Fading LED\n");
        uint8_t fade_steps = atoi(argv[1]);
        led_select_operation(2, LED_KTD_FADE, fade_steps);
    }
    else if(0 == strcmp("sel_clr", argv[0])) {
        // shell_fprintf(shell, SHELL_NORMAL, "Select LED Color\n");
        uint8_t r_val = 0;
        uint8_t g_val = 0;
        uint8_t b_val = 0;

        if(strcmp("red", argv[1])==0) {
            shell_fprintf(shell, SHELL_NORMAL, "Selected red LED\n");
            r_val = MAX_RGB_VAL;
        }
        else if(strcmp("green", argv[1])==0) {
            shell_fprintf(shell, SHELL_NORMAL, "Selected green LED\n");
            g_val = MAX_RGB_VAL;
        }
        else if(strcmp("blue", argv[1])==0) {
            shell_fprintf(shell, SHELL_NORMAL, "Selected blue LED\n");
            b_val = MAX_RGB_VAL;
        }
        else if(strcmp("orange", argv[1])==0) {
            shell_fprintf(shell, SHELL_NORMAL, "Selected orange LED\n");
            r_val = MAX_RGB_VAL;
            g_val = 10;
        }
        else if(strcmp("yellow", argv[1])==0) {
            shell_fprintf(shell, SHELL_NORMAL, "Selected yellow LED\n");
            r_val = MAX_RGB_VAL;
            g_val = 50;
        }
        else {
            shell_error(shell, "Invalid color input\nAvailable options are red,green,blue,orange,yellow\n");
        }

        if((r_val >= 0 && r_val <= MAX_RGB_VAL) &&
            (g_val >= 0 && g_val <= MAX_RGB_VAL) && 
            (b_val >= 0 && b_val <= MAX_RGB_VAL)) {
            led_select_operation(4, LED_KTD_SET_COLOR,r_val,g_val,b_val);
        }
        else {
            shell_error(shell, "Please enter color in range 0 to 255\n");
        }   
    }
    else if(0 == strcmp("set_clr", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Set LED Color\n");
        uint8_t r_val, g_val, b_val;
        r_val = atoi(argv[1]);
        g_val = atoi(argv[2]);
        b_val = atoi(argv[3]);
        if((r_val >= 0 && r_val <= MAX_RGB_VAL) &&
            (g_val >= 0 && g_val <= MAX_RGB_VAL) && 
            (b_val >= 0 && b_val <= MAX_RGB_VAL)) {
            led_select_operation(4, LED_KTD_SET_COLOR,r_val,g_val,b_val);
        }
        else {
            shell_error(shell, "Please enter color in range 0 to 255\n");
        }   
    }
    else if(0 == strcmp("brightness", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Chaning LED brightness\n");
        uint8_t led_bright = atoi(argv[1]);

        if(led_bright >= 0 && led_bright <= 100)
            led_select_operation(2, LED_KTD_BRIGHTNESS,led_bright);
        else 
            shell_error(shell, "Please enter brightness in range 0 to 100\n");
    }

    return ret;
}

/**
 * @brief Creating and Registering the shell commands for 
 * changing LED patterns and colors.
 */
SHELL_STATIC_SUBCMD_SET_CREATE( 
    led_ctrl_cmd,
    SHELL_CMD_ARG(on, NULL,
        "Turn on the led\n"
        "usage: $ led on\n",
        cmd_led_ctrl, 1, 0),
    SHELL_CMD_ARG(off, NULL,
        "Turn on the led\n"
        "usage: $ led off\n",
        cmd_led_ctrl, 1, 0),
    SHELL_CMD_ARG(blink, NULL,
        "Blink the led\n"
        "usage: $ led blink <blink interval>\n",
        cmd_led_ctrl, 2, 0),
    SHELL_CMD_ARG(blink_pwm, NULL,
        "Blink the led\n"
        "usage: $ led blink_pwm <blink interval> <On duty cycle>\n",
        cmd_led_ctrl, 3, 0),
    SHELL_CMD_ARG(fade, NULL,
        "Blink the led\n"
        "usage: $ led fade <num steps>\n",
        cmd_led_ctrl, 2, 0),
    SHELL_CMD_ARG(sel_clr, NULL,
        "Select the color of LED\n"
        "usage: $ led sel_clr <LED color>\n"
        "red, green, blue, orange, yellow\n",
        cmd_led_ctrl, 2, 0),
    SHELL_CMD_ARG(set_clr, NULL,
        "Set the color of LED\n"
        "usage: $ led set_clr <r> <g> <b>\n"
        "<r>, <g>, <b> values are in range 0 to 100\n",
        cmd_led_ctrl, 4, 0),
    SHELL_CMD_ARG(brightness, NULL,
        "Set LED brightness\n"
        "usage: $ led brightness <brightness>\n"
        "brightness is in range 0 to 100\n",
        cmd_led_ctrl, 2, 0),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(led, &led_ctrl_cmd, "Control LED pattern", NULL);

#endif


#if defined(CONFIG_DRV2624)

/**
 * @brief Function to handle haptic motor
 * 
 */
static int cmd_haptic_ctrl(const struct shell *shell, size_t argc,
        char *argv[])
{
    int ret=0;
    uint8_t rtp_in = 0;
    uint16_t pulse_width = 0;
    uint16_t intrvl = 0;
    
    if(0 == strcmp("on", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Turning on hatpic motor\n");
        drv2624::haptic_select_operation(1, HAPTIC_ON);
    }
    else if(0 == strcmp("off", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Turning off haptic motor\n");
        drv2624::haptic_select_operation(1, HAPTIC_OFF);
    }
    else if(0 == strcmp("pulse", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Pulsing haptic motor\n");
        drv2624::haptic_select_operation(1, HAPTIC_PULSE);
    }
    else if(0 == strcmp("set_in", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Set haptic intensity\n");
        rtp_in = atoi(argv[1]);
        if(rtp_in >= 0 && rtp_in <= 127) {
            drv2624::haptic_select_operation(2, HAPTIC_SET_AMPLITUDE, rtp_in);
        }else {
            shell_fprintf(shell, SHELL_NORMAL, "Haptic intensity must be in the range of 0 to 127\n");
        }
    }
    else if(0 == strcmp("start_pattern", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Start haptic pattern\n");
        rtp_in = atoi(argv[1]);
        intrvl = atoi(argv[2]);
        pulse_width = atoi(argv[3]);
        uint8_t haptic_enable = 0;
        uint16_t duration = 0;
        uint16_t interval = 0;

        if(intrvl > 0 && pulse_width > 0 && (rtp_in >= 0 && rtp_in <= 127)) {
            haptic_params_t haptic_param;
            memset(&haptic_param, 0, sizeof(haptic_param));
            haptic_param.intensity = rtp_in;
            haptic_param.pulse_width = pulse_width;
            haptic_param.interval = intrvl;
            haptic_param.duration = CONFIG_HAPTIC_DEFAULT_DURATION;
            

            sh_data_logger::get_haptic_enable_status(&haptic_enable, &duration, &interval);
            if(haptic_enable)
            {
                haptic_param.index = HAPTIC_INDEX_SWALLOW_PARAMETER_CHANGE;
                //Update the haptic paramaters
                sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t::CUSTOM, &haptic_param);
            }
            else
            {
                //Enable the haptic motor
                haptic_param.index = HAPTIC_INDEX_ENABLE;
                //Update the haptic paramaters
                sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t::CUSTOM, &haptic_param);
                k_msleep(100);
                haptic_param.index = HAPTIC_INDEX_SWALLOW_PARAMETER_CHANGE;
                //Update the haptic paramaters
                sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t::CUSTOM, &haptic_param);
            }
            k_msleep(100);
            //Now start the swallow model
            sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t::SWALLOW, NULL);
        }
        else {
            shell_fprintf(shell, SHELL_NORMAL, "Haptic values not supported\n");
        }
    }
    else if(0 == strcmp("stop_pattern", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Stop haptic pattern\n");
        sh_haptic_motor_handler::operation(sh_haptic_motor_handler::type_t::OFF, NULL);
    }

    return ret;
}

/**
 * @brief Creating and Registering the shell commands for 
 *        for testing haptic motor
 */
SHELL_STATIC_SUBCMD_SET_CREATE( 
    hatpic_ctrl_cmd,
    SHELL_CMD_ARG(on, NULL,
        "Turn on haptic motor\n"
        "usage: $ haptic on\n",
        cmd_haptic_ctrl, 1, 0),
    SHELL_CMD_ARG(off, NULL,
        "Turn off the haptic motor\n"
        "usage: $ haptic off\n",
        cmd_haptic_ctrl, 1, 0),
    SHELL_CMD_ARG(pulse, NULL,
        "Turn off the haptic motor pulse\n"
        "usage: $ haptic pulse\n",
        cmd_haptic_ctrl, 1, 0),   
    SHELL_CMD_ARG(set_in, NULL,
        "Set the haptic motor intensity \n"
        "usage: $ haptic set_in <value>\n",
        cmd_haptic_ctrl, 2, 0),
    SHELL_CMD_ARG(start_pattern, NULL,
        "Start the haptic motor pattern \n"
        "usage: $ haptic start_pattern <intensity> <interval(sec)> <pulse_width(ms)>\n",
        cmd_haptic_ctrl, 4, 0),
    SHELL_CMD_ARG(stop_pattern, NULL,
        "Stop the haptic motor pattern \n"
        "usage: $ haptic stop_pattern\n",
        cmd_haptic_ctrl, 1, 0),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(haptic, &hatpic_ctrl_cmd, "Control haptic motor", NULL);
#endif

#if defined (CONFIG_NVS)


/**
 * @brief Read the requested configuration data from the sh_config read command 
 * and print the command details on the shell console
 *
 * @param shell         instance of shell
 * @param config_type   requested configuration type to be read and print
 * 
 * @return none
 */
static void config_cmd_read_data(const struct shell *shell, config_type_t config_type)
{
    uint8_t data_buff[sizeof(config_v1_t)] = {0};

    switch (config_type)
    {
    case FACTORY_CONFIG:
        {
            factory_config_t local_conf;
            sh_data_storage::fetch_data(FACTORY_CONFIG, data_buff);
            memcpy(&local_conf, data_buff, sizeof(factory_config_t));

            // print factory config
            shell_fprintf(shell, SHELL_NORMAL, "Factory configuration\n");
            shell_fprintf(shell, SHELL_NORMAL, "is_valid_config:        %d\n", local_conf.is_valid_config);
            shell_fprintf(shell, SHELL_NORMAL, "hw_version:             %s\n", local_conf.hw_version);
            shell_fprintf(shell, SHELL_NORMAL, "serial_number:          %s\n", local_conf.serial_number);
            shell_fprintf(shell, SHELL_NORMAL, "vendor_name:            %s\n", local_conf.vendor_name);
            shell_fprintf(shell, SHELL_NORMAL, "manuf_date:             %s\n", local_conf.manuf_date);
        }
        break;
    case HAPTIC_CONFIG:
        {
            haptic_config_t local_conf;
            sh_data_storage::fetch_data(HAPTIC_CONFIG, data_buff);
            memcpy(&local_conf, data_buff, sizeof(haptic_config_t));

            shell_fprintf(shell, SHELL_NORMAL, "Haptic configuration\n");
            shell_fprintf(shell, SHELL_NORMAL, "enabled:                %d\n", local_conf.enabled);
            shell_fprintf(shell, SHELL_NORMAL, "duration:               %d\n", local_conf.pattern.duration);
            shell_fprintf(shell, SHELL_NORMAL, "intensity:              %d\n", local_conf.pattern.intensity);
            shell_fprintf(shell, SHELL_NORMAL, "interval:               %d\n", local_conf.pattern.interval);
            shell_fprintf(shell, SHELL_NORMAL, "pulse_width:            %d\n", local_conf.pattern.pulse_width);
        }
        break;
    case IMU_CONFIG:
        {
            imu_config_t local_conf;
            sh_data_storage::fetch_data(IMU_CONFIG, data_buff);
            memcpy(&local_conf, data_buff, sizeof(imu_config_t));

            shell_fprintf(shell, SHELL_NORMAL, "IMU configuration\n");
            shell_fprintf(shell, SHELL_NORMAL, "enabled:                %d\n", local_conf.enabled);
            shell_fprintf(shell, SHELL_NORMAL, "acc_mode:               %d\n", local_conf.acc_mode);
            shell_fprintf(shell, SHELL_NORMAL, "acc_odr:                %d\n", local_conf.acc_odr);
            shell_fprintf(shell, SHELL_NORMAL, "acc_range:              %d\n", local_conf.acc_range);
            shell_fprintf(shell, SHELL_NORMAL, "gyro_mode:              %d\n", local_conf.gyro_mode);
            shell_fprintf(shell, SHELL_NORMAL, "gyro_odr:               %d\n", local_conf.gyro_odr);
            shell_fprintf(shell, SHELL_NORMAL, "gyro_range:             %d\n", local_conf.gyro_range);
        }
        break;
    case MIC_CONFIG:
        {
            mic_data_config_t local_conf;
            sh_data_storage::fetch_data(MIC_CONFIG, data_buff);
            memcpy(&local_conf, data_buff, sizeof(mic_data_config_t));

            shell_fprintf(shell, SHELL_NORMAL, "MIC configuration\n");
            shell_fprintf(shell, SHELL_NORMAL, "enabled:                %d\n", local_conf.enabled);
            shell_fprintf(shell, SHELL_NORMAL, "mic_lchannel_gain:      %d\n", local_conf.mic_lchannel_gain);
            shell_fprintf(shell, SHELL_NORMAL, "mic_rchannel_gain:      %d\n", local_conf.mic_rchannel_gain);
        }
        break;
    case BARO_CONFIG:
        {
            baro_config_t local_conf;
            sh_data_storage::fetch_data(BARO_CONFIG, data_buff);
            memcpy(&local_conf, data_buff, sizeof(baro_config_t));

            shell_fprintf(shell, SHELL_NORMAL, "Barometer configuration\n");
            shell_fprintf(shell, SHELL_NORMAL, "enabled:                %d\n", local_conf.enabled);
            shell_fprintf(shell, SHELL_NORMAL, "bmp5_fifo_frame_sel:    %d\n", local_conf.bmp5_fifo_frame_sel);
            shell_fprintf(shell, SHELL_NORMAL, "bmp5_fifo_threshold:    %d\n", local_conf.bmp5_fifo_threshold);
            shell_fprintf(shell, SHELL_NORMAL, "bmp5_odr:               %d\n", local_conf.bmp5_odr);
        }
        break;
    case SHRD_CONFIG:
        {
            shrd_config_t local_conf;
            memset(data_buff, 0x00, sizeof(data_buff));
            sh_data_storage::fetch_data(SHRD_CONFIG, data_buff);
            memcpy(&local_conf, data_buff, sizeof(shrd_config_t));

            shell_fprintf(shell, SHELL_NORMAL, "SHRD configuration\n");
            shell_fprintf(shell, SHELL_NORMAL, "activity :              %s\n", (local_conf.activity)?"yes":"no");
            shell_fprintf(shell, SHELL_NORMAL, "arm_angle:              %s\n", (local_conf.arm_angle)?"yes":"no");
            shell_fprintf(shell, SHELL_NORMAL, "barometer:              %s\n", (local_conf.baro)?"yes":"no");
            shell_fprintf(shell, SHELL_NORMAL, "cough:                  %s\n", (local_conf.cough)?"yes":"no");
            shell_fprintf(shell, SHELL_NORMAL, "events:                 %s\n", (local_conf.events)?"yes":"no");
            shell_fprintf(shell, SHELL_NORMAL, "imu:                    %s\n", (local_conf.imu)?"yes":"no");
            shell_fprintf(shell, SHELL_NORMAL, "imu_aggregate:          %s\n", (local_conf.imu_aggregated)?"yes":"no");
            shell_fprintf(shell, SHELL_NORMAL, "mic:                    %s\n", (local_conf.mic)?"yes":"no");
            shell_fprintf(shell, SHELL_NORMAL, "scratch:                %s\n", (local_conf.scratch)?"yes":"no");
            shell_fprintf(shell, SHELL_NORMAL, "temperature:            %s\n", (local_conf.temp)?"yes":"no");
            shell_fprintf(shell, SHELL_NORMAL, "step:                   %s\n", (local_conf.steps)?"yes":"no");
        }
        break;
    case POWER_CONFIG:
        {
            sh_data_storage::fetch_data(POWER_CONFIG, data_buff);
            shell_fprintf(shell, SHELL_NORMAL, "Power mode configuration\n");
            shell_fprintf(shell, SHELL_NORMAL, "power_mode:  %s\n", (data_buff[0]==NORMAL)?"normal":"lowpower");
        }
        break;

    default:
        shell_error(shell, "no such configuration found.\n");
        shell_help(shell);
        break;
    }
    
}


/**
 * @brief sh_config read command handler 
 *
 * @param shell     instance of shell
 * @param sub_cmd   sub-command 
 * 
 * @return int Success indicates 0 and failure is non-zero..
 */
static int config_cmd_read_handler(const struct shell *shell, char *sub_cmd)
{
    int ret = 1;
    if(strcmp(sub_cmd, "factory_config") == 0) {
        config_cmd_read_data(shell, FACTORY_CONFIG);
    } else if(strcmp(sub_cmd, "haptic_config") == 0) {
        config_cmd_read_data(shell, HAPTIC_CONFIG);
    } else if(strcmp(sub_cmd, "imu_config") == 0) {
        config_cmd_read_data(shell, IMU_CONFIG);
    } else if(strcmp(sub_cmd, "mic_config") == 0) {
        config_cmd_read_data(shell, MIC_CONFIG);
    } else if(strcmp(sub_cmd, "barometer_config") == 0) {
        config_cmd_read_data(shell, BARO_CONFIG);
    } else if(strcmp(sub_cmd, "shrd_config") == 0) {
        config_cmd_read_data(shell, SHRD_CONFIG);
    } else if(strcmp(sub_cmd, "power_config") == 0) {
        config_cmd_read_data(shell, POWER_CONFIG);
    } else if(strcmp(sub_cmd, "all") == 0) {
        config_cmd_read_data(shell, FACTORY_CONFIG);
        shell_fprintf(shell, SHELL_NORMAL, "\n");
        config_cmd_read_data(shell, HAPTIC_CONFIG);
        shell_fprintf(shell, SHELL_NORMAL, "\n");
        config_cmd_read_data(shell, IMU_CONFIG);
        shell_fprintf(shell, SHELL_NORMAL, "\n");
        config_cmd_read_data(shell, MIC_CONFIG);
        shell_fprintf(shell, SHELL_NORMAL, "\n");
        config_cmd_read_data(shell, BARO_CONFIG);
        shell_fprintf(shell, SHELL_NORMAL, "\n");
        config_cmd_read_data(shell, SHRD_CONFIG);
        shell_fprintf(shell, SHELL_NORMAL, "\n");
        config_cmd_read_data(shell, POWER_CONFIG);
    } else {
        shell_error(shell, "no such configuration found.\n");
        shell_help(shell);
        ret = -1;
    }

    return ret;
}


/**
 * @brief configuration command handler to handle sh_config command 
 *
 * @param shell instace of shell 
 * @param argc  argument count 
 * @param argv  sub-command array
 * 
 * @return int Success indicates 0 and failure is non-zero..
 */
static int config_cmd_handler(const struct shell *shell, size_t argc,
        char *argv[])
{
    int ret=0;
    
    if(0 == strcmp("read", argv[0])) {
        ret = config_cmd_read_handler(shell, argv[1]);
    } else if(0 == strcmp("write", argv[0])) {
        ret = sh_data_storage::handle_shell_write_cmd(argv);
        if(ret == CONFIG_SHELL_UNKNOWN_VALUE) {
            shell_error(shell, "invalid parameter value or value out of range.\n");
        } else if(ret == CONFIG_SHELL_UNKNOWN_PARAM) {
            shell_error(shell, "unknown parameter.\n");
        } else if(ret == CONFIG_SHELL_UNKNOWN_CONFIG) {
            shell_error(shell, "unknown configuration.\n");
            shell_help(shell);
        } else if(ret == CONFIG_SHELL_VALID_FACTORY) {
            shell_error(shell, "valid configuration already present, cannot edit the factory configuration.\n");
        } else if(ret == CONFIG_SHELL_CONFIG_NOT_ENABLED) {
            shell_error(shell, "Related configuration is not enabled, need to enable the configuration first.\n");
        } else {
            shell_fprintf(shell, SHELL_NORMAL, "successfuly write new value\n");
            shell_fprintf(shell, SHELL_NORMAL, "NOTE: this will not saved in the NVS yet\n");
            shell_fprintf(shell, SHELL_NORMAL, "you need to use *save* command to save new config in the NVS\n");
        }
    } else if(0 == strcmp("save_factory_config", argv[0])) {
        ret = sh_data_storage::store_data(FACTORY_CONFIG);
        if(ret == CONFIG_SHELL_VALID_FACTORY) {
            shell_error(shell, "valid configuration already present, cannot save the factory configuration.\n");
        } else if (ret < 0) {
            shell_error(shell, "Failed to read current config.\n");
        } else if (ret > 0) {
            shell_fprintf(shell, SHELL_NORMAL, "successfuly write new factory config : %d\n", ret);
            config_cmd_read_data(shell, FACTORY_CONFIG);
        } else {
            shell_error(shell, "save_factory_config invalid input : %d .\n", ret);
            shell_help(shell);
        }
    } else if(0 == strcmp("save_data_config", argv[0])) {
        ret = sh_data_storage::store_data(ALL_CONFIG);
        if (ret < 0) {
            shell_error(shell, "Failed to read current config : %d.\n", ret);
        } else if (ret > 0) {
            shell_fprintf(shell, SHELL_NORMAL, "successfuly write new data config : %d\n", ret);
        } else {
            shell_error(shell, "save_data_config invalid input : %d.\n", ret);
            shell_help(shell);
        }
    } else if(0 == strcmp("select_profile", argv[0])) {
        ret = sh_data_storage::handle_shell_profile_cmd(argv);
        if(ret == CONFIG_SHELL_UNKNOWN_VALUE) {
            shell_error(shell, "invalid parameter value or value out of range.\n");
            shell_help(shell);
        } else if (ret < 0) {
            shell_error(shell, "unknown error : %d.\n", ret);
        } else {
            shell_fprintf(shell, SHELL_NORMAL, "%s profile selected successfully\n", argv[1]);
            
            // Reset the Sensor to update the configuration as our sensor does not reboot until battery is dead
            NVIC_SystemReset();
        }
    } else {
        shell_error(shell, "invalid input.\n");
        shell_help(shell);
    }

    return ret;
}



/**
 * @brief Creating and Registering the shell commands for 
 * changing LED patterns and colors.
 */
SHELL_STATIC_SUBCMD_SET_CREATE( 
    config_sub_cmd,
    SHELL_CMD_ARG(read, NULL,
        "Read the current configuration\n"
        "usage: $ sh_config read <config_type>\n"
        "<config_type>\n"
        "all:               read all configurations\n"
        "verion:            read config version\n"
        "imu_config:        read imu configuration\n"
        "factory_config:    read factory configuration\n"
        "mic_config:        read microphone configuration\n"
        "barometer_config:  read barometer configuration\n"
        "shrd_config:       read shrd configuration\n"
        "haptic_config:     read haptic configuration\n"
        "power_config:      read sensor power configuration\n",
        config_cmd_handler, 2, 0),

    SHELL_CMD_ARG(write, NULL,
        "Write the selected configuration\n"
        "usage: $ sh_config write <config_type> <option> <value>\n"
        "<config_type>\n"
        "imu_config\n"
        "factory_config\n"
        "mic_config\n"
        "barometer_config\n"
        "shrd_config\n"
        "haptic_config\n"
        "power_config\n"
        "<option>\n"
        "individual config parameter, use *read* command to get\n"
        "the list of supported parameters of a configuration"
        "<value>\n"
        "value to be filled in parameter\n",
        config_cmd_handler, 3, 1),

    SHELL_CMD_ARG(save_factory_config, NULL,
        "Save the factory configuration\n"
        "usage: $ sh_config save_factory_config \n"
        "This will save the updated factory configuration\n"
        "Note: This operation is one time only, once update it cannot be undone\n"
        "If factory configuration already present, this command has no effect\n",
        config_cmd_handler, 1, 0),

    SHELL_CMD_ARG(save_data_config, NULL,
        "Save all the data configuration\n"
        "usage: $ sh_config save_data_config \n"
        "This will save all the updated data configurations\n",
        config_cmd_handler, 1, 0),
    
    SHELL_CMD_ARG(select_profile, NULL,
        "swallow:\n"
        "accel  - +-2g, 1600Hz\n"
        "mic    - gain 80\n"
        "haptic - duration 7210, interval 60\n"
        "\n"
        "scratch:\n"
        "accel  - +-4g, 1600Hz\n"
        "mic    - disable\n"
        "haptic - enable, no parameter update\n"
        "\n"
        "swallow_raw:\n"
        "accel  - +-2g, 1600Hz\n"
        "mic    - gain 80\n"
        "haptic - duration 7210, interval 60\n"
        "\n"
        "scratch_raw:\n"
        "accel  - +-4g, 1600Hz\n"
        "mic    - disable\n"
        "haptic - enable, no parameter update\n"
        "\n"
        "ibd:\n"
        "accel  - +-4g, 1600Hz\n"
        "mic    - gain 80\n"
        "haptic - disable, no parameter update\n"
        "\n"
        "cough:\n"
        "accel  - +-4g, 1600Hz\n"
        "mic    - gain 80\n"
        "haptic - disable, no parameter update\n",
        config_cmd_handler, 2, 0),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(sh_config, &config_sub_cmd, "Control sensor's configurations", NULL);

#endif // CONFIG_NVS

#if defined (CONFIG_BT_BONDING_FEATURE)
/**
 * @brief Function to handle haptic motor
 * 
 */
static int cmd_ble_cmd(const struct shell *shell, size_t argc,
        char *argv[])
{
    int ret=0;
    
    if(0 == strcmp("start", argv[0])) {
        shell_fprintf(shell, SHELL_NORMAL, "Erase the bond information\n");
        
        //Call bond erase API
        ret = sh_ble::erase_bond();
    }
    return ret;
}

/**
 * @brief Creating and Registering the shell commands for 
 *        for testing haptic motor
 */
SHELL_STATIC_SUBCMD_SET_CREATE( 
    bt_bond_cmd,
    SHELL_CMD_ARG(start, NULL,
        "Erase the bond\n"
        "usage: $ bt_bond_erase start\n",
        cmd_ble_cmd, 1, 0),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(bt_bond_erase, &bt_bond_cmd, "erase bond information", NULL);
#endif //CONFIG_BT_BONDING_FEATURE
static int externalFlashErase(const struct shell *shell, size_t argc,
        char *argv[]){
    int ret = 0;
    int s_add;
    size_t e_size;
    int retVal;
    if(0 == strcmp("erase", argv[0])) {
        s_add = atoi(argv[1]);
        e_size = atoi(argv[2]);
        if (s_add < 0){
            shell_fprintf(shell, SHELL_NORMAL, "Starting Memory address is invalid \n");
        }
        else if ((s_add + e_size)>= 1*1024*1024*1024){
            shell_fprintf(shell, SHELL_NORMAL, "Memory address + size is greater then total avilable memory\n Enter Valid Combination");
        }
        else {
        shell_fprintf(shell, SHELL_NORMAL, "Erasing The External Flash Memory\n");
        retVal = sh_data_logger::flash_erase_size(s_add,e_size);
        if (retVal == SH_DL_SUCCESS){
            char mess[128];
            memset(mess,0,128);
            sprintf(mess," erassed the memory from %d to %d\n",s_add,(s_add+e_size-1));
            shell_fprintf(shell, SHELL_NORMAL,"Successfully");
            shell_fprintf(shell, SHELL_NORMAL,mess);
            }
            else {
                shell_fprintf(shell, SHELL_NORMAL,"Failed to erase memory\n");
            }
        }
    }
    return ret;
}
SHELL_STATIC_SUBCMD_SET_CREATE( 
    flash_erase,
    SHELL_CMD_ARG(erase, NULL,
        "External Flash Operation\n"
        "usage: $ flash erase <start_address> <size_in_bytes>\n",
        externalFlashErase, 3, 0),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(flash, &flash_erase, "erase external flash", NULL);

#endif
