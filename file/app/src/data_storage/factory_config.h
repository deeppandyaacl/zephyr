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

#ifndef __SIBEL_FACTORY_CONFIG_H__
#define __SIBEL_FACTORY_CONFIG_H__

#if defined (CONFIG_NVS)

#include <sh/sh_utils.h>

#define VALID_FACTORY_DATA              1
#define INVALID_FACTORY_DATA            0
#define FACTORY_DEFAULT_MANUF_DATE      "01012024"
#define FACTORY_DEFAULT_SERIAL_NUM      "00000"
#define FACTORY_DEFAULT_VENDOR          "Sibel"

#define HW_VER_SIZE 		    6 	// 01.00
#define SERIAL_NUMBER_SIZE	    6
#define VENDOR_NAME_SIZE	    20	
#define MANUF_DATE_SIZE		    9	//08302024 - MMDDYYYY

typedef enum{	
	FACTORY_HW_VERSION,
	FACTORY_SERIAL_NUMBER,
	FACTORY_VENDOR_NAME,
	FACTORY_MANUF_DATE
}factory_config_type_t;

/**
 * @brief Structure defines the factory configuration parameters
 */
typedef struct{
    /** Verify the validity of the configuration */
    uint8_t	is_valid_config;
    
    /** Device hardware version */
    uint8_t hw_version[HW_VER_SIZE];
    
    /** Device serial number */
    uint8_t serial_number[SERIAL_NUMBER_SIZE];

    /** Vendor name */
    uint8_t vendor_name[VENDOR_NAME_SIZE];

    /** Date of manufacturing */
    uint8_t manuf_date[MANUF_DATE_SIZE];
}factory_config_t;

#ifdef __cplusplus
namespace sh_factory_config{
    
    void set_factory_conf(factory_config_t *factory_config);

    void get_factory_conf(factory_config_t *factory_config);

    void load_default_factory_conf(factory_config_t *factory_config);

    void print_config_data(void);

    int set_valid_config(uint8_t valid_config);
    uint8_t get_valid_config(void);

    int set_serial_number(uint8_t *serial_number);
    uint8_t *get_serial_number(void);
    
    int set_hw_version(uint8_t *hw_version);
    uint8_t *get_hw_version(void);
    
    int set_vendor(uint8_t *vendor);
    uint8_t *get_vendor(void);
    
    int set_manufacturer(uint8_t *manufacturer);
    uint8_t *get_manufacturer(void);

}
#endif /* __cplusplus */

#endif // CONFIG_NVS

#endif /* __SIBEL_FACTORY_CONFIG_H__ */
