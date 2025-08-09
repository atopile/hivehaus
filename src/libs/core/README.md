# Hivehaus Core

An ato library module containing the microcontroller and required infrastructure that all Hivehaus products import and build upon.

## 📋 Specification

### Overview
The Hivehaus Core is a reusable ato library module that products import to get the common foundation needed for all Hivehaus smart home products. When a product imports the Core, it gets a complete ESP32-C3 subsystem with power management, wireless connectivity, and standardized interfaces ready to connect to product-specific peripherals.

### Architecture Principles
- **Single PCB Design**: Products import Core as a library, everything on one board
- **Modularity**: Core functions cleanly separated from product-specific features
- **ESPHome First**: Designed for ESPHome configuration and management
- **Manufacturing**: Optimized for JLCPCB assembly with common parts
- **Consistency**: All products share the same core architecture

## 🔧 Components

### 1. Microcontroller
- **ESP32-C3-MINI-1** module
  - WiFi 2.4GHz (802.11 b/g/n)
  - Bluetooth 5.0 LE
  - 4MB Flash (integrated)
  - 400KB SRAM
  - RISC-V single-core 160MHz
  - 40MHz crystal (integrated)

### 2. Power System
- **USB-C**: Always present for programming and default power
- **Power Supply Module**: Default LDO module (can be retyped)
  - Input: VBUS from USB-C
  - Output: 5V and 3.3V rails
- **Protection**: ESD on USB and exposed interfaces
- **Extensible**: Products can retype `core.power_supply` module

### 3. User Interface
- **Buttons**:
  - BOOT button (GPIO9) - Also usable as user input after boot
  - RESET button (EN pin)
- **LEDs**:
  - Power LED (connected directly to 3.3V rail with resistor)
  - Status LED (SK6805 addressable RGB LED with data line on GPIO9, exposed on HiveIO.led_data)

### 4. Debug & Programming
- **USB Serial/JTAG**: Built-in ESP32-C3 USB controller (no external chip needed)
  - GPIO18: USB D-
  - GPIO19: USB D+
- **Debug Header**: 10-pin Saleae-compatible on breakaway tab
  - I2C test points
  - SPI test points
  - Power monitoring points
- **Test Points**: Key signals accessible for probing

### 5. Expansion Interfaces
- **HiveIO**: Flexible interface for product-specific needs
  - I2C (GPIO5/6): Shared I2C bus (package default pins)
  - SPI (GPIO7/8/10): SPI bus - MOSI/MISO/SCLK (package defaults)
  - UART (GPIO20/21): Hardware UART - RX/TX
  - GPIO0: ADC capable, general IO
  - GPIO1: ADC capable, general IO
  - GPIO2: ADC capable, general IO (boot strap pin)
  - GPIO3: ADC capable, general IO
  - GPIO4: ADC capable, general IO
  - LED Data (GPIO9): Data output from addressable RGB LED (can be chained or reassigned)
- **HiveExpansion Connector**:
  - STEMMA QT / Qwiic compatible (same as Adafruit/SparkFun standard)
  - I2C interface (connected to shared I2C bus from HiveIO)
  - 3.3V power and ground
  - JST-SH 4-pin connector (standard pinout: GND, 3V3, SDA, SCL)

## 📌 ESP32-C3-MINI-1 Pin Mapping

### Complete GPIO Allocation
| GPIO | Core Usage | HiveIO Access | Package Pin Name | Notes |
|------|------------|---------------|------------------|-------|
| GPIO0 | Free | ✅ io0 | IO0 | ADC1_CH0 |
| GPIO1 | Free | ✅ io1 | IO1 | ADC1_CH1 |
| GPIO2 | Free | ✅ io2 | IO2 | ADC1_CH2, Boot strap |
| GPIO3 | Free | ✅ io3 | IO3 | ADC1_CH3 |
| GPIO4 | Free | ✅ io4 | IO4 | ADC1_CH4, JTAG TMS |
| GPIO5 | I2C SDA | ✅ i2c | IO5 | ADC2_CH0, I2C default |
| GPIO6 | I2C SCL | ✅ i2c | IO6 | I2C default, JTAG TCK |
| GPIO7 | SPI MOSI | ✅ spi | IO7 | SPI default, JTAG TDO |
| GPIO8 | SPI MISO | ✅ spi | IO8 | SPI default, Boot strap |
| GPIO9 | RGB LED Data | ✅ led_data | IO9 | Addressable LED data out |
| GPIO10 | SPI SCLK | ✅ spi | IO10 | SPI default |
| GPIO11-17 | Not Available | ❌ | - | Internal SPI Flash |
| GPIO18 | USB D- | ❌ Core | IO18 | USB_D- |
| GPIO19 | USB D+ | ❌ Core | IO19 | USB_D+ |
| GPIO20 | UART TX | ✅ uart | TXD0 | U0TXD |
| GPIO21 | UART RX | ✅ uart | RXD0 | U0RXD |

Note: The ESP32_C3_MINI_1_driver in the package also includes:
- Boot and reset buttons with debounce capacitors
- Boot mode pull-up resistors on GPIO2 and GPIO8
- Reset pull-up resistor on EN pin
- Decoupling capacitors (22µF bulk + 100nF)

### Boot Strapping Pins
- **GPIO2**: Floating (SPI boot by default)
- **GPIO8**: Floating or HIGH for SPI boot
- **GPIO9**: HIGH for normal boot, LOW for download mode

## 📐 Design Guidelines for Products

### When Using the Core
Products that import the Core module should follow these guidelines:

### Required Board Area
- **ESP32-C3-MINI-1 Module**: 13.2mm x 16.6mm x 2.4mm (height)
- **Core Section Total**: Reserve ~20x20mm for module and support components
- **Antenna Keepout**: 15x7mm keepout area for PCB antenna (no copper on any layer)
- **USB Section**: Reserve space for USB-C connector at board edge
- **Debug Section**: Optional breakaway tab (10x30mm) for debug header

### Important Design Notes
- **ESP32-C3-MINI-1**: Has integrated 40MHz crystal and 4MB flash
- **Boot Strapping**: GPIO2, GPIO8, GPIO9 have boot-time functions (handled by Core)
- **Ground Plane**: Solid ground plane recommended under ESP32 module
- **Decoupling**: Place 100nF caps close to each VDD pin

### Standard Connectors
- **USB-C**: Included in Core for power and programming (can be retyped)
- **JST-SH 4-pin**: HiveExpansion (STEMMA QT/Qwiic compatible)
  - Pin 1: GND (Black wire)
  - Pin 2: 3.3V (Red wire)
  - Pin 3: SDA (Blue wire)
  - Pin 4: SCL (Yellow wire)
- **Debug Header**: 10-pin 0.1" header on breakaway section (optional)

## 🔌 Module Interfaces

### What Products Get When Importing Core

```ato
from "hivehaus/core/main.ato" import Core

module MyProduct:
    # Instantiate the core (includes USB-C and default LDO power supply)
    core = new Core
    
    # Use the exposed interfaces
    my_sensor.i2c ~ core.expansion.i2c
    my_button ~ core.hiveio.io0
    
    # Example: Replace power supply with battery management
    # from "packages/battery-manager.ato" import BatteryPowerSupply
    # core.power_supply -> BatteryPowerSupply
```

### Exposed Interfaces from Core Module

```ato
module Core:
    # Microcontroller
    mcu = new ESP32_C3_MINI_1_driver
    
    # USB-C always present (for programming and power)
    usb = new USBCConn      # Always populated, never retyped
    
    # Power supply module (can be retyped by products)
    power_supply = new DefaultPowerSupply
    
    # Power rails (outputs from power_supply)
    power_5v = new ElectricPower     # 5V ±5%
    power_3v3 = new ElectricPower    # 3.3V ±5%
    
    # Internal connections:
    # usb.power ~ power_supply.vbus_in
    # usb.dp ~ mcu.GPIO19  # USB D+ direct to ESP32-C3
    # usb.dm ~ mcu.GPIO18  # USB D- direct to ESP32-C3
    # power_supply.power_5v ~ power_5v
    # power_supply.power_3v3 ~ power_3v3
    # power_3v3 ~ mcu.power
    
    # Internal: RGB LED controller on shared I2C bus (GPIO4/5)
    
    # Product interfaces via HiveIO
    hiveio = new HiveIO     # All product-accessible interfaces
    
    # STEMMA QT/Qwiic expansion
    expansion = new HiveExpansion  # I2C from HiveIO + 3.3V power

# Base power supply interface that all implementations must follow
module HiveCorePowerSupply:
    vbus_in = new ElectricPower   # Input from USB-C VBUS
    power_5v = new ElectricPower  # 5V output rail
    power_3v3 = new ElectricPower # 3.3V output rail

# Default implementation: Simple LDO from USB
module DefaultPowerSupply from HiveCorePowerSupply:
    # Internal: Pass-through 5V from USB
    # Internal: LDO regulator 5V to 3.3V

interface HiveIO:
    # Shared I2C bus (products and Core RGB LED controller)
    i2c = new I2C           # GPIO5/6 (SDA/SCL - package defaults)
    
    # Product SPI bus  
    spi = new SPI           # GPIO7/8/10 (MOSI/MISO/SCLK - package defaults)
    
    # Product UART bus
    uart = new UART         # GPIO20/21 (RX/TX - package defaults)
    
    # Flexible GPIOs (all ADC capable)
    io0 = new ElectricLogic # GPIO0 - ADC1_CH0
    io1 = new ElectricLogic # GPIO1 - ADC1_CH1
    io2 = new ElectricLogic # GPIO2 - ADC1_CH2, boot strap
    io3 = new ElectricLogic # GPIO3 - ADC1_CH3
    io4 = new ElectricLogic # GPIO4 - ADC1_CH4
    
    # Power reference
    power = new ElectricPower

interface HiveExpansion:
    i2c = new I2C               # Same as hiveio.i2c (GPIO5/6)
    power = new ElectricPower   # 3.3V for STEMMA QT/Qwiic devices
```

### Internal Core Connections (Handled Automatically)
- ESP32-C3-MINI-1 module with all power/ground
- USB-C connector (always present) with ESD protection
- USB data lines direct to ESP32-C3 (GPIO18/19)
- Power supply module (default: LDO, retypeable)
- BOOT button on GPIO9
- RESET button on EN
- Pull-up resistors on I2C buses
- Decoupling capacitors

### Power Supply Interface Contract
All power supply modules must inherit from `HiveCorePowerSupply`:
- `vbus_in`: Input from USB-C VBUS
- `power_5v`: 5V output rail  
- `power_3v3`: 3.3V output rail

This ensures retype compatibility and consistent interfaces.

## 🔄 Power Architecture Flexibility

### Default Power Configuration
The Core has a two-tier power architecture:
- **USB-C connector**: Always present for programming and default power
- **Power Supply Module**: Default LDO implementation (retypeable)
- **Power outputs**: 5V and 3.3V rails available to products

### Customizing Power Architecture
Products retype `core.power_supply` to change power management:

```ato
# Battery Power Supply Module  
module BatteryPowerSupply from HiveCorePowerSupply:
    # Inherited: vbus_in, power_5v, power_3v3
    
    # Internal components
    charger = new BatteryCharger
    battery = new LiPoBattery
    power_mux = new PowerMux
    ldo_3v3 = new LDO_3V3
    
    # Internal connections
    vbus_in ~ charger.vbus
    charger.vbat ~ battery.power
    charger.vsys ~ power_mux.input_a
    battery.power ~ power_mux.input_b
    power_mux.output ~ power_5v
    power_5v ~ ldo_3v3.vin
    ldo_3v3.vout ~ power_3v3

# Using in a product
module BatteryProduct:
    core = new Core
    core.power_supply -> BatteryPowerSupply
```

```ato
# Buck Converter Power Supply (for higher voltage input)
module BuckPowerSupply from HiveCorePowerSupply:
    # Inherited: vbus_in, power_5v, power_3v3
    
    # Additional input for high voltage
    vin_high = new ElectricPower   # 12-24V input
    
    # Buck converters
    buck_5v = new BuckConverter_5V
    buck_3v3 = new BuckConverter_3V3
    
    # Use high voltage input, ignore USB
    vin_high ~ buck_5v.vin
    buck_5v.vout ~ power_5v
    vin_high ~ buck_3v3.vin  
    buck_3v3.vout ~ power_3v3

# Product with external power
module HighVoltageProduct:
    core = new Core
    core.power_supply -> BuckPowerSupply
    
    # Connect external power
    barrel_jack = new DCBarrelJack
    barrel_jack.power ~ core.power_supply.vin_high
```

### Power States
- **Active**: Full operation, WiFi/BT active (~80mA)
- **Light Sleep**: CPU idle, peripherals active (~15mA)
- **Deep Sleep**: RTC only (<10µA)
- **Power control**: Managed via ESPHome configuration

## 🧪 Test Strategy

### Manufacturing Tests
- Boundary scan for connectivity
- Power rail validation
- RF performance verification
- Flash programming verification

### Functional Tests
- WiFi/Bluetooth connectivity
- USB enumeration and data transfer
- GPIO functionality
- Sleep mode current consumption

## 📊 Performance Targets

- **Power Consumption**:
  - Active WiFi: <80mA @ 3.3V
  - BLE advertising: <15mA @ 3.3V
  - Deep sleep: <10µA
- **Boot Time**: <2 seconds to user code
- **WiFi Range**: >20m indoor typical
- **Operating Temp**: -20°C to +70°C

## 🏭 Manufacturing Considerations

- **Assembly**: Single-sided placement preferred
- **Components**: JLCPCB basic/preferred parts where possible
- **Testability**: Built-in self-test firmware
- **Panelization**: V-score compatible design

## 🏠 ESPHome Integration

### Base Configuration Template
```yaml
esphome:
  name: hivehaus-core
  platform: ESP32
  board: esp32-c3-devkitm-1

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  encryption:
    key: !secret api_encryption_key

ota:
  password: !secret ota_password

# Single I2C bus (shared by Core and products)
i2c:
  id: i2c_bus
  sda: GPIO5
  scl: GPIO6
  scan: true
  frequency: 100kHz

# Core status LED (I2C RGB controller on shared bus)
light:
  - platform: pca9685
    id: rgb_controller
    address: 0x60  # Fixed address for Core RGB LED
    i2c_id: i2c_bus
    
  - platform: rgb
    name: "Status LED"
    red: rgb_controller_0
    green: rgb_controller_1
    blue: rgb_controller_2

# SPI bus (package defaults)
spi:
  clk_pin: GPIO10
  mosi_pin: GPIO7
  miso_pin: GPIO8

# Product UART (package defaults)
uart:
  tx_pin: GPIO21
  rx_pin: GPIO20
  baud_rate: 115200

# Flexible HiveIO pins (product-specific)
# All 5 GPIOs available for any use
binary_sensor:
  - platform: gpio
    pin: GPIO0
    name: "HiveIO Input 0"
    
output:
  - platform: gpio
    pin: GPIO1
    id: hiveio_output_1
    
sensor:
  - platform: adc
    pin: GPIO2
    name: "HiveIO ADC 2"
    attenuation: 11db
  - platform: adc
    pin: GPIO3
    name: "HiveIO ADC 3"
    attenuation: 11db
  - platform: adc
    pin: GPIO4
    name: "HiveIO ADC 4"
    attenuation: 11db
```

### Fixed Pin Assignments
Core reserved pins (not accessible to products):
- **GPIO18/19**: USB Serial/JTAG (built-in USB programming)
- **GPIO9**: BOOT button

### Product Accessible Pins (via HiveIO)
- **GPIO5/6**: I2C bus (SDA/SCL - shared with Core RGB LED at address 0x60)
- **GPIO7/8/10**: SPI bus (MOSI/MISO/SCLK)
- **GPIO20/21**: UART bus (RX/TX)
- **GPIO0-4**: 5 flexible GPIOs (all ADC capable)

### STEMMA QT/Qwiic Compatibility
The HiveExpansion connector follows the standard pinout used by:
- Adafruit STEMMA QT
- SparkFun Qwiic
- Grove (with adapter)

This allows direct connection to thousands of existing I2C sensors and modules without custom adapters.

### Power Management in ESPHome
```yaml
# Deep sleep configuration
deep_sleep:
  run_duration: 60s
  sleep_duration: 5min
  wakeup_pin: GPIO9  # BOOT button as wake source
```
