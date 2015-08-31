I2S_test
==================================

app_i2s_test_linux is an application used to test Xylon logiI2S IP core ALSA driver. 

I2S_test main functions are:
 - Initialise the sound codec adau1761 (located on zed board)
 - Call external play/record application (aplay/arecord)

Building 
-------------

To build this application, you need to do the following:

1. Make sure `CC` variable in Makefile corresponds
   to the installed cross-compiler.


2. Make sure cross-compiler set in `CC` variable is added accessible from the
   command line where building of the application will be performed. If using
   Vivado 2014.4 SDK execute following command.

    bash> source /opt/Xilinx/SDK/2014.4/settings32.sh


3. To build the application execute:

    bash> make

   Copy resulting executable to the SD card.


4. Run binary on the target

    bash> /mnt/app_i2s_test_linux /mnt/test_stereo_44100Hz_16bit_PCM.wav -twav        # Play the test_stereo_44100 sound
    bash> /mnt/app_i2s_test_linux /tmp/sound.wav -twav -r48000 -d10                   # Record a test sound for 10 seconds in 48kH sampling rate
