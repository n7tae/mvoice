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
DEBUG = false

ifeq ($(DEBUG), true)
CPPFLAGS = -ggdb -W -Wall -std=c++11 -Icodec2 -DCFG_DIR=\"$(CFGDIR)\"
else
CPPFLAGS = -W -Wall -std=c++11 -Icodec2 -DCFG_DIR=\"$(CFGDIR)\"
endif

ifeq ($(USE44100), true)
CPPFLAGS += -DUSE44100
endif

CPPFLAGS += `fltk-config --cxxflags`

EXE = mvoice

ifeq ($(USE44100), true)
SRCS = $(wildcard *.cpp)
else
SRCS = AboutDlg.cpp Base.cpp Configure.cpp M17Gateway.cpp MainWindow.cpp SettingsDlg.cpp UDPSocket.cpp AudioManager.cpp Callsign.cpp CRC.cpp M17RouteMap.cpp TransmitButton.cpp UnixDgramSocket.cpp
endif

SRCS += $(wildcard codec2/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

$(EXE) : $(OBJS)
	g++ -o $@ $^ `fltk-config --use-images --ldflags` -lasound -lcurl -pthread

%.o : %.cpp
	g++ $(CPPFLAGS) -MMD -MD -c $< -o $@

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
