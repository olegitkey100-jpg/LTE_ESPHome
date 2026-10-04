idf_component_register(
    SRCS "usbh_modem.c"
    INCLUDE_DIRS "." "include"  # залежно від того, де саме лежить iot_usbh_modem.h
    REQUIRES esp_modem usb_host esp_netif lwip freertos
)