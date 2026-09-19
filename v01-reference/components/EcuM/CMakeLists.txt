idf_component_register(SRCS "src/ecum.c" INCLUDE_DIRS "include" REQUIRES Os esp_timer Mcal_Port Mcal_Pwm Mcal_I2c Drv_Bme280 Rte Hm Log Det IoHwAb Swc_ClimateController Swc_DisplayDemo)
