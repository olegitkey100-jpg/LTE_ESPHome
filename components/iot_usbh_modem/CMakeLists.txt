idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "private_include"
                    REQUIRES driver usb_host
                    PRIV_REQUIRES esp_event)

