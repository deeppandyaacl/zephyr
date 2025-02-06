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

#ifndef __SIBEL_FACTORY_DATA_H__
#define __SIBEL_FACTORY_DATA_H__


#include <zephyr/kernel.h>
#include <string.h>
#include <stdio.h>
#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

/*Max Buffer size to store the cboe encoded data*/
#define MAX_BUFFER_SIZE 192
/* Max elemets that a JSON file can have.*/
#define MAX_ELEMENTS 5  
/* Max String length for factory data */
#define MAX_STRING_LEN 24

/** @brief Gloal structure to store and access the decoded cbor data */
struct fdata {
	struct zcbor_string keys[MAX_ELEMENTS];
	struct zcbor_string values[MAX_ELEMENTS];
	size_t names_count;
};


#if defined(CONFIG_FACTORY_DATA_BLOCK)
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** @brief enumeration for json factory data packet. */
enum e_json_index{
    JSON_VER_INDEX,
    SER_INDEX,
    VENDOR_INDEX,
    DATE_INDEX,
    HW_VER_INDEX
};

/** @brief Structure to make a copy of data for the furture operation. */
struct copied_fdata {
    char json_ver[MAX_STRING_LEN];
    char serial_number[MAX_STRING_LEN];
    char vendor_name[MAX_STRING_LEN];
    char date[MAX_STRING_LEN];
    char hardware_version[MAX_STRING_LEN];
};

/**
 * Copy CBOR Data from Source to Destination
 *
 * This function copies CBOR-encoded data from a source fdata structure to a destination
 * copied_fdata structure.
 *
 * @param data Pointer to the source fdata structure containing CBOR-encoded data.
 * @param cdata Pointer to the destination copied_fdata structure to store the copied data.
 *
 */
void copy_cbor_data(const struct fdata *data, struct copied_fdata *cdata);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CONFIG_FACTORY_DATA_BLOCK */

#endif /* __SIBEL_FACTORY_DATA_H__ */
