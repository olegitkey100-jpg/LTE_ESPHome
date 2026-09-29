idf_component_register(SRCS "ppp_modem_component.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_modem esp_iot_solution iot_usbh_cdc iot_usbh_modem usb_host
)
