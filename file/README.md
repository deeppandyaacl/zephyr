# Anne Aria  
Aria is Adam 10 project  

## Prerequisites

### Install following tools
``visual studio``
``nrfutil``

### visual studio plug-ins
``C/C++``
``nRF connect for VS Code``
``nRF connect for VS Code Extension Pack``
``nRF DeviceTree``
``nRF Kconfig``
``nRF Terminal``
``CMake``
``CMake Language Support``
``CMake Tools``
``Device Tree for the Zephyr Project``
``Kconfig for the Zephyr Project``


## Getting Started  
To set up the development environment follow steps 1-7, skipping step 4 in the link below. Please note the link is pointing you to the nRF Connect SDK V2.4.0    
[Zephyr Getting Started](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/2.4.0/nrf/getting_started/installing.html).  

### Initialization

The first step is to initialize the workspace folder (``my-workspace``) where
``Anne-Aria`` and all the Zephyr modules will be cloned. To do so run the following
command:  
```shell
west init -m https://github.com/sibelhealth/Anne-Aria.git --mr develop my-workspace
```
Go into the workspace folder using:
```shell
cd my-workspace
```  
Clone the modules with the following command:  
```shell
west update
```  

### Building and running using command 

To build the application, run the following command:

Go into ``my-workspace/Anne-Aria``:
```shell
cd Anne-Aria
```  
Build the application for EVT1 board 
```shell
west build --pristine=always -b anne_aria_evt_rev_a_cpuapp app/ -- -DCONF_FILE="$(pwd)/app/prj_evt1.conf" 

```

Build the application for EVT2 board 
```shell
west build --pristine=always -b anne_aria_cpuapp@2_0_0 app/ -- -Dmcuboot_CONF_FILE="$(pwd)/app/mcuboot.conf" -DCONF_FILE="$(pwd)/app/prj_evt2.conf" 

```

Build the application for EVT3 board 
```shell
west build --pristine=always -b anne_aria_cpuapp@3_0_0 app/ -- -Dmcuboot_CONF_FILE="$(pwd)/app/mcuboot.conf" -DCONF_FILE="$(pwd)/app/prj_evt2.conf" 

```

Build the application for EVT2 boards for alpha relase
```shell
west build --pristine=always -b anne_aria_cpuapp@3_0_0 app/ -- -Dmcuboot_CONF_FILE="$(pwd)/app/mcuboot.conf" -DCONF_FILE="$(pwd)/app/prj_evt2.conf" -DRELEASE_TYPE="ALPHA"

```

Build the application for EVT3 boards for custom feature release
```shell
west build --pristine=always -b anne_aria_cpuapp@3_0_0 app/ -- -Dmcuboot_CONF_FILE="$(pwd)/app/mcuboot.conf" -DCONF_FILE="$(pwd)/app/prj_evt2.conf" -DRELEASE_TYPE="FEATURE" -DFEATURE_STRING="custom_feature"

```

Flash the application  
```shell
west flash
```

### Configure VS Studio nrfconnect plugin

Open the Workspace folder in Visual Studio
```shell
In File -> Open Folder -> Open Workspace folder
```

Open Application from nrfconnect plugin
```shell
In nrf plugin option -> Open Application -> Open Anne-Aria/app folder.
```

Create new build configuration folder

In Application create a new build configuration.

For EVT1 Select board anne_aria_rev_c board from custom board

Select Below configuration for EVT1 board
```
prj_evt1.conf
```
For EVT2 and EVT3 Select board anne_aria_cpuapp and select Revision as 2.0.0 and 3.0.0 respectively

Select Below configuration for EVT2 board
```
prj_evt2.conf
```

Add the following extra cmake argument to select the relase mode as ALPHA release mode
```
-DRELEASE_TYPE=ALPHA
```

Enable debug options

Click on Build Configuration
```

### Building and running using VS Studio nrfconnect plugin

build the application
```
shell
In Actions, Click on Build
```

Connect the device
```
shell
In Connected device Menu, connect the device, if not connected
```

Flash the application
```shell
In Actions, Click on Flash
```

### factory data config and upload 

Facotry data information is mentioned in FACTORY_DATA.md file.

### For memfault core dump

For debugging issues in code, add #include "memfault/core/trace_event.h" into it and core messages like below in the error conditions to trace them.
MEMFAULT_TRACE_EVENT(critical_log);
MEMFAULT_TRACE_EVENT_WITH_LOG(critical_log, "Aria");

Once done, trace the logs using shell,
where all trace can be downloaded with with the shell command "mflt export"

Copy the chunk data received and upload into memfault cloud to know information about those chunks.

For adding more user trace events add into "memfault_platform_config.h" file inside memfault_config folder.

