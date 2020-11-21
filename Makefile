# Copyright (c) 2019-2020 by Thomas A. Early N7TAE
CFGDIR = $(HOME)/etc/
BINDIR = $(HOME)/bin/

# choose this if you want debugging help
#CPPFLAGS=-ggdb -W -Wall -std=c++11 -Icodec2 -DCFG_DIR=\"$(CFGDIR)\" `pkg-config --cflags gtkmm-3.0`
# or, you can choose this for a smaller executable without debugging help
CPPFLAGS=-W -Wall -std=c++11 -Icodec2 -DCFG_DIR=\"$(CFGDIR)\" `pkg-config --cflags gtkmm-3.0`

EXE = mvoice
SRCS = $(wildcard *.cpp) $(wildcard codec2/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

$(EXE) : $(OBJS)
	g++ -o $@ $^ `pkg-config --libs gtkmm-3.0` -lasound -lsqlite3 -lcurl -pthread

%.o : %.cpp MVoice.glade
	g++ $(CPPFLAGS) -MMD -MD -c $< -o $@

.PHONY : clean

clean :
	$(RM) $(OBJS) $(DEPS) $(EXE)

-include $(DEPS)

install : $(EXE)
	mkdir -p $(CFGDIR)
	/bin/ln -f $(shell pwd)/MVoice.glade $(CFGDIR)
	mkdir -p $(BINDIR)
	/bin/cp -f $(EXE) $(BINDIR)

uninstall :
	/bin/rm -rf $(CFGDIR)announce
	/bin/rm -f $(CFGDIR)MVoice.glade
	/bin/rm -f $(CFGDIR)$(EXE).cfg
	/bin/rm -f $(CFGDIR)qn.db
	/bin/rm -f $(CFGDIR)mvoice.cfg
	/bin/rm -f $(CFGDIR)m17refl.json
	/bin/rm -f $(BINDIR)$(EXE)

#interactive :
#	GTK_DEBUG=interactive ./$(EXE)
