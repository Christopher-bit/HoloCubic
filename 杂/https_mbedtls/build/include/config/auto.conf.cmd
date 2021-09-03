deps_config := \
	/home/hzx/esp/esp-idf/components/app_trace/Kconfig \
	/home/hzx/esp/esp-idf/components/aws_iot/Kconfig \
	/home/hzx/esp/esp-idf/components/bt/Kconfig \
	/home/hzx/esp/esp-idf/components/driver/Kconfig \
	/home/hzx/esp/esp-idf/components/esp32/Kconfig \
	/home/hzx/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/home/hzx/esp/esp-idf/components/esp_event/Kconfig \
	/home/hzx/esp/esp-idf/components/esp_http_client/Kconfig \
	/home/hzx/esp/esp-idf/components/esp_http_server/Kconfig \
	/home/hzx/esp/esp-idf/components/ethernet/Kconfig \
	/home/hzx/esp/esp-idf/components/fatfs/Kconfig \
	/home/hzx/esp/esp-idf/components/freemodbus/Kconfig \
	/home/hzx/esp/esp-idf/components/freertos/Kconfig \
	/home/hzx/esp/esp-idf/components/heap/Kconfig \
	/home/hzx/esp/esp-idf/components/libsodium/Kconfig \
	/home/hzx/esp/esp-idf/components/log/Kconfig \
	/home/hzx/esp/esp-idf/components/lwip/Kconfig \
	/home/hzx/esp/esp-idf/components/mbedtls/Kconfig \
	/home/hzx/esp/esp-idf/components/mdns/Kconfig \
	/home/hzx/esp/esp-idf/components/mqtt/Kconfig \
	/home/hzx/esp/esp-idf/components/my_com/Kconfig \
	/home/hzx/esp/esp-idf/components/nvs_flash/Kconfig \
	/home/hzx/esp/esp-idf/components/openssl/Kconfig \
	/home/hzx/esp/esp-idf/components/pthread/Kconfig \
	/home/hzx/esp/esp-idf/components/spi_flash/Kconfig \
	/home/hzx/esp/esp-idf/components/spiffs/Kconfig \
	/home/hzx/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/home/hzx/esp/esp-idf/components/vfs/Kconfig \
	/home/hzx/esp/esp-idf/components/wear_levelling/Kconfig \
	/home/hzx/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/hzx/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/hzx/esp/esp-idf-v3.1.4/my_prj/https_mbedtls/main/Kconfig.projbuild \
	/home/hzx/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/hzx/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
