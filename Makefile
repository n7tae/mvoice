# Copyright (c) 2019-2022 by Thomas A. Early N7TAE
CFGDIR = $(HOME)/etc/
BINDIR = $(HOME)/bin/

CPPFLAGS = -W -Wall -std=c++11 -Icodec2 -DCFG_DIR=\"$(CFGDIR)\" `fltk-config --cxxflags`
# uncomment the next line if you want debugging support
#CPPFLAGS += -ggdb

EXE = mvoice
SRCS = $(wildcard *.cpp) $(wildcard codec2/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

$(EXE) : $(OBJS)
	g++ -o $@ $^ `fltk-config --use-images --ldflags` -lasound -lcurl -pthread

%.o : %.cpp
	g++ $(CPPFLAGS) -MMD -MD -c $< -o $@

.PHONY : clean

clean :
	$(RM) $(OBJS) $(DEPS) $(EXE)

-include $(DEPS)

install : $(EXE)
	mkdir -p $(CFGDIR)
	/bin/cp -f $(shell pwd)/MVoice.glade $(CFGDIR)
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
