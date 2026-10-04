idf_component_register(
    SRCS "src/usbh_modem.c"
    INCLUDE_DIRS "." "include" "src"
    REQUIRES usb_host esp_netif lwip esp_modem
)