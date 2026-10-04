idf_component_register(
    SRCS "iot_usbh_modem.c" # (або назви ваших .c/.cpp файлів у цьому компоненті)
    INCLUDE_DIRS "." "include"
    REQUIRES usb_host esp_netif lwip
)