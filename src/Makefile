# Copyright (c) 2025 Vincent Jardin, Free Mobile
NAME := sffcmis
LIB_MAJOR := 0
LIB_MINOR := 1
LIBNAME := lib$(NAME)
VERSION := $(LIB_MAJOR).$(LIB_MINOR)
SONAME := $(LIBNAME).so.$(LIB_MAJOR)
LIB := $(LIBNAME).so.$(VERSION)

SRCS += json_print.c
SRCS += json_writer.c
SRCS += module-common.c
SRCS += sff-common.c
SRCS += sfpdiag.c

SRCS += i2c.c
SRCS += sffcmis.c

SRCS += sfpid.c
SRCS += qsfp.c
SRCS += cmis.c

OBJS        := $(SRCS:.c=.o)

# Compiler and linker (can be overridden by environment)
# Buildroot: 
# Yocto (TBC): make CC=${TARGET_PREFIX}gcc LD=${TARGET_PREFIX}ld ...
CC ?= gcc
ifeq ($(LD),ld)
LD :=$(CC)
endif
AR ?= ar
STRIP ?= strip

CFLAGS ?= -Wall -Wextra -O2 -fPIC
LDFLAGS ?= -shared -Wl,-soname,$(SONAME)

DESTDIR ?=
PREFIX ?= /usr
INSTALL_LIB ?= $(DESTDIR)$(PREFIX)/lib

all: $(LIB)

$(LIB): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -f *.o $(LIB)

install: $(LIB)
	install -d $(INSTALL_LIB)
	install -m 0755 $(LIB) $(INSTALL_LIB)/
	ln -sf $(LIB) $(INSTALL_LIB)/$(SONAME)
	ln -sf $(SONAME) $(INSTALL_LIB)/$(LIBNAME).so

.PHONY: all clean install
