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

#include "factory_data.h"
#include <zephyr/shell/shell.h>
#include <shell_ble/shell_ble.h>
#include <sh/factory_data_block.h>
#include <sh/zcbor_api.h>
#include <stdlib.h>

#include <sh/sh_bmp5.h>


#if defined(CONFIG_FACTORY_DATA_BLOCK)
struct copied_fdata copy_data;
const struct device *flash_dev = FLASH_PARTITION_DEVICE ;

 /**
  * @brief Function to display the factory data on shell terminal.
  */
void print_factory_data(const struct shell *shell)
{
    fetch_factory_data(flash_dev, copy_cbor_data, &copy_data);
    shell_fprintf(shell, SHELL_NORMAL,"********************************************\n");
    shell_fprintf(shell, SHELL_NORMAL,"Version= %s.\n", copy_data.json_ver);
    shell_fprintf(shell, SHELL_NORMAL,"Vendor name= %s.\n", copy_data.vendor_name);
    shell_fprintf(shell, SHELL_NORMAL,"Hw_Version= %s.\n", copy_data.hardware_version);
    shell_fprintf(shell, SHELL_NORMAL,"serial number= %s.\n", copy_data.serial_number);
    shell_fprintf(shell, SHELL_NORMAL,"date= %s.\n", copy_data.date);
    shell_fprintf(shell, SHELL_NORMAL,"********************************************\n");
}

/**
 * @brief Function to handle the shell command to edit the factory data.
 */
static int cmd_g_edit_factory_data(const struct shell *shell, size_t argc,
        char *argv[])
{
    int res;
    if (argc <= 1) {
         shell_error(shell, "Missing data to be written.");
         return -EINVAL;
     }

    if(strcmp("version", argv[0]) == 0) {
        shell_fprintf(shell, SHELL_NORMAL, "Changing the version to: %s.\n", argv[1]);
        res = edit_factory_data(flash_dev, copy_cbor_data, &copy_data, 0,argv[1]);
        if(res) {
            shell_error(shell, "Falied to edit the factory data."); 
        }
     }
    if(strcmp("sn", argv[0]) == 0) {
        shell_fprintf(shell, SHELL_NORMAL, "Changing the serial number to: %s.\n", argv[1]);
        res = edit_factory_data(flash_dev, copy_cbor_data, &copy_data, 1,argv[1]);
        if(res) {
            shell_error(shell, "Falied to edit the factory data."); 
        }
    }
    if(strcmp("vendor_name", argv[0]) == 0) {
        shell_fprintf(shell, SHELL_NORMAL, "Changing the vendor_name to: %s.\n", argv[1]);
        res = edit_factory_data(flash_dev, copy_cbor_data, &copy_data, 2,argv[1]);
        if(res) {
            shell_error(shell, "Falied to edit the factory data."); 
        }
    }
    if(strcmp("date", argv[0]) == 0) {
        shell_fprintf(shell, SHELL_NORMAL, "Changing the date to: %s.\n", argv[1]);
        res = edit_factory_data(flash_dev, copy_cbor_data, &copy_data, 3,argv[1]);
        if(res) {
            shell_error(shell, "Falied to edit the factory data."); 
        }
    }
     if(strcmp("hw_ver", argv[0]) == 0) {
        shell_fprintf(shell, SHELL_NORMAL, "Changing the hw_ver to: %s.\n", argv[1]);
        res = edit_factory_data(flash_dev, copy_cbor_data, &copy_data, 4,argv[1]);
        if(res) {
            shell_error(shell, "Falied to edit the factory data."); 
        }
    }
    return 0;
}

/**
 * @brief Creating and Registering the shell commands for editing the factory data block.
 */
SHELL_STATIC_SUBCMD_SET_CREATE(
write_factory_data_cmd,
    SHELL_CMD_ARG(version, NULL,
        "Change the version number.\n"
        "usage:\n"
        "$ write_factory_data version <version number>\n",
        cmd_g_edit_factory_data, 2, 0),
    SHELL_CMD_ARG(sn, NULL,
        "Change the serial number\n"
        "usage:\n"
        "$ write_factory_data sn <serial number>\n",
        cmd_g_edit_factory_data, 2, 0),
    SHELL_CMD_ARG(vendor_name, NULL,
        "Change the vendor name\n"
        "usage:\n"
        "$ write_factory_data vendor_name <vendorname>\n",
        cmd_g_edit_factory_data, 2, 0),
    SHELL_CMD_ARG(date, NULL,
        "Change the date\n"
        "usage:\n"
        "$ write_factory_data date <date>\n",
        cmd_g_edit_factory_data, 2, 0),
    SHELL_CMD_ARG(hw_ver, NULL,
        "Change the Hardware Version\n"
        "usage:\n"
        "$ write_factory_data hw_ver <Hardware version>\n",
        cmd_g_edit_factory_data, 2, 0),
    SHELL_SUBCMD_SET_END
    );
SHELL_CMD_REGISTER(write_factory_data, &write_factory_data_cmd, "Write factory data", NULL);

/*Register shell command to display the factory data.*/
SHELL_CMD_ARG_REGISTER(print_factory_data, NULL, "display the factory data", &print_factory_data, 1, 0);

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

/**
 * @brief Command handler for version command.
 */
// extern float pressure_val;
static int cmd_g_get_barometer(const struct shell *shell)
{
    // shell_fprintf(shell, SHELL_NORMAL, "Pressure value received: %d\n", (int) pressure_val);

    return 0;
}


/* Registering the version command.*/
SHELL_CMD_ARG_REGISTER(get_barometer_data, NULL, "Get the current git version.", cmd_g_get_barometer, 1, 0);


#endif /* CONFIG_FACTORY_DATA_BLOCK */


#endif // CONFIG_SHELL
