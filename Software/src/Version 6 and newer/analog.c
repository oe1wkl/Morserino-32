#include "analog.h"

#include <stdint.h>
#include <stdarg.h>

#include <hal/adc_hal.h>
#include <driver/periph_ctrl.h>
#ifdef FADC_CAL_USE
#include <esp_adc_cal.h>
#endif

#ifdef FADC_CAL_USE
// Calibration table
static uint16_t adc_cal_tab[(1 << FADC_CAL_SIZE) + 1];

typedef struct {
  uint64_t v_cali_input;                                      //Input to calculate the error
  uint8_t  term_num;                                          //Term number of the algorithm formula
  const uint64_t (*coeff)[4][5][2];      //Coeff of each term. See `adc_error_coef_atten` for details (and the magic number 2)
  const int32_t  (*sign)[4][5];          //Sign of each term
} esp_adc_error_calc_param_t;

static const uint64_t adc1_error_coef_atten[4][5][2] = {
  {{27856531419538344, 1e16}, {50871540569528, 1e16}, {9798249589, 1e15}, {0, 0}, {0, 0}},                       //ADC1 atten0
  {{29831022915028695, 1e16}, {49393185868806, 1e16}, {101379430548, 1e16}, {0, 0}, {0, 0}},                     //ADC1 atten1
  {{23285545746296417, 1e16}, {147640181047414, 1e16}, {208385525314, 1e16}, {0, 0}, {0, 0}},                    //ADC1 atten2
  {{644403418269478, 1e15}, {644334888647536, 1e16}, {1297891447611, 1e16}, {70769718, 1e15}, {13515, 1e15}}     //ADC1 atten3
};

static const int32_t adc1_error_sign[4][5] = {
  {-1, -1, 1, 0,  0},  //ADC1 atten0
  {-1, -1, 1, 0,  0},  //ADC1 atten1
  {-1, -1, 1, 0,  0},  //ADC1 atten2
  {-1, -1, 1, -1, 1}   //ADC1 atten3
};

static int32_t myesp_adc_cal_get_reading_error(const esp_adc_error_calc_param_t *param, uint8_t atten) {
    if (param->v_cali_input == 0) {
        return 0;
    }

    uint64_t v_cali_1 = param->v_cali_input;
    uint8_t term_num = param->term_num;
    int32_t error = 0;
    uint64_t coeff = 0;
    uint64_t variable[term_num];
    uint64_t term[term_num];
    memset(variable, 0, term_num * sizeof(uint64_t));
    memset(term, 0, term_num * sizeof(uint64_t));

    variable[0] = 1;
    coeff = (*param->coeff)[atten][0][0];
    term[0] = variable[0] * coeff / (*param->coeff)[atten][0][1];
    error = (int32_t)term[0] * (*param->sign)[atten][0];

    for (int i = 1; i < term_num; i++) {
        variable[i] = variable[i-1] * v_cali_1;
        coeff = (*param->coeff)[atten][i][0];
        term[i] = variable[i] * coeff;

        term[i] = term[i] / (*param->coeff)[atten][i][1];
        error += (int32_t)term[i] * (*param->sign)[atten][i];
    }

    return error;
}

static uint32_t myesp_adc_cal_raw_to_voltage(uint32_t adc_reading, const esp_adc_cal_characteristics_t *chars) {
    uint32_t voltage = 0;
    int32_t error = 0;
    uint64_t v_cali_1 = 0;
    v_cali_1 = adc_reading * chars->coeff_a;
    v_cali_1 = v_cali_1 / 1000000;
    esp_adc_error_calc_param_t param = {
        .v_cali_input = v_cali_1,
        .term_num = 5,
        .coeff = &adc1_error_coef_atten,
        .sign = &adc1_error_sign,
    };
    error = myesp_adc_cal_get_reading_error(&param, chars->atten);
    voltage = (int32_t)v_cali_1 - error;
    return voltage;
}

uint16_t fadcApply(uint32_t v) {
  if(v <= 0) return adc_cal_tab[0];
  if(v >= (1 << FADC_CAL_RESOLUTION)) return adc_cal_tab[(1 << FADC_CAL_SIZE)];
  uint32_t i = (v >> (FADC_CAL_RESOLUTION - FADC_CAL_SIZE));
  uint32_t f = v & ((1 << (FADC_CAL_RESOLUTION - FADC_CAL_SIZE)) - 1);
  return (uint16_t)(((uint32_t)adc_cal_tab[i] * ((1 << (FADC_CAL_RESOLUTION - FADC_CAL_SIZE)) - f) + (uint32_t)adc_cal_tab[i + 1] * f) >> (FADC_CAL_RESOLUTION - FADC_CAL_SIZE));
}
#endif

void fadcInit(uint8_t pins, ...) {

  // Initialize ADC using built-ins
  analogReadResolution(FADC_RESOLUTION);
  analogSetAttenuation(FADC_ATTEN);
  
  // Initialize pins using built-ins
  va_list args;
  va_start(args, pins);
  while(pins--) {
    int pin = va_arg(args, int);
    analogSetPinAttenuation(pin, FADC_ATTEN);
    analogRead(pin);
  }
  va_end(args);

#ifdef FADC_CAL_USE
  // Get ADC characteristings from EFUSE
  // Note: Assumes 11dB attenuation and 12 bit resolution
  esp_adc_cal_characteristics_t chars = {};
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)1, (adc_atten_t)FADC_ATTEN, (adc_bits_width_t)(FADC_RESOLUTION - 9), 1100, &chars);
  // Generate calibration table
  for(uint16_t n = 0; n <= (1 << FADC_CAL_SIZE); n++) {
    adc_cal_tab[n] = myesp_adc_cal_raw_to_voltage(n << (FADC_RESOLUTION - FADC_CAL_SIZE), &chars);
  }
#endif

  // Enable ADC
  periph_module_enable(PERIPH_SARADC_MODULE);
  adc_power_acquire();
}
