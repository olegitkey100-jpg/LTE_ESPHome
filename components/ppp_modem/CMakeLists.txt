idf_component_register(
    SRCS "ppp_modem_component.cpp"
         "iot_usbh_cdc.c"
         "iot_usbh_modem.c"
         "iot_usbh_descriptor.c"
         "usbh_helper.c"
         "usbh_modem.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        esp_modem 
        usb_host
        esp_netif
        freertos
)
