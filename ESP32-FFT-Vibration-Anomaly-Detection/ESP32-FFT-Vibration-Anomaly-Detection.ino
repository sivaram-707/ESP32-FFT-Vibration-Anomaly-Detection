// ESP32 + ADXL345 with FFT and LED Alerts
// Libraries needed:
// 1. Adafruit ADXL345
// 2. arduinoFFT

#include <Wire.h> //I2c inter integrated circiut
#include <Adafruit_ADXL345_U.h>
#include "arduinoFFT.h"

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// FFT Configuration - OPTIMIZED FOR SPEED
#define SAMPLES 256          // Reduced from 512 for faster processing
#define SAMPLING_FREQ 200

double vReal[SAMPLES];
double vImag[SAMPLES];
unsigned int samplingIndex = 0;

ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// LED Configuration
#define RED_LED_PIN 10       // CHANGE TO YOUR RED LED PIN
#define GREEN_LED_PIN 11    // CHANGE TO YOUR GREEN LED PIN
#define BUZZER_PIN 21        // CHANGE TO YOUR BUZZER PIN

// Anomaly Detection Settings
#define FREQ_THRESHOLD 60.0      // Frequency threshold (Hz)
#define ANOMALY_DURATION 3000    // 3 seconds for LED alert
#define BUZZER_DURATION 10000    // 10 seconds for buzzer alert
#define NOISE_THRESHOLD 2.0

// Anomaly tracking
unsigned long anomalyStartTime = 0;
bool anomalyActive = false;
bool buzzerActive = false;
bool ledState = false;
bool buzzerState = false;
unsigned long lastLedToggle = 0;
unsigned long lastBuzzerToggle = 0;
const int LED_BLINK_RATE = 250;      // Blink every 250ms
const int BUZZER_BEEP_RATE = 500;    // Beep every 500ms

// Timing
unsigned long lastSampleTime = 0;
const unsigned long samplePeriod = 1000000 / SAMPLING_FREQ;
unsigned long startMillis = 0;

void setup() {
  Serial.begin(500000);
  delay(1000);
  
  // Initialize LED pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Initialize I2C
  Wire.begin(8, 9);
  Wire.setClock(400000);
  
  // Initialize ADXL345
  if(!accel.begin()) {
    Serial.println("ERROR: ADXL345 not detected!");
    blinkError();
    while(1);
  }
  
  accel.setRange(ADXL345_RANGE_16_G);
  accel.setDataRate(ADXL345_DATARATE_200_HZ);
  
  Serial.println("=== ESP32 Vibration Monitor with LED Alerts ===");
  Serial.println("ADXL345 Initialized!");
  
  Serial.println("\nAlert Settings:");
  Serial.println("  Frequency Threshold: " + String(FREQ_THRESHOLD) + " Hz");
  Serial.println("  LED Alert Duration: " + String(ANOMALY_DURATION/1000) + " seconds");
  Serial.println("  Buzzer Alert Duration: " + String(BUZZER_DURATION/1000) + " seconds");
  Serial.println("  RED LED: GPIO " + String(RED_LED_PIN));
  Serial.println("  GREEN LED: GPIO " + String(GREEN_LED_PIN));
  Serial.println("  BUZZER: GPIO " + String(BUZZER_PIN));
  Serial.println("\n--- CSV Data Log ---");
  Serial.println("Time(s),Frequency(Hz),Magnitude,Anomaly,Alert");
  
  delay(1000);
  startMillis = millis();
  lastSampleTime = micros();
  
  // Start with green LED on (safe state)
  digitalWrite(GREEN_LED_PIN, HIGH);
}

void loop() {
  unsigned long currentTime = micros();
  
  // Sample at precise intervals
  if (currentTime - lastSampleTime >= samplePeriod) {
    lastSampleTime = currentTime;
    
    // Get accelerometer event
    sensors_event_t event; 
    accel.getEvent(&event);
    
    float x = event.acceleration.x;
    float y = event.acceleration.y;
    float z = event.acceleration.z;
    
    // Calculate magnitude
    float magnitude = sqrt(x*x + y*y + z*z);
    
    // Store in FFT buffer
    vReal[samplingIndex] = magnitude;
    vImag[samplingIndex] = 0;
    samplingIndex++;
    
    // When buffer is full, perform FFT analysis
    if(samplingIndex >= SAMPLES) {
      performFFT();
      samplingIndex = 0;
      delay(100); // Reduced delay for faster output
    }
  }
  
  // Handle LED blinking
  updateLEDs();
}

void performFFT() {
  // Remove DC offset
  double mean = 0;
  for(int i = 0; i < SAMPLES; i++) {
    mean += vReal[i];
  }
  mean /= SAMPLES;
  
  for(int i = 0; i < SAMPLES; i++) {
    vReal[i] -= mean;
  }
  
  // Apply Hamming window
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  
  // Compute FFT
  FFT.compute(FFTDirection::Forward);
  
  // Calculate magnitudes
  FFT.complexToMagnitude();
  
  // Find peak (skip frequencies below 5 Hz)
  double maxMagnitude = 0;
  int maxIndex = 0;
  int minIndex = (int)(5.0 * SAMPLES / SAMPLING_FREQ);
  
  for(int i = minIndex; i < (SAMPLES/2); i++) {
    if(vReal[i] > maxMagnitude) {
      maxMagnitude = vReal[i];
      maxIndex = i;
    }
  }
  
  double peakFrequency = (maxIndex * 1.0 * SAMPLING_FREQ) / SAMPLES;
  
  // Only log if vibration is significant
  if(maxMagnitude > NOISE_THRESHOLD) {
    logData(peakFrequency, maxMagnitude);
  }
}

void logData(double frequency, double magnitude) {
  // Calculate elapsed time in seconds
  float elapsedSeconds = (millis() - startMillis) / 1000.0;
  
  // Check if frequency exceeds threshold
  bool isOverThreshold = (frequency > FREQ_THRESHOLD);
  
  // Anomaly state machine
  if (isOverThreshold) {
    if (!anomalyActive) {
      // Start tracking anomaly
      anomalyStartTime = millis();
      anomalyActive = true;
      buzzerActive = false;
    }
    
    unsigned long anomalyDuration = millis() - anomalyStartTime;
    
    // Check if buzzer should activate (after 10 seconds)
    if (anomalyDuration >= BUZZER_DURATION && !buzzerActive) {
      buzzerActive = true;
      Serial.println(">>> CRITICAL: Buzzer activated! <<<");
    }
    
    if (anomalyDuration >= ANOMALY_DURATION) {
      // Anomaly confirmed - activate RED LED (and buzzer after 10s)
      String alertStatus = buzzerActive ? "CRITICAL_BUZZER" : "ALERT_ACTIVE";
      
      // Print CSV row
      Serial.print(elapsedSeconds, 2);
      Serial.print(",");
      Serial.print(frequency, 2);
      Serial.print(",");
      Serial.print(magnitude, 2);
      Serial.print(",YES,");
      Serial.println(alertStatus);
      return;
    } else {
      // Anomaly detected but not confirmed yet
      String alertStatus = "WARNING_" + String((ANOMALY_DURATION - anomalyDuration)/1000) + "s";
      
      Serial.print(elapsedSeconds, 2);
      Serial.print(",");
      Serial.print(frequency, 2);
      Serial.print(",");
      Serial.print(magnitude, 2);
      Serial.print(",PENDING,");
      Serial.println(alertStatus);
      return;
    }
  } else {
    // Frequency is safe - reset anomaly tracking
    if (anomalyActive) {
      anomalyActive = false;
      buzzerActive = false;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println(">>> Anomaly cleared <<<");
    }
    
    // Print normal reading
    Serial.print(elapsedSeconds, 2);
    Serial.print(",");
    Serial.print(frequency, 2);
    Serial.print(",");
    Serial.print(magnitude, 2);
    Serial.print(",NO,");
    Serial.println("SAFE");
  }
}

void updateLEDs() {
  unsigned long currentMillis = millis();
  
  if (anomalyActive && (currentMillis - anomalyStartTime >= ANOMALY_DURATION)) {
    // ALERT STATE - Blink RED LED
    digitalWrite(GREEN_LED_PIN, LOW);
    
    if (currentMillis - lastLedToggle >= LED_BLINK_RATE) {
      lastLedToggle = currentMillis;
      ledState = !ledState;
      digitalWrite(RED_LED_PIN, ledState ? HIGH : LOW);
    }
    
    // BUZZER - Activate after 10 seconds
    if (buzzerActive) {
      if (currentMillis - lastBuzzerToggle >= BUZZER_BEEP_RATE) {
        lastBuzzerToggle = currentMillis;
        buzzerState = !buzzerState;
        digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
      }
    }
  } else {
    // SAFE STATE - Blink GREEN LED, buzzer off
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    
    if (currentMillis - lastLedToggle >= LED_BLINK_RATE) {
      lastLedToggle = currentMillis;
      ledState = !ledState;
      digitalWrite(GREEN_LED_PIN, ledState ? HIGH : LOW);
    }
  }
}

void blinkError() {
  // Blink both LEDs rapidly to indicate error
  while(1) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, HIGH);
    delay(200);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    delay(200);
  }
}