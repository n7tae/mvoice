include ../mvoice.mk

LOCALEDIR = $(BASEDIR)/share/locale
DOMAIN = mvoice
LINGUAS = it

all:
	for lang in $(LINGUAS); do \
	  msgfmt $$lang.po --output=$$lang.mo; \
	done

$(DOMAIN).pot:
	test -f $@ || touch $@
	xgettext --keyword=_ --join-existing --output=$@ --package-name=$(DOMAIN) ../*.cpp ../*.h

install:
	for lang in $(LINGUAS); do \
	  mkdir -p "$(LOCALEDIR)/$$lang/LC_MESSAGES"; \
	  cp $$lang.mo "$(LOCALEDIR)/$$lang/LC_MESSAGES/$(DOMAIN).mo"; \
	done

uninstall:
	for lang in $(LINGUAS); do \
	  rm -f "$(LOCALEDIR)/$$lang/LC_MESSAGES/$(DOMAIN).mo"; \
	  rmdir -p "$(LOCALEDIR)/$$lang/LC_MESSAGES"; \
	done
	rmdir -p "$(LOCALEDIR)" --ignore-fail-on-non-empty

clean: ;
