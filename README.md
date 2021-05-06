# DIY-Battery-Monitor V1.00/19.04.2021, V2.00/03.05.2021
Hi, this is the repository of my DIY- Battery Monitor Project shown on my YouTube Channel: Roland W
First Video, Overview https://youtu.be/2_O4ZlxBaak

Please check the Manual.pdf

The Battery Monitor is set up for a Lithium Polymer Pack, but can be used for other chemistries as well.
Specific Voltage curves would have to be entered. 

The Monitor is using any Micro Controller, with a couple of mostly I2C devices. If your MC doesn't have on-board WiFi, you can use a ESP-01 module for it.
Modules:
- ADS1115 ADC for measuring Battery Voltage via a Voltage Divider 20k/1,680k (max 59V Bus)
- INA219 Shunt sensor, only used for Shunt Voltage measurement!
- LCD Display (I am using a 1602-I2C (V1), you could use a 2004-I2C (V2) for more convenience)

Logic: Voltage based prediction of SOC, corrected by Current measured on the Battery-Bus. Counting of real
Charging Cycles, V2: dedicated Discharge Test Mode

Values displayed are:
- Total Pack Voltage
- Charge and Discharge Current
- SOC
- Actual Power on Battery
- Internet Connection
- 2 Temperature Probes
- Cycle Counter
- Whs charged during actual cycle
- Ahs and Whs during discharge
- predicted battery capacity during discharge
- Millis Counter

WiFi and remote Monitoring using Blynk-App
