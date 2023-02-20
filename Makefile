# Copyright (c) 2019-2022 by Thomas A. Early N7TAE

include mvoice.mk

ifeq ($(DEBUG), true)
CPPFLAGS = -ggdb -W -std=c++17 -Icodec2 -DCFG_DIR=\"$(CFGDIR)\" -DLOCALEDIR=\"$(LOCALEDIR)\"
else
CPPFLAGS = -W -std=c++17 -Icodec2 -DCFG_DIR=\"$(CFGDIR)\" -DLOCALEDIR=\"$(LOCALEDIR)\"
endif

ifneq ($(USE_DVIN), true)
CPPFLAGS += -DNO_DHT
endif

ifeq ($(USE44100), true)
CPPFLAGS += -DUSE44100
endif

CPPFLAGS += `fltk-config --cxxflags`

EXE = mvoice

SRCS = AboutDlg.cpp AudioManager.cpp Base.cpp Callsign.cpp Configure.cpp CRC.cpp M17Gateway.cpp M17RouteMap.cpp MainWindow.cpp SettingsDlg.cpp TransmitButton.cpp UDPSocket.cpp UnixDgramSocket.cpp

ifeq ($(USE44100), true)
SRCS += Resampler.cpp
endif

SRCS += $(wildcard codec2/*.cpp)

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

all : subdirs $(EXE)

$(EXE) : $(OBJS)
	g++ -o $@ $^ `fltk-config --use-images --ldflags` -lasound -lcurl -pthread -lopendht

%.o : %.cpp
	g++ $(CPPFLAGS) -MMD -c $< -o $@

mvoice.mk :
	cp example.mk mvoice.mk

.PHONY : clean

clean : subdirs
	$(RM) *.o codec2/*.o *.d codec2/*.d $(EXE)

-include $(DEPS)

install : $(EXE) subdirs
	mkdir -p $(CFGDIR)
	/bin/cp -f $(shell pwd)/images/mvoice48.png $(CFGDIR)
	mkdir -p $(BINDIR)
	/bin/cp -f $(EXE) $(BINDIR)

uninstall : subdirs
	/bin/rm -f $(CFGDIR)mvoice48.png
	/bin/rm -f $(CFGDIR)$(EXE).cfg
	/bin/rm -f $(CFGDIR)m17refl.json
	/bin/rm -f $(BINDIR)$(EXE)

SUBDIRS = po

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
