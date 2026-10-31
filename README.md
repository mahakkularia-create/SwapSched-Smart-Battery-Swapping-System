# SwapSched – Smart Battery Swapping & Scheduling System

**An Arduino-based, automated four-bay battery swapping and scheduling prototype.**

## 1. Project Overview

SwapSched is a smart battery swapping and scheduling system designed to manage battery availability across multiple charging bays.

The system uses scheduling algorithms to determine which battery bays should be charged, based on simulated battery State of Charge (SoC), customer demand, charging capacity and electricity tariff periods.

The prototype demonstrates automated scheduling using an Arduino UNO, a four-channel relay module, LEDs, an RTC module, an LCD display, a DHT11 sensor and a buzzer.

The system operates independently after uploading the firmware, without requiring a laptop to run its scheduling logic.

## 2. Project Objectives

* Demonstrate automated battery charging allocation.
* Simulate customer arrivals and battery swapping.
* Maintain battery availability using scheduling logic.
* Demonstrate capacity-constrained charging decisions.
* Display system information through an LCD.
* Provide visual and audio status indicators.
* Demonstrate RTC-based time awareness.

## 3. Hardware Components

| Component              | Purpose                               |
| ---------------------- | ------------------------------------- |
| Arduino UNO            | Main controller                       |
| 4-channel relay module | Controls four simulated charging bays |
| 4 Green LEDs           | Indicates charging status             |
| DS1307 RTC             | Provides real-time clock information  |
| 16x2 I2C LCD           | Displays system status                |
| DHT11                  | Measures temperature and humidity     |
| Active buzzer          | Provides audio alerts                 |
| LM2596 buck converter  | Steps down DC voltage                 |
| 220Ω resistors         | Limits LED current                    |
| 12V DC adapter         | External power source                 |

## 4. System Architecture

The system follows this sequence:

1. Arduino initializes all connected components.
2. RTC provides the current time.
3. The software initializes four simulated battery SoC values.
4. Customer arrivals are generated using a simulated demand model.
5. The scheduling algorithm estimates future battery demand.
6. Charging decisions are made according to the selected policy and simulated charging capacity.
7. Relays and LEDs indicate which bays are selected for charging.
8. The LCD displays time and operational statistics.
9. The buzzer provides alerts when simulated demand cannot be served.

## 5. Scheduling Logic

The current Arduino demonstration implements two scheduling policy modes:

**P2 – Mean Demand Policy**

* Estimates expected demand.
* Determines a target number of ready batteries.
* Selects charging bays according to battery readiness.

**P3 – Chance-Constrained Policy**

* Estimates demand over a future lead window.
* Calculates a target ready-battery quantity using a demand probability threshold.
* Prioritizes batteries that are closest to becoming fully charged.

The firmware starts in P3 mode.

The simulation uses:

* Four battery bays.
* 1.5 kWh simulated battery capacity.
* 0.75 kW simulated charging power per bay.
* 1.5 kW simulated charging capacity limit.
* 95% ready State of Charge threshold.
* Accelerated 15-minute simulation intervals.

These are demonstration parameters, not measurements from actual batteries or chargers.

## 6. Arduino Pin Configuration

| Component       | Arduino Pin |
| --------------- | ----------- |
| Relay 1         | D2          |
| Relay 2         | D4          |
| Relay 3         | D7          |
| Relay 4         | D8          |
| LED 1           | D3          |
| LED 2           | D5          |
| LED 3           | D6          |
| LED 4           | D9          |
| DHT11 Data      | D11         |
| Buzzer Positive | D12         |
| LCD SDA         | A4          |
| LCD SCL         | A5          |
| RTC SDA         | A4          |
| RTC SCL         | A5          |

LCD and RTC share the I2C communication pins.

## 7. Software Requirements

* Arduino IDE
* Arduino UNO board support
* USB cable for uploading firmware

The current standalone firmware uses the built-in `Wire.h` library and does not require additional external Arduino libraries.

## 8. Installation and Execution

1. Download the SwapSched Arduino firmware.
2. Extract the ZIP archive.
3. Open `SwapSched_Standalone.ino` in Arduino IDE.
4. Select Arduino UNO under Tools → Board.
5. Select the appropriate COM port.
6. Upload the program.
7. Open Serial Monitor at 9600 baud.

To switch scheduling modes:

* Send `2` to activate P2.
* Send `3` to activate P3.

The system then continues operating automatically.

## 9. Demonstration Outputs

The prototype demonstrates:

* Automatic selection of charging bays.
* Individual relay activation.
* Green LED charging indicators.
* RTC time display.
* Simulated battery SoC updates.
* Simulated customer arrivals.
* Ready battery count.
* Served and unserved customer statistics.
* Temperature and humidity readings.
* Buzzer alerts.

## 10. Important Prototype Limitations

This version is a functional scheduling demonstration, not a complete commercial battery swapping station.

* Battery SoC is simulated in software.
* Customer demand is generated mathematically.
* Charging power is simulated.
* No real battery charging takes place.
* LEDs represent charging status.
* Relay contacts are not connected to actual charging loads.
* DHT11 provides environmental readings, not battery temperature.
* The RTC provides real time, while the simulation advances on an accelerated clock.

The original C++ simulation and the Arduino hardware demonstration have different scales and implementation details.

## 11. Future Improvements

Potential future development includes:

* Real battery State of Charge monitoring.
* Current and voltage measurement.
* Actual charging hardware integration.
* More advanced demand forecasting.
* Integration of all scheduling policies.
* Mobile application and remote monitoring.
* Cloud-based battery swapping analytics.
* Expansion from four bays to a larger swapping station.

## 12. Safety

This prototype is intended for low-voltage educational demonstration.

* Use individual resistors with LEDs.
* Verify the LM2596 output before connecting it to the Arduino.
* Use an appropriately rated supply for the relay module.
* Keep relay COM, NO and NC terminals disconnected during the LED-only demonstration.
* Do not connect household mains electricity or actual battery charging loads.

## 13. Project Summary

SwapSched demonstrates how intelligent scheduling can improve battery availability in a battery swapping station.

By combining demand forecasting, simulated battery State of Charge, capacity-constrained scheduling and hardware-based indicators, the project provides a working foundation for developing a larger automated battery swapping management system.

**Project:** SwapSched
**Platform:** Arduino UNO
**Prototype:** Four-bay automated battery scheduling demonstration
**Control:** Standalone embedded firmware
