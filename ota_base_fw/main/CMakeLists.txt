idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_http_server app_update nvs_flash mdns driver u8g2)
