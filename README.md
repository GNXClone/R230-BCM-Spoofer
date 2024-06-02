### Spoof a healthy BCM/BNS on the R230 CAB-B bus to extinguish the read battery warning from the instrument panel.

## Green Board
![Green Board](res/green.png)

## Programming

Connect an FTDI USB serial interface to the pads  as seen in the photo.
![Serial Connection](res/program.png)

Use STM32Prg to flash the firmware. 

![stm32prg](res/stm32prg.png)

The first time you press connect, you may see this error. Just keep trying. I always see it at least once.

![error](res/error.png)
