# Review
Good job it builds!

## Implementation

1. use mcu.usb_if instead of going to io directly
2. use USB2_0TypeCHorizontalConnector insead of HorizontalUSBTypeCConnector_model
3. Where are the LEDs? Find actual leds to use
    3.1 PowerLED_driver is weird, just use the LED (or LEDIndicator) from the standard library 

## Spec
1. since we have the led driver, might as well expose the unused leds in hiveio