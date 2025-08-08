# Agents Guide for hivehaus

This document is for AI coding agents and collaborators working in this repo inside Cursor/Windsurf. It summarizes the project shape, the Ato DSL constraints, and how to use the Atopile MCP server to be productive without breaking things.

## What You’re Building

- Smart home hardware project using Atopile and the Ato DSL.
- Entry point defined in `ato.yaml` → build `default` targets `main.ato:App`.
- The editor extension (Cursor/Windsurf) invokes the Atopile CLI to build; you generally don’t run the CLI directly here.

## Smart Home Context

- Scope: Reusable building blocks for common smart home devices (power input/PMIC, processors/MCUs, radios, sensors, and I/O), plus a growing set of complete device examples.
- Targets: Designs intended to pair with firmware stacks like ESPHome (ESP32/ESP8266) and Zephyr-based projects (e.g., nRF52, ESP32 with Zephyr, etc.). Firmware isn’t authored in Ato; Ato captures the hardware structure so firmware teams can target consistent interfaces.
- Goals: Hardware reuse, consistent interfaces (I2C/SPI/UART/GPIO/PWM/ADC, power rails), and easy mapping from “device example” → firmware config or application.

Suggested library organization (non-binding; align with maintainers):

- Building blocks: MCUs, power stages, radio modules, sensor front-ends, IO drivers (relays, MOSFETs, triacs), connectors.
- Device examples: Concrete products like plugs, light switches, sensor nodes, thermostats, door/window sensors, etc.

Design tips:

- Keep blocks parameterized (e.g., `input_voltage: V`, `current_limit: A`, `i2c.address`).
- Expose standard interfaces (e.g., `I2C`, `SPI`, `ElectricPower`, `ElectricLogic`) for easy composition.
- Add `assert` statements to reflect firmware expectations (supply rails, pull-ups, logic levels, addresses, bus speeds).

## Repo Layout

- `main.ato`: project entry module (`module App`).
- `ato.yaml`: project config (paths, builds, atopile version).
- `layouts/`: place PCB layout files if/when generated.
- `.cursor/`: MCP server config and Ato rules for the editor.
- `CLAUDE.md`: reference for Ato grammar, examples, and library usage.

If/when the repo splits into libraries and examples, consider top-level folders such as `blocks/` (reusable modules/interfaces) and `examples/` (end devices). Until then, keep modules self-contained and clearly named.

## Ato DSL Constraints (very important)

- No control flow except experimental `for` loops: no `if`, no `while`.
- No functions, classes, objects, exceptions, or generators.
- Declarative only: no side effects, no imperative code.
- Organize code with `module`, `component`, and `interface` blocks.
- Define pins and signals, assign parameters, connect nets, apply traits, and assert constraints.

See detailed examples and full grammar in `CLAUDE.md` and `.cursor/rules/ato.mdc`.

## Productive Patterns

- Define modules and interfaces with clear pins and typed parameters.
- Instantiate modules via `new`, optionally with templates like `<int_=1>`.
- Connect with `~`, and use directed connections with `<~` and `~>` when needed.
- Use `assert` liberally (voltages, ranges, tolerances) to bound design intent.
- Use `trait` to annotate behavior/capabilities; prefer composition over retyping.
- Arrays of submodules are supported (`new Module[10]`); experimental `for` loops can iterate them if feature is enabled.

Smart home specific patterns:

- Power entry + regulation chain (e.g., AC/DC → DC/DC → rails), then MCU + radio + sensor buses.
- Standardize sensor busses (I2C for environmental sensors; SPI for high-speed devices) and expose connectors/pads where appropriate.
- For ESPHome-targeted designs, provide labeled GPIOs and power rails that map cleanly to ESPHome YAML; for Zephyr, align to common devicetree-friendly interfaces.

## Things Not To Do

- Don’t introduce procedural constructs (no conditionals, functions, or runtime state).
- Don’t write Python/JS inside `.ato` files—Ato is its own declarative language.
- Don’t depend on side effects; prefer explicit declarations, connections, and assertions.

## Using the Atopile MCP Server (inside Cursor)

Configured in `.cursor/mcp.json` as `atopile`. Use it to discover and inspect library building blocks instead of guessing.

- Discover interfaces: ask the MCP server for `get_library_interfaces`.
- Discover modules: ask the MCP server for `get_library_modules`.
- Inspect definitions: use `inspect_library_module_or_interface` to read code before instantiating.

Example agent intents (natural language to MCP):

- “List common power and logic interfaces.”
- “Show the API for `Resistor`, `Capacitor`, and `I2C`.”
- “Inspect `ElectricPower` and `ElectricLogic` to confirm pin naming.”

Reference snippets (from CLAUDE.md):

- `interface ElectricPower: hv = new Electrical; lv = new Electrical`
- `module Resistor: resistance: ohm; max_power: W; unnamed = new Electrical[2]`
- `interface I2C: scl = new ElectricLogic; sda = new ElectricLogic; frequency: Hz; address: dimensionless`

## Common Tasks

Add a simple module:

```ato
module SensorNode:
    vcc = new ElectricPower
    comm = new I2C
    pullup = new Resistor

    pullup.resistance = 4.7kohm
    # Connect I2C lines to power reference, etc., as needed
    comm.scl.reference ~ vcc
    comm.sda.reference ~ vcc
```

Instantiate and connect in `App`:

```ato
module App:
    power = new ElectricPower
    node = new SensorNode

    node.vcc ~ power
    assert power.hv within 3.0V to 3.6V
```

Array + experimental for-loop (if enabled via pragma):

```ato
#pragma experiment("FOR_LOOP")
module App:
    leds = new Resistor[4]
    for r in leds:
        r.resistance = 330ohm
```

## Build and Validate

- Build is driven by Cursor/Windsurf invoking the Atopile CLI.
- Project declares `requires-atopile: "0.11.1"` in `README.md`/`ato.yaml`.
- Default build entry is `main.ato:App` as set in `ato.yaml`.
- If you need a new top module, update `ato.yaml` accordingly.

Firmware pairing notes:

- ESPHome: After hardware stabilizes, provide a sample YAML (outside Ato) that maps exposed GPIOs, I2C addresses, and power rails to components.
- Zephyr: Provide guidance or a sample board overlay and Kconfig for the chosen MCU; keep hardware interfaces named to match typical Zephyr drivers.

## Conventions

- Prefer newline-terminated simple statements; use semicolons to group related short ops on one line.
- Use docstring-style strings at top of modules to explain intent.
- Keep names clear and typed parameters explicit (e.g., `resistance: ohm`).
- Put general language references in `CLAUDE.md`; keep `main.ato` tidy and minimal.
- Prefer names that hint at firmware mapping (e.g., `i2c`, `spi`, `uart`, `pwm`, `led`, `button`, `relay`), and keep power rails explicit (`vbat`, `vcc3v3`, `vcc5v`).

## When In Doubt

- Check `CLAUDE.md` for syntax and grammar details.
- Query the MCP server to discover correct library usage.
- Ask the user for electrical requirements (voltages, currents, interfaces) and capture them as `assert` statements.
- Consider the intended firmware stack (ESPHome or Zephyr) and ensure hardware interfaces and pinouts map cleanly to it.

## Quick Checklist

- Defined pins, signals, and types for every new module.
- Connected nets intentionally; used traits or retyping when needed.
- Added assertions for key electrical constraints.
- Kept to declarative constructs; no forbidden features.
- Updated `ato.yaml` if the entry point changed.
