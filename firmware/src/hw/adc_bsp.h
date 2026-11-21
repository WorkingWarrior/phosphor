#ifndef ADC_BSP_H
#define ADC_BSP_H

#include <Arduino.h>

// divider: battery voltage per volt at the ADC pin (Touch LCD 3.49: 3).
void Adc_PortInit(float divider = 3.0f);
float Adc_GetBatteryVoltage(int *data);
// Adc_Update() samples and advances the filters (call every few seconds); the getters below read the filtered result.
void Adc_Update(void);
float Adc_GetSmoothedVoltage(void);
uint8_t Adc_GetBatteryLevel(void);  // discharge-curve percent of the smoothed voltage
bool Adc_IsCharging(void);          // inferred from the voltage (no charge pin on the Touch LCD 3.49)


#endif