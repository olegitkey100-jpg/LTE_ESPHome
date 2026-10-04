idf_component_register(
    SRCS "usbh_modem.c"
    INCLUDE_DIRS "." "include"
    REQUIRES usb_host esp_netif lwip esp_modem
)