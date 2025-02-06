#!/bin/zsh
source ../app/version.conf
source memfault_env_setup.sh

PS3='What hardware are you uploading this OTA file for: '
# options=("evt1" "evt2" "devboard" "Quit")
select opt in evt1 evt2 devboard cancel
do
    case $opt in
        evt1)
            ;&
        evt2)
            ;&
        devboard)
            hardware_version="aria-$opt"
            break;
            ;;
        cancel)
            break
            ;;
        *) 
        echo "invalid option $REPLY"
        ;;
    esac
done

cp ../build/zephyr/app_update.bin $hardware_version+$CONFIG_MEMFAULT_NCS_FW_VERSION.bin

memfault upload-ota-payload \
  --hardware-version $hardware_version \
  --software-type "aria-fw" \
  --software-version $CONFIG_MEMFAULT_NCS_FW_VERSION \
  $hardware_version+$CONFIG_MEMFAULT_NCS_FW_VERSION.bin 
