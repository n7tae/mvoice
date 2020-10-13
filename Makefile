# Copyright (c) 2019-2020 by Thomas A. Early N7TAE
CFGDIR = $(HOME)/etc/
BINDIR = $(HOME)/bin/

# choose this if you want debugging help
#CPPFLAGS=-ggdb -W -Wall -std=c++11 -Icodec2 -DCFG_DIR=\"$(CFGDIR)\" `pkg-config --cflags gtkmm-3.0`
# or, you can choose this for a much smaller executable without debugging help
CPPFLAGS=-W -Wall -std=c++11 -Icodec2 -DCFG_DIR=\"$(CFGDIR)\" `pkg-config --cflags gtkmm-3.0`

EXE = mvoice
SRCS = $(wildcard *.cpp) $(wildcard codec2/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

$(EXE) : $(OBJS)
	g++ -o $@ $^ `pkg-config --libs gtkmm-3.0` -lasound -lsqlite3 -pthread

%.o : %.cpp
	g++ $(CPPFLAGS) -MMD -MD -c $< -o $@

.PHONY : clean

clean :
	$(RM) $(OBJS) $(DEPS) $(EXE)

-include $(DEPS)

install : $(EXE) MVoice.glade
	mkdir -p $(CFGDIR)
	/bin/cp -rf $(shell pwd)/announce $(CFGDIR)
	/bin/ln -f $(shell pwd)/MVoice.glade $(CFGDIR)
	mkdir -p $(BINDIR)
	/bin/cp -f $(EXE) $(BINDIR)

uninstall :
	/bin/rm -rf $(CFGDIR)announce
	/bin/rm -f $(CFGDIR)MVoice.glade
	/bin/rm -f $(CFGDIR)$(EXE).cfg
	/bin/rm -f $(CFGDIR)qn.db
	/bin/rm -f $(BINDIR)$(EXE)
	/bin/rm -f $(WINDIR)index.php
	/bin/rm qdvdash.service

#interactive :
#	GTK_DEBUG=interactive ./$(EXE)
