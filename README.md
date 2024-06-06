### Spoof a healthy BCM/BNS on the R230 CAB-B bus to extinguish the read battery warning from the instrument panel.

## Green Board
![Green Board](res/green.png)

The boards arrive with 120ohm terminating resistors on the CAN bus. Remove the resistors or remove any solder blob which enables them. Be sure to connect CAN 1 to the CAN-B bus. CAN 2 goes unused in this implementation.

## Programming

Use STM32Prg to flash the firmware. Use the `CANFilterBNSEmulator.bin` file in the `firmware` directory.

Connect an FTDI USB serial interface to the pads as seen in the photo.

Make certain the Boot0 pin is getting 3.3V. This will put the MCU into bootloader mode.

Before disconnecting power from the board after flashing, disconnect Boot0 first! This is very important. Failing to do this will wipe the recently flashed firmware image. 

![Serial Connection](res/program.png)

![stm32prg](res/stm32prg.png)

The first time you press connect, you may see this error. Keep trying. I always see it at least once.

![error](res/error.png)
