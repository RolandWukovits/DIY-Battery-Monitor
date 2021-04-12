# DIY-Battery-Monitor, Code coming soon...
Hi, this is the repository of my DIY- Battery Monitor Project shown on my YouTube Channel: Roland W

The Battery Monitor is set up for a Lithium Polymer Pack, but can be used for other chemistries as well.
Specific Voltage curves would have to be entered. 

The Monitor is using any Micro Controller, with a couple of mostly I2C devices. If your MC doesn't have on-board WiFi, you can use a ESP-01 module for it.
Modules:
- ADS1115 ADC for measuring Battery Voltage via a Voltage Divider 20k/1,680k
- INA219 Shunt sensor, only used for Shunt Voltage measurement!
- LCD Display

Logic: Voltage based prediction of SOC, corrected by Current measured on the Battery-Bus. Counting of real
Charging Cycles

Values displayed are:
- Total Pack Voltage
- Charge and Discharge Current
- SOC
- Actual Power on Battery
- Internet Connection
- 2 Temperature Probes
- Cycle Counter
- kWhs charged during actual cycle
- Millis Counter

WiFi and remote Monitoring using Blynk-App
