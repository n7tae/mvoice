# Copyright (c) 2022 by Thomas A. Early N7TAE

# Make sure you have write/modify access to the next two path variables. You need to have
# write access to both of these folders.
# Change the following line if you want to change where the MVoice configuration data is installed.
# Some like to use $(HOME)/.config/mvoice/
CFGDIR = $(HOME)/etc/
# This defines where the MVoice executable is installed.
BINDIR = $(HOME)/bin/

# Set the following to true if you want to build in debugging support.
DEBUG = false

# The only audio rates supported are 8000 and 44100 Hz.
# MVoice will be smaller and a little bit faster with 8000 Hz.
# So if your audio system supports it, use 8000.
# Otherwise, set this to true.
USE44100 = false

# By default, mvoice uses the Digital Voice Information Network, a "distributed hash table" network.
# The DVIN will provide addtional information about reflectors, making it easier to use them.
# if you don't want this, set this to false.
USE_DVIN = true

# If you change any of the above three values, be sure to do a "make clean" before you do a "make".

# This defines where the translations are installed.
LOCALEDIR = $(HOME)/share/locale
