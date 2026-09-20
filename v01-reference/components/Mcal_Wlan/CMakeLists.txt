idf_component_register(SRCS "src/mcal_wlan.c" INCLUDE_DIRS "include" REQUIRES Std Mcal_Radio PRIV_REQUIRES esp_event esp_wifi esp_netif)
