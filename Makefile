# Copyright (c) 2019-2022 by Thomas A. Early N7TAE

# make sure you have write/modify access to the next two path variables
# Change the following line if you want to change where the MVoice configuration data is installed.
CFGDIR = $(HOME)/etc/
# some like to use $(HOME)/.config/mvoice/

# Change the following line if you want to change where the MVoice executable is installed.
BINDIR = $(HOME)/bin/

# The only audio rates supported are 8000 and 44100 Hz.
# MVoice will be smaller and a little bit faster with 8000 Hz.
# So if your audio system supports it, use 8000.
# Otherwise, set it to true.
USE44100 = false

# Set the next line to true if you want debugging support in the mvoice executable.
DEBUG = true

ifeq ($(DEBUG), true)
CPPFLAGS = -ggdb -Werror -Wall -Wextra -std=c++17 -Icodec2 -DCFG_DIR=\"$(CFGDIR)\"
else
CPPFLAGS = -Werror -Wall -Wextra -std=c++17 -Icodec2 -DCFG_DIR=\"$(CFGDIR)\"
endif

ifeq ($(USE44100), true)
CPPFLAGS += -DUSE44100
endif

CPPFLAGS += `fltk-config --cxxflags`

EXE = mvoice

SRCS = AboutDlg.cpp AudioManager.cpp Base.cpp Callsign.cpp Configure.cpp CRC.cpp M17Gateway.cpp M17RouteMap.cpp MainWindow.cpp SettingsDlg.cpp TransmitButton.cpp UDPSocket.cpp UnixDgramSocket.cpp

ifneq ($(USE44100), true)
SRCS += Resampler.cpp
endif

SRCS += $(wildcard codec2/*.cpp)

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

all : $(EXE) test-get

$(EXE) : $(OBJS)
	g++ -o $@ $^ `fltk-config --use-images --ldflags` -lasound -lcurl -pthread -lopendht

test-get : TestGet.cpp
	g++ -o $@ $^ -pthread -lopendht

%.o : %.cpp
	g++ $(CPPFLAGS) -MMD -c $< -o $@

.PHONY : clean

clean :
	$(RM) *.o codec2/*.o *.d codec2/*.d $(EXE)

-include $(DEPS)

install : $(EXE)
	mkdir -p $(CFGDIR)
	/bin/cp -f $(shell pwd)/images/mvoice48.png $(CFGDIR)
	mkdir -p $(BINDIR)
	/bin/cp -f $(EXE) $(BINDIR)

uninstall :
	/bin/rm -f $(CFGDIR)mvoice48.png
	/bin/rm -f $(CFGDIR)$(EXE).cfg
	/bin/rm -f $(CFGDIR)m17refl.json
	/bin/rm -f $(BINDIR)$(EXE)
