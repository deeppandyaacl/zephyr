source ../app/version.conf
source memfault_env_setup.sh

memfault --org-token $MEMFAULT_ORG_TOKEN  --org $MEMFAULT_ORG --project $MEMFAULT_PROJECT \
  upload-mcu-symbols \
  ../build/zephyr/zephyr.elf
