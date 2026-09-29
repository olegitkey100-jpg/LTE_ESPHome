idf_component_register(
    SRCS "ppp_modem_component.cpp"
         "usb_drivers/iot_usbh_cdc.c"
         "usb_drivers/iot_usbh_modem.c"
    INCLUDE_DIRS "." "usb_drivers"
    PRIV_REQUIRES esp_modem usb_host
)
