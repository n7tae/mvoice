# M17 Digital Voice

*M17 Digital Voice* , mvoice, is a fully functional, graphical repeater. It uses David Rowes Codec 2 and operates as a complete M17 repeater, only there is no RF component. It can Link to M17 reflectors and it can also do *routing*! It works best with a USB-based headset with microphone. mvoice uses the default pulseaudio/ALSA input and output device, so for most versions of linux, all you need to do is plug your headset in and you should be ready to go.

## Building on a Raspberry Pi

Raspberry Pi OS is not ready for *mvoice* out of the box. While Raspbian has ALSA, it also needs pulseaudio:

```bash
sudo apt install -y pulseaudio pavucontrol paprefs
```

It's proabably a good idea to reboot after installing pulseaudio. After it's installed, take a look at the output of `aplay -L`. Near the beginning is should say:

```bash
default
    Playback/recording through the PulseAudio sound server
```

You might also need to go to the ALSA audio configuration. For Debian Buster, this is found in main menu under **Preferences-->Audio Device Settings**. Select your headset in the drop-down list of devices, and then configure it and set it to be the default device. Set your speaker and microphone gain somewhere near the top. Once you build and configure *mvoice*, you can use the Echo feature to help you set these gains adjust the speaker volume of you headset for a comfortable level and adjust the mic gain for the loudest playback without clipping. For my setup, the playback (speaker) was near 100% and the mic gain was at about 50%.

## Building and installing

There are several library requirements before you start:

```bash
sudo apt install -y git build-essential libgtkmm-3.0-dev libasound2-dev libsqlite3-dev

```

Then you can download and build mvoice:

```bash
git clone git://github.com/n7tae/mvoice.git
cd mvoice
make
```

If it builds okay, then you can install it:

```bash
make install
```

All the configuration files are located in ~/etc and the mvoice executable is in ~/bin. Please note that symbolic links are installed, so you can do a `make clean` to get rid of the intermediate object files, but don't delete the build directory unless you want to remove *mvoice* from your system.

## Configuring mvoice

Plug in your headset and start *mvoice*: Open a shell and type `mvoice` if ~/bin is in your search path, otherwise, move to your home directory and type `bin/mvoice`. Most log messages will be displayed within the log window of mvoice, but a few messages, especially if there are problems, may also be printed in the shell you launch mvoice from.

Once it launches, click the **Settings** button and make sure to set your callsign and the codec setting on the M17 page. You can usually leave the audio settings on "default". Also enable IPv6 if your internet provider supports it. Click the Okay button and your settings will be saved in your ~/etc/ directory.

On the main window, set your destination callsign and IP address. Once you've entered valid values for both, you can save these for future use. Please note that you don't have to save a destination to use it, but it will not be available in the drop-down selection until you do save it. If you haven't saved a destination and you select another destination from the dropdown list, your unsaved destination will no longer be available. If you are going to use an M17 reflector, a vaild callsign is exactly **nine** characters long, `M17-xxx y` where `xxx` is made up of letters or numbers and `y` is the module, a letter from `A` to `Z`. Note that there is one space before the module. If you are callsign routing to an individual, you can provide a module in the ninth position, but it isn't necessary.

For Linking, you can select a reflector. The link button will only be activated after you have entered a valid target in the destination callsign and IP address. This validation **does not** check to see if the module you are requesting is actually configured and operational, or even if it actually exists.

## Firewall

*mvoice* can operate in linking or routing mode. If you want to be able to recieve incoming callsign routes, you'll need to port-forward UDP 17000 on your network firewall. If you can't, you can still connect to M17 reflectors. You can originate a direct, callsign route, but if there is a long pause in the QSO, you're firewall will shutdown the port until you key up again.

## Operating

There are three "transmitting" buttons on *mvoice*:

1) **Echo Test** is a toggle switch. Click it to start recording an echo test and your voice will be encoded using the Codec 2 mode you selected in the Settings dialog. Note that the button will turn red when you are recording and listening to your echo. Click the button again and *mvoice* will decode the saved AMBE data and play it back to you. This is a great way to check the operation of your setup as well as the volume levels and make sure your headset is working properly. Whenever you complete an Echo Test, you'll get a summary showing the "hot mic" time, the volume (in decibels relative to an arbitrary reference that is mostly quiet audio) and the precentage of when your audio was at or very near clipping (95% of the maximum amplitude). For the echo test, you'll generally want a volume of at least 24 to 30dB, although below 20dB is still copyable. Note that *mvoice* has no volume or gain controls for either TX or RX. You'll use your Linux gui settings for this.

2) **PTT** the large PTT button is also toggle switch. Click it and it will turn red also and *mvoice* will encode your audio and send it on a route or to your destination. Click the button again to return *mvoice* to receiving. When you do, you'll see your volume statistics. *mvoice* will also report volume statistics on incoming audio streams.

3) **Quick Key** is a single press button that will send a 200 millisecond transmission. This is useful when trying to get the attention of a Net Control Operator when your are participating in a Net. It could also be useful if you are trying to do a direct callsign route when you are behind a firewall you can't configure.

## Bugs

 I would like the Echo Test button to switch back from red as soon as you toggle the button to end your recording, but it does not. This is a bug that may not be fixable based on the gtkmm gui. For now at least, we can say the Echo Test button will stay lit until the playback has completed.

 The text buffer for the display window is not truncated in anyway and given enough messages, it will eventually overflow and probably crash *mvoice*, but it will take a very long time. I'll fix this as soon as I decide on a strategy.

 I know, auto-scrolling doesn't work as good as it should. I *think* I'm doing everything I'm suppose to be doing. This may be another limitation of gtkmm.

de N7TAE (at) tearly (dot) net
