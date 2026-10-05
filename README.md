# All the Software Goodies for the Motor Test Stand

To use the serial loggers, simply download this repository as a zip, extract, and...
    On Windows, double click serial_logger.exe
    On Ubuntu, open in terminal, and run ./serial_logger -port YOURPORT

On Windows there could be some goofy shenanigans with serial ports and stuff, if data simply isn't coming through talk to Brylan

Currently, if the test stand is powered on and the throttle is not at 0, it will simply reboot over and over and over, you'll be able
to tell cause the esc/motor will keep making that kind of unhappy noise. If it's doing that, turn the throttle knob all the way to
the right, and it should initialize. 

## Windows serial comms issue
If your on windows, it's likely the esp32 will not be able to communicate over usb with your computer. To verify this, open device manager, 
and under Ports (COM & LPT) if a new one doesn't show up when you plug in the esp32, you're missing the driver.
To install the driver, please visit https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads
Under "Downloads" click on the most recent CP210x Universal Windows Driver and download it.
Unzip the downloaded folder, and inside that folder you should have a file called silabser, with type describing it as setup information.
Right click on it and select install. 
Once installed unplug and replug the esp32 and it should appear under comm ports.