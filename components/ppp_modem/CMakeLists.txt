idf_component_register(
    SRCS "ppp_modem_component.cpp"
         "iot_usbh_cdc.c"
         "iot_usbh_modem.c"
    INCLUDE_DIRS 
        "."
    PRIV_REQUIRES 
        esp_modem 
        usb_host
        esp_netif
        freertos
)
