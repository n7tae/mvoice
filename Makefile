# Copyright (c) 2019-2020 by Thomas A. Early N7TAE
CFGDIR = $(HOME)/etc/
BINDIR = $(HOME)/bin/
WWWDIR = $(HOME)/www/
SYSDIR = /lib/systemd/system/

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
	$(RM) $(OBJS) $(DEPS)

-include $(DEPS)

qdvdash.service : qdvdash.txt
	sed -e "s|HHHH|$(WWWDIR)|" qdvdash.txt > qdvdash.service

install : $(EXE) qdvdash.service MVoice.glade
	mkdir -p $(CFGDIR)
	/bin/cp -rf $(shell pwd)/announce $(CFGDIR)
	/bin/ln -f $(shell pwd)/MVoice.glade $(CFGDIR)
	mkdir -p $(BINDIR)
	/bin/cp -f $(EXE) $(BINDIR)
	mkdir -p $(WWWDIR)
	sed -e "s|HHHH|$(CFGDIR)|" index.php > $(WWWDIR)index.php

installdash :
	/usr/bin/apt update
	/usr/bin/apt install -y php-common php-fpm sqlite3 php-sqlite3
	/bin/cp -f qdvdash.service $(SYSDIR)
	systemctl enable qdvdash.service
	systemctl daemon-reload
	systemctl start qdvdash.service

uninstall :
	/bin/rm -rf $(CFGDIR)announce
	/bin/rm -f $(CFGDIR)MVoice.glade
	/bin/rm -f $(CFGDIR)$(EXE).cfg
	/bin/rm -f $(CFGDIR)qn.db
	/bin/rm -f $(BINDIR)$(EXE)
	/bin/rm -f $(WINDIR)index.php
	/bin/rm qdvdash.service

uninstalldash :
	systemctl stop qdvdash.service
	systemctl disable qdvdash.service
	/bin/rm -f $(SYSDIR)qdvdash.service
	systemctl daemon-reload
	/bin/rm -f $(WWWDIR)index.php
	/bin/rm -f $(CFGDIR)qn.db

#interactive :
#	GTK_DEBUG=interactive ./$(EXE)
