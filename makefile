# Installation paths
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

# Binary name
NAME = builder

# Compiler settings (requires R)
CC = $(shell R CMD config CC)
CFLAGS = $(shell R CMD config --cppflags)
LDFLAGS = $(shell R CMD config --ldflags)
EXTRAFLAGS = -Wall -Iinclude
RELEASEFLAGS = -s
DEBUGFLAGS = -g

# Source files
FILES = src/main.c \
	src/r.c \
	src/for.c \
	src/parser.c \
	src/file.c \
	src/define.c \
	src/include.c \
	src/fstring.c \
	src/deconstruct.c \
	src/test.c \
	src/preflight.c \
	src/plugins.c \
	src/const.c \
	src/deadcode.c \
	src/error.c \
	src/sourcemap.c \
	src/watch.c \
	src/depends.c \
	src/create.c \
	src/assert.c \
	src/buffer.c \
	src/config.c

# Development commands
CMD = ./bin/$(NAME) \
	-input srcr \
	-profile dev \
	-DDEBUG -DTEST '"a string"' -DXXX 42 \
	-deadcode \
	-import builder.r::generate.rh \
	-plugin builder.air::plugin \
	-strip \
	-prepend test \
	-sourcemap

CMD_DEBUG = ./bin/$(NAME)-debug \
	-input srcr \
	-DDEBUG -DTEST '"a string"' -DXXX 42 \
	-deadcode \
	-import builder.r::generate.rh \
	-plugin builder.air::plugin \
	-strip \
	-prepend test \
	-sourcemap

.PHONY: all build build-debug clean install uninstall dev debug site

all: build

build: $(FILES) | bin
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $(RELEASEFLAGS) $^ -o bin/$(NAME) $(LDFLAGS)

build-debug: $(FILES) | bin
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $(DEBUGFLAGS) $^ -o bin/$(NAME)-debug $(LDFLAGS)

bin:
	mkdir -p bin

clean:
	rm -f bin/$(NAME) bin/$(NAME)-debug

install: build
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 bin/$(NAME) $(DESTDIR)$(BINDIR)/$(NAME)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(NAME)

dev: build
	$(CMD)

dryrun: build
	$(CMD) -dry-run -diff

debug: build-debug
	valgrind --leak-check=full $(CMD_DEBUG)

site:
	./docs/build.sh
