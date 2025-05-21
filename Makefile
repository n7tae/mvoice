# Copyright (c) 2019-2022 by Thomas A. Early N7TAE

include mvoice.mk

ifeq ($(DEBUG), true)
CPPFLAGS = -ggdb -W -std=c++17 -Icodec2 -DCFGDIR=\"$(CFGDIR)\" -DBASEDIR=\"$(BASEDIR)\"
else
CPPFLAGS = -W -std=c++17 -Icodec2 -DCFGDIR=\"$(CFGDIR)\" -DBASEDIR=\"$(BASEDIR)\"
endif

LDFLAGS = `fltk-config --use-images --ldflags` -lasound -lcurl -pthread

ifeq ($(USE_DVIN), true)
LDFLAGS += -lopendht
else ifeq ($(USE_DHT), true)
LDFLAGS += -lopendht
else
CPPFLAGS += -DNO_DHT
endif

ifeq ($(USE44100), true)
CPPFLAGS += -DUSE44100
endif

CPPFLAGS += `fltk-config --cxxflags`

EXE = mvoice

SRCS = AboutDlg.cpp AudioManager.cpp Base.cpp Callsign.cpp Configure.cpp CRC.cpp M17Gateway.cpp M17RouteMap.cpp MainWindow.cpp Packet.cpp SettingsDlg.cpp SMSDlg.cpp TransmitButton.cpp UDPSocket.cpp UnixDgramSocket.cpp

ifeq ($(USE44100), true)
SRCS += Resampler.cpp
endif

SRCS += $(wildcard codec2/*.cpp)

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

all : subdirs $(EXE)

$(EXE) : $(OBJS)
	g++ -o $@ $^ $(LDFLAGS)

%.o : %.cpp
	g++ $(CPPFLAGS) -MMD -c $< -o $@

mvoice.mk :
	cp example.mk mvoice.mk

.PHONY : clean

clean : subdirs
	$(RM) *.o codec2/*.o *.d codec2/*.d $(EXE)

-include $(DEPS)

install : $(EXE) subdirs
	mkdir -p $(BASEDIR)/bin
	/bin/cp -f $(EXE) $(BASEDIR)/bin

uninstall : subdirs
	/bin/rm -f $(BASEDIR)/bin/$(EXE)
	cd $(BASEDIR) && rmdir -p bin --ignore-fail-on-non-empty

SUBDIRS = po

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
