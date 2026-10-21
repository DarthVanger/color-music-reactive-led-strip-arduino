#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <arduinoFFT.h>
#include <math.h>

#define NEOPIXEL_DI_PIN 4
#define AUDIO_INPUT_PIN 5
#define NUM_LEDS 15
#define LEDS_PER_CHANNEL 5
#define ADC_MAX_VALUE 4095;

#define BASS_MAX_FREQUENCY 200
#define MID_MAX_FREQUENCY 4000

#define MIN_REFRESH_RATE_MILLISECONDS 100
#define LED_FADE_OUT_TIME_MILLISECONDS 400

// Signal magnitude when no audio source is connected.
// Samples with this magnitude (+- deviation) are considered noise and discarded.
#define NO_SIGNAL_MEAN_MAGNITUDE 1820
#define NO_SIGNAL_MAGNITUDE_DEVIATION 30

unsigned long last_loop_duration_milliseconds = MIN_REFRESH_RATE_MILLISECONDS;
uint8_t led_brightness_decrease_per_loop;

const uint16_t samples = 128; //This value MUST ALWAYS be a power of 2
const double samplingFrequency = 10000; //Hz, must be less than 10000 due to ADC
unsigned int sampling_period_us;
unsigned long sampling_microseconds;
unsigned long last_loop_start_milliseconds;

/*
These are the input and output vectors
Input vectors receive computed results from FFT
*/
double vReal[samples];
double vImag[samples];

/* Create FFT object */
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, samples, samplingFrequency);

Adafruit_NeoPixel pixels(NUM_LEDS, NEOPIXEL_DI_PIN, NEO_GRB + NEO_KHZ800);

uint8_t bass_leds[LEDS_PER_CHANNEL] = {0, 1, 2, 3, 4};
uint8_t mid_leds[LEDS_PER_CHANNEL] = {5, 6, 7, 8, 9};
uint8_t treble_leds[LEDS_PER_CHANNEL] = {10, 11, 12, 13, 14};

uint8_t leds_current_brightness[NUM_LEDS];

void setup() {
  sampling_period_us = round(1000000*(1.0/samplingFrequency));
  Serial.begin(115200);
  pinMode(NEOPIXEL_DI_PIN, OUTPUT);
  pinMode(AUDIO_INPUT_PIN, INPUT);
  pixels.begin();
}

void loop() {
  if (millis() - last_loop_start_milliseconds < MIN_REFRESH_RATE_MILLISECONDS) return;

  Serial.println("============");
  Serial.println("new loop run");
  Serial.println("============");
  last_loop_start_milliseconds = millis();

  /* SAMPLING */
  sampling_microseconds = micros();
  for(int i=0; i<samples; i++)
  {
      vReal[i] = analogRead(AUDIO_INPUT_PIN);
      vImag[i] = 0;
      while(micros() - sampling_microseconds < sampling_period_us){
        //empty loop
      }
      sampling_microseconds += sampling_period_us;
  }

  uint16_t sample_mean = 0;
  for (uint16_t i = 0; i < samples; i++) {
    sample_mean += vReal[i] / samples;
  }

  uint32_t sample_variance = 0;
  int16_t current_deviation_from_mean;
  for (uint16_t i = 0; i < samples; i++) {
    current_deviation_from_mean = vReal[i] - sample_mean;
    sample_variance += current_deviation_from_mean * current_deviation_from_mean / samples;
  }

  uint16_t sample_standard_deviation = sqrt(sample_variance);

  printf("sample_mean %d\n", sample_mean);
  printf("sample_variance %lu\n", sample_variance);
  printf("sample_standard_deviation %d\n", sample_standard_deviation);

  /* Fourier transform */
  FFT.complexToMagnitude();
  
  /* Compute bass, mid, treble magnitudes */
  uint32_t max_measured_frequency = samplingFrequency / 2;
  uint16_t frequency_step = samplingFrequency / samples;
  uint16_t num_measured_bass_frequencies = BASS_MAX_FREQUENCY / frequency_step + 1;
  uint16_t num_measured_mid_frequencies = (MID_MAX_FREQUENCY - BASS_MAX_FREQUENCY) / frequency_step + 1;
  uint16_t num_measured_treble_frequencies = (max_measured_frequency - MID_MAX_FREQUENCY) / frequency_step;

  uint16_t bass_average_magnitude = 0;
  uint16_t mid_average_magnitude = 0;
  uint16_t treble_average_magnitude = 0;
  uint16_t all_freq_average_magnitude = 0;
  uint16_t min_magnitude = ADC_MAX_VALUE;
  uint16_t max_magnitude = 0;
  uint16_t counted_num_bass_frequencies = 0;
  uint16_t counted_num_mid_frequencies = 0;
  uint16_t counted_num_treble_frequencies = 0;

  uint16_t sample_frequency;
  uint16_t sample_magnitude;

  for (uint16_t i = 0; i < samples >> 1; i++) {
    sample_frequency = i * frequency_step;
    sample_magnitude = vReal[i];

    if (sample_magnitude < min_magnitude) {
      min_magnitude = sample_magnitude;
    }

    if (sample_magnitude > max_magnitude) {
      max_magnitude = sample_magnitude;
    }

    all_freq_average_magnitude += sample_magnitude / (samples >> 1);
    if (sample_frequency < BASS_MAX_FREQUENCY) {
      bass_average_magnitude += sample_magnitude / num_measured_bass_frequencies;
      counted_num_bass_frequencies++;
    } else if (sample_frequency < MID_MAX_FREQUENCY) {
      mid_average_magnitude += sample_magnitude / num_measured_mid_frequencies;
      counted_num_mid_frequencies++;
    } else {
      treble_average_magnitude += sample_magnitude / num_measured_treble_frequencies;
      counted_num_treble_frequencies++;
    }
  }

  printf("max_measured_frequency %d\n", max_measured_frequency);
  printf("num_measured_bass_frequencies %d\n", num_measured_bass_frequencies);
  printf("num_measured_mid_frequencies %d\n", num_measured_mid_frequencies);
  printf("num_measured_treble_frequencies %d\n", num_measured_treble_frequencies);
  printf("counted_num_bass_frequencies %d\n", counted_num_bass_frequencies);
  printf("counted_num_mid_frequencies %d\n", counted_num_mid_frequencies);
  printf("counted_num_treble_frequencies %d\n", counted_num_treble_frequencies);

  printf("bass_average_magnitude %d\n", bass_average_magnitude);
  printf("mid_average_magnitude %d\n", mid_average_magnitude);
  printf("treble_average_magnitude %d\n", treble_average_magnitude);
  printf("avg all freq %d\n", all_freq_average_magnitude);
  printf("min_magnitude %d\n", min_magnitude);
  printf("max_magnitude %d\n", max_magnitude);

  uint16_t magnitude_range = max_magnitude - min_magnitude;
  uint16_t bass_magnitude_normalized = bass_average_magnitude - min_magnitude;
  uint16_t mid_magnitude_normalized = mid_average_magnitude - min_magnitude;
  uint16_t treble_magnitude_normalized = treble_average_magnitude - min_magnitude;

  printf("magnitude_range %d\n", magnitude_range);
  printf("bass_magnitude_normalized %d\n", bass_magnitude_normalized);
  printf("mid_magnitude_normalized %d\n", mid_magnitude_normalized);
  printf("treble_magnitude_normalized %d\n", treble_magnitude_normalized);

  /* Compute how many LEDs to light up based on magnitudes */
  uint16_t magnitude_per_led = magnitude_range / LEDS_PER_CHANNEL;
  uint8_t num_bass_leds_to_light_up = bass_magnitude_normalized / magnitude_per_led;
  uint8_t num_mid_leds_to_light_up = mid_magnitude_normalized / magnitude_per_led;
  uint8_t num_treble_leds_to_light_up = treble_magnitude_normalized / magnitude_per_led;

  int16_t deviation_from_no_signal_mean = sample_mean - NO_SIGNAL_MEAN_MAGNITUDE;
  printf("deviation_from_no_signal_mean %d\n", deviation_from_no_signal_mean);
  if (abs(deviation_from_no_signal_mean) <= NO_SIGNAL_MAGNITUDE_DEVIATION) {
    num_bass_leds_to_light_up = 0;
    num_mid_leds_to_light_up = 0;
    num_treble_leds_to_light_up = 0;
  }

  printf("magnitude_per_led %d\n", magnitude_per_led);
  printf("num_bass_leds_to_light_up %d\n", num_bass_leds_to_light_up);
  printf("num_mid_leds_to_light_up %d\n", num_mid_leds_to_light_up);
  printf("num_treble_leds_to_light_up %d\n", num_treble_leds_to_light_up);


  /* Light up LEDs */
  pixels.clear();

  unsigned long last_refresh_duration = last_loop_duration_milliseconds > MIN_REFRESH_RATE_MILLISECONDS ? last_loop_duration_milliseconds : MIN_REFRESH_RATE_MILLISECONDS;

  led_brightness_decrease_per_loop = last_refresh_duration * 0xff / LED_FADE_OUT_TIME_MILLISECONDS;

  for (int i = 0; i < NUM_LEDS; i++) {
    if (leds_current_brightness[i] >= led_brightness_decrease_per_loop) {
      leds_current_brightness[i] -= led_brightness_decrease_per_loop;
    } else {
      leds_current_brightness[i] = 0;
    }
  }

  for (int i = 0; i < LEDS_PER_CHANNEL; i++) {
    if (i < num_bass_leds_to_light_up) {
      leds_current_brightness[bass_leds[i]] = 0xff;
    }
    pixels.setPixelColor(bass_leds[i], pixels.Color(leds_current_brightness[bass_leds[i]], 0, 0));
  }

  for (int i = 0; i < LEDS_PER_CHANNEL; i++) {
    if (i < num_mid_leds_to_light_up) {
      leds_current_brightness[mid_leds[i]] = 0xff;
    }
    pixels.setPixelColor(mid_leds[i], pixels.Color(0, leds_current_brightness[mid_leds[i]], 0));
  }

  for (int i = 0; i < LEDS_PER_CHANNEL; i++) {
    if (i < num_treble_leds_to_light_up) {
      leds_current_brightness[treble_leds[i]] = 0xff;
    }
    pixels.setPixelColor(treble_leds[i], pixels.Color(0, 0, leds_current_brightness[treble_leds[i]]));
  }

  last_loop_duration_milliseconds = millis() - last_loop_start_milliseconds;
  printf("last_loop_duration_milliseconds %lu\n", last_loop_duration_milliseconds);

  pixels.show();
}

