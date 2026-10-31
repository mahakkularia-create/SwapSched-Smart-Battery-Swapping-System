SWAPSCHED – FOUR-BAY HARDWARE + LAPTOP DASHBOARD

FILES
- SwapSched_Standalone.ino : upload this to Arduino UNO
- swapsched_dashboard.py  : live laptop dashboard
- README.txt               : setup instructions

HARDWARE PINS
- Relay IN1..IN4: D2, D4, D7, D8 (assumes active-LOW board)
- LED1..LED4: D3, D5, D6, D9; each LED has its own 220 ohm resistor
- Buzzer +: D12, buzzer -: GND (direct drive only if low-current 5V buzzer)
- DHT11 DATA: D11, VCC: 5V, GND: GND
- RTC DS1307: SDA A4, SCL A5, VCC 5V, GND GND
- I2C LCD: SDA A4, SCL A5, VCC 5V, GND GND

RUN IT
1. Keep relay COM/NO/NC screw terminals disconnected.
2. Confirm the relay board has a suitable regulated 5V supply. Do not assume UNO 5V can power four relay coils.
3. Upload SwapSched_Standalone.ino to the UNO.
4. Close Arduino Serial Monitor (only one program can use the COM port at a time).
5. Install Python serial package:
       py -m pip install pyserial
6. Run:
       py swapsched_dashboard.py
7. Select the Arduino COM port and click Connect.
8. Choose P2 or P3 from the dashboard dropdown. Default is P3.
9. To save received data, click Save received data as CSV.

HOW THE DEMO WORKS
- Four simulated battery/vehicle slots are shown as Vehicle 01..04.
- The model starts with four full packs, as the supplied C++ simulator does.
- Each simulated step is 15 minutes; one step occurs every 15 real seconds for a faster demo.
- Simulated arrivals consume ready packs; returned packs receive a simulated SoC between 10% and 30%.
- Scheduler chooses charging bays under a 1.5 kW demonstration cap (two 0.75 kW chargers).
- Green LEDs and relay coils indicate selected charging bays.
- RTC time is displayed as real clock time. Simulation time-of-day drives the demand/tariff model.
- LCD shows real RTC time + selected policy, then ready/waiting counts.
- Laptop shows each bay's simulated SoC, charging/waiting/full state, plus arrivals, served requests, stockouts, ready count, tariff, temperature and humidity.
- Send policy 2 or 3 from dashboard to change the policy.

MODEL LIMITATIONS / HONEST REPORTING
- This is a four-bay hardware demonstration adapted from a 12-bay C++ simulation.
- The source project uses 12 bays, 96 x 15-minute steps, 6 kW cap, 1.5 kWh packs, 0.75 kW chargers, 95% ready threshold, epsilon 0.05, and P0/P1/P2/P3 policies. This hardware demo reduces capacity to four bays and 1.5 kW so a maximum of two relays can be active.
- This firmware implements the P2 mean and P3 chance-constrained policy logic for the 4-bay demo. It does not implement P0/P1 or the full C++ experiment/CSV/CI pipeline.
- Vehicle IDs, SoC, arrivals, served counts, energy and tariff are simulated. They are not measured from real vehicles or batteries.
- DHT11 readings are physical environmental readings. RTC is physical clock time.
- ACS712 is not used. There are no real loads, so no real charging current or energy is measured.
- Relay contacts must remain disconnected. Do not connect actual batteries, chargers, mains AC, or household loads.
- The LCD driver assumes a common PCF8574 backpack mapping and address 0x27 or 0x3F. If LCD is blank, the backpack may use another mapping/address.
- Relay logic assumes active-LOW. If your relay module is active-HIGH, invert the ON/OFF writes in applyOutputs().
- The buzzer is driven directly from D12 only if its current is within the UNO GPIO limit; otherwise use a transistor driver.
- The simple DHT11 bit-bang routine assumes a 3-pin digital module. If it gives errors, use the DHT11 sensor's tested library-based read routine instead.
- USB is needed for the live laptop dashboard. The Arduino scheduling/LED/relay/LCD operation itself continues standalone after upload; the laptop display is an optional serial monitor.
