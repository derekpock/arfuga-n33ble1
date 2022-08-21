# Arfuga-N33ble1

This is the companion code for the [arfuga](https://github.com/derekpock/arfuga) (https://github.com/derekpock/arfuga) Android repository. Take a look there to see what this is used for!

This code is for an [Arduino Nano 33 BLE](https://store.arduino.cc/products/arduino-nano-33-ble) device called "N33ble1" that is wired up to two momentary push buttons and installed into the dash of a vehicle.

N33ble1 uses Bluetooth LE (BLE) technology to advertise and transmit user-input events (through the momentary push buttons) to a companion app (called Arfuga). N33ble1 also accepts specific BLE write events to set the momentary button LED behaviors, as well as the less-visible on-board RGB LED and on-board external LED.

N33ble1 alone only provides the raw user input, described by a bit-field, to Arfuga. More complex processing is done with this information in the [arfuga](https://github.com/derekpock/arfuga) Android repository.

## Setup

This section is WIP. Bug me if you're interested!