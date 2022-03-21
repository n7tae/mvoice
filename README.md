# M17 Digital Voice

*M17 Digital Voice* , MVoice, is a fully functional, graphical repeater. It uses David Rowes Codec 2 and operates as a complete M17 repeater, only there is no RF component. It can Link to M17 reflectors and it can also do *routing*! It works best with a USB-based headset with microphone. MVoice uses the default pulseaudio/ALSA input and output device, so for most versions of linux, all you need to do is plug your headset in and you should be ready to go.

## Building on a Raspberry Pi

Raspberry Pi OS is not ready for *MVoice* out of the box. While Raspbian has ALSA, it also needs pulseaudio:

```bash
sudo apt install -y pulseaudio pavucontrol paprefs
```

It's proabably a good idea to reboot after installing pulseaudio. After it's installed, take a look at the output of `aplay -L`. Near the beginning is should say:

```bash
default
    Playback/recording through the PulseAudio sound server
```

You might also need to go to the ALSA audio configuration. For Debian Buster, this is found in main menu under **Preferences-->Audio Device Settings**. Select your headset in the drop-down list of devices, and then configure it and set it to be the default device. Set your speaker and microphone gain somewhere near the top. Once you build and configure *MVoice*, you can use the Echo feature to help you set these gains adjust the speaker volume of you headset for a comfortable level and adjust the mic gain for the loudest playback without clipping. For my setup, the playback (speaker) was near 100% and the mic gain was at about 50%.

## Building tools and prerequisites

There are several library requirements before you start:

```bash
sudo apt install -y git build-essential libasound2-dev libcurl4-gnutls-dev
```

## FLTK

*MVoice* uses FLTK, the *Fast Light Tool Kit*. You may be able to install it with the package manager:

```bash
sudo apt install -y libfltk1.3-dev
```

You can also build it yourself from the FLTK git repository:

```bash
git clone https://github.com/fltk/fltk.git
cd fltk
git checkout branch-1.3
make
sudo make install
```

## Building and Installing *MVoice*

Then you can download and build MVoice:

```bash
git clone git://github.com/n7tae/MVoice.git
cd MVoice
make
```

If it builds okay, then you can install it:

```bash
make install
```

All the configuration files are located in ~/etc and the MVoice executable is in ~/bin. Please note that symbolic links are installed, so you can do a `make clean` to get rid of the intermediate object files, but don't delete the build directory because that's the only way to remove *MVoice* from your system:

```bash
make uninstall
```

### Special comments only for the Raspberry Pi

If you want a desktop icon to launch *MVoice* then, from your build directory:

```bash
cp MVoice.desktop ~/Desktop
```

If you don't want to answer the question "What do you want to do with it?" every time you double-click the new desktop icon, then in the Raspberry Pi Menu, go to "Accessories->File Manager->Edit->Preference->General and turn on the "Don't ask options..." check box. The MVoice.desktop file will lauch a terminal window to run *MVoice*. If you really don't want to see this terminal window, then change the "Terminal=true" line to "Terminal=false" in your copy of MVoice.desktop in your Desktop folder.

The Raspberry Pi OS has a bug in xdg-open and that's what *MVoice*'s "Open Dashboard" button uses to view a reflector's dashboard. If Chromium isn't running when you click this button, it will freeze the *MVoice* gui. To unfreeze the gui, simple kill Chromium. To get around this bug, launch Chromium before you click this "Open Dashboard" button and a new tab with the dashboard will open without a problem.

## Configuring MVoice

Plug in your headset and start *MVoice*: Open a shell and type `mvoice` if ~/bin is in your search path, otherwise, move to your home directory and type `bin/mvoice`. Most log messages will be displayed within the log window of MVoice, but a few messages, especially if there are problems, may also be printed in the shell you launch MVoice from.

The first thing *MVoice* will do is to download the list of registered reflectors from the M17project.org website and save it in your configuration directory. It will do this every time you start *MVoice*.

Once it launches, click the **Settings** button and make sure to set your callsign and the codec setting on the M17 page. You can usually leave the audio settings on "default". Also enable IPv6 if your internet provider supports it. Click the Okay button and your settings will be saved in your ~/etc/ directory.

On the main window, set your destination callsign and IP address. You can select a reflector from the dropdown list and that will fill the Destination callsign and IP address, or you can type the callsign into the Callsign Entry box. If the IP address can be found from your local data, the IP will be automatically set, otherwise, you will need to know the IP address of your destination. Once you have valid values for both, you can save these for future use, if it's not already in your database. Please remember that when you are setting a new destination, the callsign and IP entry will "approve" their input strictly based on syntax and not whether the callsign or an IP address is actually a place where an M17 destination exists.

Please note that you don't have to save a destination to use it, but it will not be available in the drop-down selection until you do save it. If you haven't saved a destination and you select another destination from the dropdown list, your unsaved destination will no longer be available. If you are going to use an M17 reflector, a vaild callsign is exactly **seven** characters long, `M17-xxx` where `xxx` is made up of letters or numbers. Or, now you can also link to the new URF reflectors. A URF reflector callsign is exactly **six** characters long, beginning with "URF".

Once you set the reflector callsign, you can select the reflector module with the radio buttons from `A` to `Z`. If you are callsign routing to an individual, you can provide a module in the ninth position, but it isn't necessary. If the reflector you're using is from the `M17Hosts.csv` file, you can open the reflector's dashboard with the `Open Dashboard` button.

For Linking, you can select a reflector. The link button will only be activated after you have entered a valid target in the destination callsign and IP address. This validation **does not** check to see if the module you are requesting is actually configured and operational, or even if it actually exists.

## Firewall

*MVoice* can operate in linking or routing mode. If you want to be able to recieve incoming callsign routes, you'll need to port-forward UDP 17000 on your network firewall. If you can't, you can still connect to M17 reflectors. You can originate a direct, callsign route, but if there is a long pause in the QSO, you're firewall will shutdown the port until you key up again.

## Operating

There are three "transmitting" buttons on *MVoice*:

1) **Echo Test** is a toggle switch. Click it to start recording an echo test and your voice will be encoded using the Codec 2 mode you selected in the Settings dialog. Note that the button will turn red when you are recording and listening to your echo. Click the button again and *MVoice* will decode the saved AMBE data and play it back to you. This is a great way to check the operation of your setup as well as the volume levels and make sure your headset is working properly. Whenever you complete an Echo Test, you'll get a statistical summary of your recorded audio, the volume (in decibels relative to an arbitrary reference that is mostly quiet audio) and the precentage of when your audio was within 6 dB of clipping (50% of the maximum amplitude). For the echo test, you'll generally want 0% clip and a volume of around 20-40 dB, although most anything above 6 dB is still copyable. Note that *MVoice* has no volume or gain controls for either TX or RX. You'll use your Linux gui settings for this.

2) **PTT** the large PTT button is also toggle switch. Click it and it will turn red also and *MVoice* will encode your audio and send it on a route or to your destination. Click the button again to return *MVoice* to receiving. When you do, you'll see your volume statistics. *MVoice* will also report volume statistics on incoming audio streams.

3) **Quick Key** is a single press button that will send a 200 millisecond transmission. This is useful when trying to get the attention of a Net Control Operator when your are participating in a Net. It could also be useful if you are trying to do a direct callsign route when you are behind a firewall you can't configure.

de N7TAE (at) tearly (dot) net
