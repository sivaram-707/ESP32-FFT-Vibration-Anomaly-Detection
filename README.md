# ESP32-Based Vibration Monitoring and Anomaly Detection Using FFT

## 1. Project Overview

This project implements a real-time vibration monitoring system using an ESP32 microcontroller and an ADXL345 three-axis accelerometer.

The system continuously collects acceleration data, processes the vibration signal using Fast Fourier Transform (FFT), and identifies abnormal vibration conditions based on a predefined frequency threshold.

An LED-based visual indication system and a buzzer provide alerts when abnormal vibration conditions persist for a specified duration.

The project aims to demonstrate the application of signal processing techniques in embedded systems for basic vibration monitoring and condition monitoring.

## 2. Objectives

- Acquire real-time three-axis acceleration data using the ADXL345 accelerometer.
- Calculate vibration magnitude from X, Y and Z acceleration measurements.
- Apply FFT to analyse vibration frequency components.
- Identify dominant vibration frequencies.
- Detect abnormal vibration conditions using a configurable frequency threshold.
- Implement visual and audible alerts using LEDs and a buzzer.
- Log vibration measurements in CSV format for further analysis.

## 3. Hardware Requirements

| Component | Purpose |
|---|---|
| ESP32 | Main microcontroller |
| ADXL345 | Three-axis accelerometer |
| Red LED | Abnormal condition indication |
| Green LED | Normal condition indication |
| Buzzer | Audible warning |
| Breadboard and jumper wires | Circuit connections |

## 4. Software Requirements

- Arduino IDE
- ESP32 board package
- Adafruit ADXL345 Unified library
- arduinoFFT library
- Python 3
- Required Python packages for the vibration logger

## 5. System Architecture

The system follows this processing sequence:

1. The ADXL345 measures acceleration along three axes.
2. The ESP32 calculates the acceleration magnitude.
3. The collected samples are stored in an FFT buffer.
4. DC offset is removed from the collected samples.
5. A Hamming window is applied to reduce spectral leakage.
6. FFT
