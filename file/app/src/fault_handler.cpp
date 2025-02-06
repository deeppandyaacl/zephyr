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

#include <zephyr/logging/log.h>
#include <sh/sh_utils.h>
#include <fault_handler.h>
#include <ptx30_thread.h>
#include <sh/sh_data_logger.h>

LOG_MODULE_REGISTER(fault_handler, LOG_LEVEL_ERR);

// only turn on led when IMU, haptic, IMU, Memory, PMIC is failed
#define MODULE_FAIL_CHECK_FOR_LED   0X67	//0b0110 0111 

static sys_status_data_v1_t device_status;

static sh_ble::adv_data_t adv_batt_data;

static bool startup_failure = false;

int fault_handler::initialize(void)
{
    int ret = 0;

    if(device_status.module_status != 0x00){
        //Turn on solid RED LED
        startup_failure = true;
        ret = powermngr::handle_sensor_failure();
        if(ret != 0)
        {
            LOG_ERR("Not able to turn on red LED");
        }
    }    

    //Set default parameters
    device_status.adv_data = 0x11;    //ADV version: 0x08 and NFC is supported
    device_status.device_type = 0x20;
    LOG_ERR("Send status to BLE");

    adv_batt_data.device_status = device_status.module_status;
    sh_ble::update_advertising_data(sh_ble::BLE_ADV_DEVICE_STATUS, adv_batt_data);

    //Send dignostics to BLE service
    ret = sh_ble_sys_status_send((uint8_t *)&device_status, 3);
    if(ret != 0)
    {
        LOG_ERR("Not able to send status to BLE status service");
    }

    return ret;
}

int fault_handler::update_fault(fault_type sensor_type)
{
    device_status.module_status |= (1 << sensor_type);
    return 0;
}

int fault_handler::send_diagnostics(fault_type sensor_type)
{
    int ret = 0;
    device_status.module_status |= (1 << sensor_type);

    
    if(device_status.module_status & MODULE_FAIL_CHECK_FOR_LED)  
    {
        //Turn on solid RED LED
        ret = powermngr::handle_sensor_failure();
        if(ret != 0)
        {
            LOG_ERR("Not able to turn on red LED");
        }
    }

    //Set default parameters
    device_status.adv_data = 0x11;    //ADV version: 0x08 and NFC is supported
    device_status.device_type = 0x20;
    LOG_ERR("Send status to BLE");

    adv_batt_data.device_status = device_status.module_status;
    sh_ble::update_advertising_data(sh_ble::BLE_ADV_DEVICE_STATUS, adv_batt_data);

    //Send dignostics to BLE service
    ret = sh_ble_sys_status_send((uint8_t *)&device_status, 3);
    if(ret != 0)
    {
        LOG_ERR("Not able to send status to BLE status service");
    }

    return ret;
}

int fault_handler::reset_modulestatus(void)
{
    int ret = 0;
    
    ret = powermngr::reset_sensor_failure();
    if(ret != 0)
    {
        LOG_ERR("Not able to turn off red LED");
    }

    //Set default parameters
    device_status.adv_data = 0x11;    //ADV version: 0x08 and NFC is supported
    device_status.device_type = 0x20;
    device_status.module_status = 0x00;
    startup_failure = false;
    LOG_ERR("Send status to BLE");

    adv_batt_data.device_status = device_status.module_status;
    sh_ble::update_advertising_data(sh_ble::BLE_ADV_DEVICE_STATUS, adv_batt_data);

    //Send dignostics to BLE service
    ret = sh_ble_sys_status_send((uint8_t *)&device_status, 3);
    if(ret != 0)
    {
        LOG_ERR("Not able to send status to BLE status service");
    }

    sh_data_logger::update_failure_status(0);

    return 0;
}


int fault_handler::get_diagnostics(uint8_t *sensor_type)
{
    if(sensor_type == NULL)
    {
        return -1;
    }
    *sensor_type = device_status.module_status;
    return 0;
}


bool fault_handler::get_modulefauilreforled()
{
    if((device_status.module_status & MODULE_FAIL_CHECK_FOR_LED) || (startup_failure == true))
    {
        return true;
    }
    return false;
}