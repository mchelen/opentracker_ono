# $Id: Makefile,v 1.45 2008/01/17 10:46:25 erdgeist Exp $

CC?=gcc

# Linux flavour
# PREFIX?=/opt/diet
# LIBOWFAT_HEADERS=$(PREFIX)/include
# LIBOWFAT_LIBRARY=$(PREIFX)/lib

# BSD flavour
# PREFIX?=/usr/local
# LIBOWFAT_HEADERS=$(PREFIX)/include/libowfat
# LIBOWFAT_LIBRARY=$(PREIFX)/lib

# Debug flavour
PREFIX?=..
LIBOWFAT_HEADERS=$(PREFIX)/libowfat
LIBOWFAT_LIBRARY=$(PREFIX)/libowfat

BINDIR?=$(PREFIX)/bin

#FEATURES =-DWANT_TRACKER_SYNC
#FEATURES+=-DWANT_BLACKLISTING
#FEATURES+=-DWANT_CLOSED_TRACKER
#FEATURES+=-DWANT_UTORRENT1600_WORKAROUND
#FEATURES+=-DWANT_IP_FROM_QUERY_STRING
#FEATURES+=-DWANT_COMPRESSION_GZIP 
#FEATURES+=-DWANT_LOG_NETWORKS
#FEATURES+=-D_DEBUG_HTTPERROR

OPTS_debug=-g -ggdb #-pg # -fprofile-arcs -ftest-coverage
OPTS_production=-Os

CFLAGS+=-I$(LIBOWFAT_HEADERS) -Wall -pipe -Wextra #-pedantic -ansi
LDFLAGS+=-L$(LIBOWFAT_LIBRARY) -lowfat -pthread -lz -lhashish -lm

BINARY =opentracker
HEADERS=trackerlogic.h scan_urlencoded_query.h ot_mutex.h ot_stats.h ot_sync.h ot_vector.h ot_clean.h ot_udp.h ot_iovec.h ot_fullscrape.h ot_accesslist.h ot_http.h ono.h bencode.h
SOURCES=opentracker.c trackerlogic.c scan_urlencoded_query.c ot_mutex.c ot_stats.c ot_sync.c ot_vector.c ot_clean.c ot_udp.c ot_iovec.c ot_fullscrape.c ot_accesslist.c ot_http.c ono.c bencode.c

OBJECTS = $(SOURCES:%.c=%.o)
OBJECTS_debug = $(SOURCES:%.c=%.debug.o)

.SUFFIXES: .debug.o .o .c

all: $(BINARY) $(BINARY).debug

CFLAGS_production = $(CFLAGS) $(OPTS_production) $(FEATURES)
CFLAGS_debug = $(CFLAGS) $(OPTS_debug) $(FEATURES)

$(BINARY): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)
	strip $@
$(BINARY).debug: $(OBJECTS_debug) $(HEADERS)
	$(CC) -o $@ $(OBJECTS_debug) $(LDFLAGS)

.c.debug.o : $(HEADERS)
	$(CC) -c -o $@ $(CFLAGS_debug) $(<:.debug.o=.c)

.c.o : $(HEADERS)
	$(CC) -c -o $@ $(CFLAGS_production) $<

clean:
	rm -rf opentracker opentracker.debug *.o *~

install:
	install -m 755 opentracker $(BINDIR)
