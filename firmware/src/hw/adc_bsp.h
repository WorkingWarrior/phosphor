#ifndef ADC_BSP_H
#define ADC_BSP_H

#include <Arduino.h>

// divider: battery voltage per volt at the ADC pin (Touch LCD 3.49: 3).
void Adc_PortInit(float divider = 3.0f);
float Adc_GetBatteryVoltage(int *data);
uint8_t Adc_GetBatteryLevel(void);


#endif