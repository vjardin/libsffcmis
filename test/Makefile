CC ?= gcc
LD ?= $(CC)
AR ?= $(CROSS_COMPILE)ar
STRIP ?= $(CROSS_COMPILE)strip

TOP      := ..
INCLUDES := -I$(TOP)/src
LIBDIR   := $(TOP)/src
LIBS     := -lsffcmis

LIBS     += -lm

SRC   := sffcmis_test.c

OBJ   := $(SRC:.c=.o)
TARGET := sffcmis_test

CFLAGS  ?= -Wall -Wextra -O0 -g
LDFLAGS ?= 
CPPFLAGS += $(INCLUDES)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ -L$(LIBDIR) $(LIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	$(INSTALL) -d $(DESTDIR)/usr/bin
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)/usr/bin/

clean:
	@$(RM) -f $(OBJ) $(TARGET)
