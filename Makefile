# Copyright (c) 2019-2020 by Thomas A. Early N7TAE
CFGDIR = $(HOME)/etc/
BINDIR = $(HOME)/bin/
WWWDIR = $(HOME)/www/
SYSDIR = /lib/systemd/system/

# choose this if you want debugging help
CPPFLAGS=-ggdb -W -Wall -std=c++11 -Iircddb -Icodec2 -DCFG_DIR=\"$(CFGDIR)\" `pkg-config --cflags gtkmm-3.0`
# or, you can choose this for a much smaller executable without debugging help
#CPPFLAGS=-W -Wall -std=c++11 -Iircddb -Icodec2 -DCFG_DIR=\"$(CFGDIR)\" `pkg-config --cflags gtkmm-3.0`

EXE = mmdvoice
SRCS = $(wildcard *.cpp) $(wildcard ircddb/*.cpp) $(wildcard codec2/*.cpp)
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

hostfile :
	wget -O XLX_Hosts.txt http://xlxapi.rlx.lu/api.php?do=GetXLXDMRMaster
	wget http://www.pistar.uk/downloads/DExtra_Hosts.txt
	wget http://www.pistar.uk/downloads/DPlus_Hosts.txt
	wget http://www.pistar.uk/downloads/DCS_Hosts.txt
	/bin/rm -f gwys.txt
	echo "# Downloaded from www.pistar.uk and xlxapi.rlx.lu `date`" > gwys.txt
	awk '$$1 ~ /^DCS[0-9]+$$/ { printf "%s %s 30051\n", $$1, $$2 }' DCS_Hosts.txt >> gwys.txt
	awk '$$1 ~ /^REF[0-9]+$$/ { printf "%s %s 20001\n", $$1, $$2 }' DPlus_Hosts.txt >> gwys.txt
	awk '$$1 ~ /^XLX[0-9]+$$/ { gsub("\r", ""); printf "%s %s 30001\n", $$1, $$2 }' XLX_Hosts.txt >> gwys.txt
	awk '$$1 ~ /^XRF[0-9]+$$/ { printf "%s %s 30001\n", $$1, $$2 }' DExtra_Hosts.txt >> gwys.txt
	if test -e My_Hosts.txt; then cat My_Hosts.txt >> gwys.txt; fi
	/bin/rm DCS_Hosts.txt DPlus_Hosts.txt XLX_Hosts.txt DExtra_Hosts.txt

qdvdash.service : qdvdash.txt
	sed -e "s|HHHH|$(WWWDIR)|" qdvdash.txt > qdvdash.service

install : $(EXE) qdvdash.service DigitalVoice.glade
	mkdir -p $(CFGDIR)
	/bin/cp -rf $(shell pwd)/announce $(CFGDIR)
	/bin/ln -f $(shell pwd)/DigitalVoice.glade $(CFGDIR)
	/bin/ln -f -s $(shell pwd)/gwys.txt $(CFGDIR)
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
	/bin/rm -f $(CFGDIR)DigitalVoice.glade
	/bin/rm -f $(CFGDIR)gwys.txt
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
