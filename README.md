# M17 Digital Voice, now using FLTK

*M17 Digital Voice* , mvoice, is a fully functional, M17 gateway and module for both voice and packet mode. For voice, it uses David Rowes Codec 2 and operates as a complete M17 repeater, only there is no RF component. It can Link to M17 reflectors and it can also do *routing*! It works best with a USB-based headset with microphone. mvoice uses the default pulseaudio/ALSA input and output device, so for most versions of linux, all you need to do is plug your headset in and you should be ready to go. mvoice does SMS texting using M17 Packet mode. At this time, mvoice only supports SMS texting.

## Building on a Raspberry Pi

Raspberry Pi OS is not ready for *mvoice* out of the box. While Raspbian has ALSA, it also needs pulseaudio:

```bash
sudo apt install -y pulseaudio pavucontrol paprefs
```

It's probably a good idea to reboot after installing pulseaudio. After it's installed, take a look at the output of `aplay -L`. Near the beginning is should say:

```bash
default
    Playback/recording through the PulseAudio sound server
```

You might also need to go to the ALSA audio configuration. For Debian Buster, this is found in main menu under **Preferences-->Audio Device Settings**. Select your headset in the drop-down list of devices, and then configure it and set it to be the default device. Set your speaker and microphone gain somewhere near the top. Once you build and configure *mvoice*, you can use the Echo feature to help you set these gains adjust the speaker volume of you headset for a comfortable level and adjust the mic gain for the loudest playback without clipping. For my setup, the playback (speaker) was near 100% and the mic gain was at about 50%.

## Building tools and prerequisites

There are several library requirements before you start:

```bash
sudo apt install -y git build-essential libasound2-dev nlohmann-json3-dev libcurl4-gnutls-dev gettext
```

## FLTK

*mvoice* now uses FLTK, the *Fast Light Tool Kit*. You may be able to install it with the package manager:

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

## Distributed Hash Table (OpenDHT)

Using the OpenDHT library, mvoice now joins a special digital voice, ham-radio DHT network to discover reflector and repeater destinations directly. For these target systems using the DHT, connection information is published and updated directly by the target and is available to mvoice in near-realtime. All the mvoice user needs to know is the callsign of the target.

If you are using a new OS, like Debian 12 or Ubuntu 24, you may not have to build the OpenDHT support. Try:

```
sudo apt install libopendht-dev
```

If the description shows that this package was build with `C++17` or newer, go ahead and let it install, otherwise you should build your own version of the OpenDHT library.

### Building and installing the OpenDHT support

OpenDHT is available [here](https://github.com/savoirfairelinux/opendht.git). Building and installing instructions are in the [OpenDHT Wiki](https://github.com/savoirfairelinux/opendht/wiki/Build-the-library). Pascal support and proxy-server support (RESTinio) is not required for mvoice and so can be considered optional. With this in mind, this should work on Debian/Ubuntu-based systems:

```bash
# Install OpenDHT dependencies
sudo apt install libncurses5-dev libreadline-dev nettle-dev libgnutls28-dev libargon2-0-dev libmsgpack-dev  libssl-dev libfmt-dev libjsoncpp-dev libhttp-parser-dev libasio-dev cmake pkg-config libcppunit-dev

# clone the repo
git clone https://github.com/savoirfairelinux/opendht.git

# build and install
cd opendht
mkdir build && cd build
cmake -DOPENDHT_PYTHON=OFF -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```

Please note that there is no easy way to uninstall OpenDHT once it's been installed. However, it is based on static header files and libraries and doesn't use any resources except for a small amount of disk space, unless mvoice is running. You can delete your downloaded OpenDHT git repo once you've installed the OpenDHT library.

## Building and Installing *mvoice*

### Get the *mvoice* repository, and move to it

```bash
git clone https://github.com/n7tae/mvoice.git
cd mvoice
```

### Compiling

There are several compile-time options for building *mvoice*, including where run-time files are stored, where the *mvoice* executable is installed, support for 44,100 Hz (CD-quality) audio, debugging support and support for the new **Digital Voice Information Network**. These are all controlled by your very own `mvoice.mk` file. Start by creating the file:

```bash
cp example.mk mvoice.mk
```

Use your favorite editor to change values in your `mvoice.mk` file.

You're now read to build *mvoice*:

```bash
make
```

### Installing, cleaning, and uninstalling

If it builds okay, then you can install it:

```bash
make install
```

To clean up object files and other intermediate files:

```bash
make clean
```

Don't delete the build directory because that's the only way to remove *mvoice* from your system:

```bash
make uninstall
```

### Special comments only for the Raspberry Pi

If you want a desktop icon to launch *mvoice* then, from your build directory:

```bash
cp mvoice.desktop ~/Desktop
```

If you don't want to answer the question "What do you want to do with it?" every time you double-click the new desktop icon, then in the Raspberry Pi Menu, go to "Accessories->File Manager->Edit->Preference->General and turn on the "Don't ask options..." check box. The mvoice.desktop file will launch a terminal window to run *mvoice*. If you really don't want to see this terminal window, then change the "Terminal=true" line to "Terminal=false" in your copy of mvoice.desktop in your Desktop folder.

The Raspberry Pi OS has a bug in xdg-open and that's what *mvoice*'s "Open Dashboard" button uses to view a reflector's dashboard. If Chromium isn't running when you click this button, it will freeze the *mvoice* gui. To unfreeze the gui, simple kill Chromium. To get around this bug, launch Chromium before you click this "Open Dashboard" button and a new tab with the dashboard will open without a problem.

## Configuring mvoice

Plug in your headset and start *mvoice*: Open a shell and type `mvoice` if ~/bin is in your search path, otherwise, move to your home directory and type `bin/mvoice`, or where ever you installed it. Most log messages will be displayed within the log window of mvoice, but a few messages, especially if there are problems, may also be printed in the shell you launch mvoice from.

## Firewall

*mvoice* can connect to a reflector module (channel) or it can also do direct one-to-one routing to a target. If you want to be able to receive incoming callsign routes, you'll need to port-forward UDP 17000 on your network firewall. If you can't, you can still connect to M17 reflectors.

## Operating

The first thing *mvoice* will do is to download the list of registered M17 and URF reflectors from the hostfiles.refcheck.radio website and parse it. It will do this every time you start *mvoice*.

Once it launches, click the **Settings** button and make sure to set your callsign and the codec setting on the M17 page. You can usually leave the audio settings on "default". Also enable IPv6 if your internet provider supports it. Click the Okay button and your settings will be saved in your configuration directory.

On the main window, set your destination callsign and IP address. You can select a reflector from the dropdown list and that will fill the Destination callsign and IP address, or you can type the callsign into the Callsign Entry box. The dropdown list of targets is very large and it can be tedious to find a target in the lowest part of the list. It can be much easier to simply type into the target field. Once you've typed something in your database, it will fill in the, IsLegacy checkbox, IP address an port number for you. If target cannot be found in the database, you will need to know if the reflector is a legacy reflector and the IP address of your destination. You also need to specify UDP port number. Usually this is 17000. Once you have valid values for all three items, you can save these for future use, if it's not already in your database. Please remember that when you are setting a new destination, the callsign and IP entry will "approve" their input strictly based on syntax and not whether the callsign, an IP address and a port is actually a place where an M17 destination exists. If you are connecting to a reflector, select the module you want to use.

Please note that you don't have to save a destination to use it, but it will not be available in the drop-down selection until you do save it. If you haven't saved a destination and you select another destination from the dropdown list, your unsaved destination will no longer be available. If you are going to use an M17 reflector, a valid callsign is exactly **seven** characters long, `M17-xxx` where `xxx` is made up of letters or numbers. Or, now you can also link to the new URF reflectors. A URF reflector callsign is exactly **six** characters long, `URFxxx`.

Once you set the reflector callsign, you can select the reflector module with the radio buttons from `A` to `Z`. If you are callsign routing to an individual, you can provide a module in the ninth position, but it isn't necessary. If *mvoice* has found a URL for the target, you can open the reflector's dashboard with the **Open Dashboard** button.

You need to connect to a reflector before you can use it. The connect button will only be activated after you have entered a valid target in the destination callsign and IP address and port number. If you've entered a malformed field for any of the three values, the background for that field will turn red. Properly formed input will have green backgrounds. This validation **does not** check to see if reflect exists at that IP and the module you are requesting is actually configured and operational, or even if it actually exists. You also need to set the `IsLegacy` checkbox. Once all three values are green, you can try to connect to it with the `Connect` button.

The `IsLegacy` checkbox should be set if you are connecting to either any URF reflector or an M17 reflector that is version #0.x.y. These reflectors require that the M17 Destination address be set to the reflector designation and module to which you are connecting. By setting this checkbox, The Destination field will automatically be set correctly and locked once you are connected. Newer M17 reflectors, with versions greater than or equal to 1.x.y support other Destinations like `#PARROT`, hint: when you are connected to a newer M17 reflector, transmitting to `#PARROT` is just like doing an `ECHO`, except you are having the reflector play back your transmission. Careful, M17 reflectors will only record and play back the first 20 seconds of your PARROT transmission.

There are four "transmitting" buttons on *mvoice* that are explained below, but first, an explanation:

Both the **Echo Test** and the **PTT** buttons are *toggle* buttons. You press and release these buttons to start their function and press and release again to stop their function. Both of these buttons also have built-in timers and will show the *approximate* "key on" time.

1) **Echo Test** is a toggle switch. Click it to start recording an echo test and your voice will be encoded using the Codec 2 mode you selected in the Settings dialog. Note that the button will turn yellow when you are recording your test. Click the button again and *mvoice* will decode the saved AMBE data and play it back to you. This is a great way to check the operation of your setup as well as the volume levels and make sure your headset is working properly. Whenever you complete an Echo Test, you'll get a statistical summary of your recorded audio, the volume (in decibels relative to an arbitrary reference that is properly adjusted audio) and the percentage of when your audio was within 6 dB of clipping (50% of the maximum amplitude). For the echo test, you'll generally want 0% clip and a volume of around -14 dB. So ***first*** adjust your system microphone gain to achieve the desired dB, and ***then*** adjust you system volume level for a comfortable listening level. Note that *mvoice* has no volume or gain controls for either TX or RX. You'll use your Linux gui audio settings for this.

2) **PTT** the large PTT button is also toggle switch. Click it and it will turn yellow also and *mvoice* will encode your audio and send it to your destination. Click the button again to return *mvoice* to receiving. When you do, you'll see your volume statistics. *mvoice* will also report volume statistics on incoming audio streams. There is a locking mechanism in PTT: When you start a transmission by toggling the PTT to the "key on" state, *mvoice* will attempt to lock the gateway. If the lock is achieved, all is good. If the gateway is busy, the PTT lock will fail and the PTT button will be returned to the "key off" state. While response of this lock is quick, it does take a finite time to execute! So here is a best practice: Key up, and **wait one or two seconds** before you start talking. By waiting, you're insuring that the PTT lock has been successfully achieved.

3) **Quick Key** is a single press button that will send a 200 millisecond transmission. This is useful when trying to get the attention of a Net Control Operator when your are participating in a Net. It could also be useful if you are trying to do a direct callsign route when you are behind a firewall you can't configure.

4) **Send** on the SMS Texting is used to send a single text message to your target using M17 Packet Mode. This SMS Texting is hidden by default and you show this window by selecting the `Texting...` menu item in the main window. You can compose you message and send it to the target from this window. Messages can be over 800 character long, but usually they will just be a short line or two. After you press the `Send` button, the composition window will be cleared and will be displayed in the main window as an outgoing message.

New to version 1.3, you can also specify the DST callsign for both streaming (voice) mode and packet mode. The DST must be either a valid ham radio callsign, `@ALL` or `#PARROT`. You should know that mvoice operates in a *promiscuous* mode: it will play any incoming voice stream and display any incoming SMS text message regardless of what's specified in the DST encoded callsign field and the **Channel Access Number or "CAN"** of the incoming packet(s). Other M17 receiving system can be more restrictive. Some may only pass data with a specific CAN or only data with a DST of `@ALL` (the M17 BROADCAST address), or their specific callsign. One more thing to note is that when you talk or text through a reflector, your data will be sent to all other client on that reflector module, regardless of the DST you have set.

The `PARROT` DST is for parroting your you input on the latest M17 reflectors (version 1.1 or above). Make sure the reflector to which you are connected is at this release or later before attempting a parrot.

## Language support

Thanks to IU5HKX, *mvoice* is now available in Italian.

If you would like to add another language to *mvoice* GUI to use a different language, I would welcome a pull request. To get started, see https://www.gnu.org/software/gettext/manual/html_node/index.html.

de n7tae (at) tearly (dot) net
