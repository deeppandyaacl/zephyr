# factory data config and upload 

Step 1
Create a JSON file using the convert_to_JSON.py script located inside scripts folder.
```
python convert_to_JSON.py --output ./generated.JSON --version 1 --sn 1023654 --vendor_name SibelHealth --date 12/13/2023  --hw_ver 1.0.0 --overwrite
```

Step 2
Create a hex file containing CBOR information of factory data and stored in configured flash partition address using partition_manager.py script located inside scripts folder.
```
python partition_manager.py -i ./generated.JSON -o ./new_output --offset 0xfc000 --size 0x4000
```

Step 3
Flash and verify the factory data into the nRF53 flash using following command
```
nrfjprog -f nrf53 --program ./new_output.hex --sectorerase --verify
```

## adding more factory information

Factory data block supports information that is supported in factory data component table which is describe here https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/matter/nrfconnect_factory_data_configuration.html

To customize the factory data, you need to make change in factory_data.h file

```
/*Max Buffer size to store the cboe encoded data*/
#define MAX_BUFFER_SIZE 192
/* Max elemets that a JSON file can have.*/
#define MAX_ELEMENTS 5  
/* Max String length for factory data */
#define MAX_STRING_LEN 24

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
```

Here ,
MAX_ELEMENTS should be configured with the number of factory data that need to be used.
e_json_index is used to map the json file and extract factory information from that key
struct copied_fdata is used to define the local string to store the factory data.