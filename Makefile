SUBDIRS+=src
SUBDIRS+=test

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean
