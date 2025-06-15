SUBDIRS+=src
SUBDIRS+=test

.PHONY: all clean install $(SUBDIRS)

all: $(SUBDIRS)

install: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean
