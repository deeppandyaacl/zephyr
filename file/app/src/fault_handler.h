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

#ifndef __FAULT_HANDLER__
#define __FAULT_HANDLER__

#include <zephyr/kernel.h>
#include <ble_base/ble.h>
#include <sys_service/ble_sys.h>

typedef enum
{
    /* Fault modules */
    PMIC_FAULT,
    NAND_FLASH_FAULT,
    IMU_FAULT,
    BAROMETER_FAULT,
    TEMP_FAULT,
    MIC_FAULT,
    HAPTIC_FAULT,
    LED_FAULT
}fault_type;

#ifdef __cplusplus
namespace fault_handler{

    /**
     * @brief Initalize fault module and update led and ble if there is any fault
     * @return int Success indicates 0 and failure is non-zero.
     */
    int initialize(void);

    /**
     * @brief Send fault related information to fault service
     * @param sensor_type : type of sensor having fault
     * @return int Success indicates 0 and failure is non-zero.
     */
    int update_fault(fault_type sensor_type);

    /**
     * @brief Send fault related information to fault service
     *        Turn on red LED
     * @param sensor_type : type of sensor having fault
     * @return int Success indicates 0 and failure is non-zero.
     */
    int send_diagnostics(fault_type sensor_type);

    /**
     * @brief reset module status and disable red led
     *
     * @return int Success indicates 0 and failure is non-zero.
     */
    int reset_modulestatus(void);

    /**
     * @brief Get fault diagnostics for SHRD
     * @param sensor_type : type of sensor having fault
     * @return int Success indicates 0 and failure is non-zero.
     */
    int get_diagnostics(uint8_t *sensor_type);

    /**
     * @brief Get fault diagnostics for LED status
     * @return return true led on due to fault else return false
     */
    bool get_modulefauilreforled();

}
#endif /* __cplusplus */

#endif /* __FAULT_HANDLER__ */