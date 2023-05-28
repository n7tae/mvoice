# Copyright (c) 2022 by Thomas A. Early N7TAE

# BASEDIR includes two places:
# $(BASEDIR)/bin for the executable
# $(BASEDIR)/share/locale for the language file(s)
BASEDIR = $(HOME)
# If mvoice is shared between different user, you can use:
# BASEDIR = /usr/local

# The user configuration files are always relative to $(HOME)
CFGDIR = .config/mvoice

# Set the following to true if you want to build in debugging support.
DEBUG = false

# The only audio rates supported are 8000 and 44100 Hz.
# MVoice will be smaller and a little bit faster with 8000 Hz.
# So if your audio system supports it, use 8000.
# Otherwise, set this to true.
USE44100 = false

# By default, mvoice uses the Ham-DHT network, a "distributed hash table" network.
# The Ham-DHT will provide addtional information about reflectors, making it easier to use them.
# if you don't want this, set this to false.
USE_DHT = true
