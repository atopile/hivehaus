# Hivehaus Core Implementation Plan

## 🎯 Objective
Implement the Hivehaus Core as an ato library module that provides a standardized ESP32-C3 foundation for all smart home products.

## 📋 Implementation Strategy

### Phase 1: Research & Component Selection (Tasks 1-5)
**Goal**: Identify and validate all required atopile packages

#### Strategy:
1. Search atopile registry for existing packages
2. Verify JLCPCB part availability for each component
3. Check voltage/current ratings match our requirements
4. Prefer packages with proven track records

#### Components to Research:
- **USB-C Connector**: Need data + power, JLCPCB basic part preferred
- **LDO (5V→3.3V)**: 500mA+ current rating, low dropout
- **I2C RGB LED Controller**: PCA9685 or similar, address 0x60
- **ESD Protection**: TVS diodes for USB and exposed pins

### Phase 2: Core Module Structure (Tasks 6-10)
**Goal**: Create the base Core module with proper interfaces

#### Strategy:
1. Define interfaces first (HiveIO, HiveExpansion, HiveCorePowerSupply)
2. Create Core module importing ESP32_C3_MINI_1_driver
3. Wire power architecture with retypeable power_supply
4. Connect USB directly to ESP32-C3 (no external UART bridge)

#### Key Files:
- `main.ato`: Core module definition
- `interfaces.ato`: HiveIO and HiveExpansion interfaces
- `power.ato`: HiveCorePowerSupply and DefaultPowerSupply

### Phase 3: Pin Connections (Tasks 11-16)
**Goal**: Wire all ESP32-C3 pins according to specification

#### Strategy:
1. Use package default pin assignments (I2C on GPIO5/6, SPI on GPIO7/8/10)
2. Connect all interfaces through HiveIO
3. Add pull-up resistors on I2C bus
4. Keep BOOT/RESET buttons from ESP32_C3_MINI_1_driver

#### Pin Mapping:
```
I2C:   GPIO5/6   → hiveio.i2c
SPI:   GPIO7/8/10 → hiveio.spi  
UART:  GPIO20/21 → hiveio.uart
GPIOs: GPIO0-4   → hiveio.io0-4
USB:   GPIO18/19 → usb.dm/dp
```

### Phase 4: Core Peripherals (Tasks 17-19)
**Goal**: Add Core-specific components

#### Strategy:
1. RGB LED controller on shared I2C bus (address 0x60)
2. Power LED connected to 3.3V rail
3. HiveExpansion connector with STEMMA QT pinout

#### Components:
- PCA9685 or similar I2C LED driver
- Standard LEDs with current limiting resistors
- JST-SH 4-pin connector

### Phase 5: Protection & Quality (Tasks 20-21)
**Goal**: Add protection circuits and create examples

#### Strategy:
1. ESD protection on all exposed interfaces
2. Add test points for debugging
3. Create example product showing Core usage
4. Document best practices

### Phase 6: Testing & Validation (Tasks 22-24)
**Goal**: Verify the implementation works correctly

#### Strategy:
1. Run `ato build` to check for errors
2. Verify all components have valid JLCPCB parts
3. Check electrical rules (no shorts, proper connections)
4. Test ESPHome configuration compiles

## 📝 Detailed Todo List

### Research Phase
- [ ] 1. Find USB-C connector package with data lines (search: "usb-c", "usb-connectors")
- [ ] 2. Find LDO package for 5V→3.3V, 500mA+ (search: "ldo", "voltage regulator")
- [ ] 3. Find I2C RGB LED controller (search: "pca9685", "led driver", "i2c led")
- [ ] 4. Find ESD protection packages (search: "tvs", "esd", "protection")
- [ ] 5. Verify all packages have JLCPCB part numbers

### Implementation Phase
- [ ] 6. Create `interfaces.ato` with HiveIO and HiveExpansion definitions
- [ ] 7. Create `power.ato` with HiveCorePowerSupply base module
- [ ] 8. Implement DefaultPowerSupply with LDO
- [ ] 9. Update `main.ato` with Core module structure
- [ ] 10. Connect USB-C to power_supply and ESP32-C3

### Wiring Phase
- [ ] 11. Wire ESP32-C3 power with decoupling (already in driver)
- [ ] 12. Connect USB data lines to GPIO18/19
- [ ] 13. Wire I2C bus (GPIO5/6) to hiveio.i2c with pull-ups
- [ ] 14. Wire SPI bus (GPIO7/8/10) to hiveio.spi
- [ ] 15. Wire UART (GPIO20/21) to hiveio.uart
- [ ] 16. Wire GPIO0-4 to hiveio.io0-4

### Peripherals Phase
- [ ] 17. Add RGB LED controller on I2C bus
- [ ] 18. Add power LED with resistor
- [ ] 19. Create HiveExpansion module with JST-SH connector

### Protection Phase
- [ ] 20. Add ESD protection to USB-C
- [ ] 21. Add ESD protection to HiveIO/HiveExpansion pins

### Example Phase
- [ ] 22. Create `example.ato` showing basic Core usage
- [ ] 23. Create battery-powered example with power_supply retype
- [ ] 24. Create product example with sensors on HiveExpansion

### Testing Phase
- [ ] 25. Run `ato build` and fix any errors
- [ ] 26. Verify JLCPCB compatibility
- [ ] 27. Create test ESPHome configuration
- [ ] 28. Document pin mappings and usage

## 🚀 Getting Started

1. Start with research phase - find all packages first
2. Create interfaces before implementations
3. Build incrementally - test after each major addition
4. Use `ato build` frequently to catch errors early

## 📦 Expected Deliverables

1. **Core Library Module** (`/src/libs/core/`)
   - `main.ato` - Core module definition
   - `interfaces.ato` - HiveIO/HiveExpansion interfaces  
   - `power.ato` - Power supply modules
   - `example.ato` - Usage examples

2. **Documentation**
   - `README.md` - Complete specification (already done)
   - `PLAN.md` - This implementation plan
   - Pin mapping tables
   - ESPHome configuration templates

3. **Tested Implementation**
   - Builds without errors
   - All components have JLCPCB parts
   - Example products demonstrate usage

## 🎓 Key Design Decisions

1. **Single I2C Bus**: Shared between Core and products (simpler)
2. **Package Defaults**: Use ESP32-C3 package default pin assignments
3. **No I2S**: Keep GPIOs flexible instead of audio-specific
4. **Retypeable Power**: Power supply module can be replaced
5. **STEMMA QT**: Standard connector for I2C expansion

## ⚠️ Critical Considerations

1. **I2C Address Conflicts**: RGB LED fixed at 0x60, products must avoid
2. **Boot Strapping**: GPIO2/8/9 have special boot functions
3. **Power Sequencing**: Ensure proper 3.3V rise time for ESP32-C3
4. **ESD Protection**: Critical for USB and exposed pins
5. **Antenna Keepout**: No copper near PCB antenna area

## 📅 Timeline Estimate

- **Phase 1**: 2-3 hours (research can be slow)
- **Phase 2**: 1-2 hours (module structure)
- **Phase 3**: 1 hour (wiring)
- **Phase 4**: 1 hour (peripherals)
- **Phase 5**: 30 minutes (protection)
- **Phase 6**: 1-2 hours (testing and debugging)

**Total**: 6-10 hours for complete implementation

## 🔄 Next Steps

1. Begin with USB-C connector package research
2. Set up development environment with `ato` CLI
3. Create basic file structure
4. Implement incrementally following the phases