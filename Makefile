####################################################################################
#
# v32sim Makefile
#
# directory layout:
#   src/  C sources, plus lexer.l (flex) and parser.y (bison)
#   inc/  headers
#   obj/  build intermediates (.o, .d, and the
#         flex/bison generated lexer.c, parser.c,
#         parser.h)                             [generated]
#   bin/  compiled binary                       [generated]
#
# build requirements: a C compiler, GNU readline, flex, and bison
#
####################################################################################

ARCH    = $(shell uname -m)
OS      = $(shell uname -s)

CC      = gcc
TARGET  = v32sim

FLEX    = flex
BISON   = bison
VERSION_STRING = $(shell sed -n 's/^\#define  *VERSION  *"\(.*\)".*/\1/p' inc/version.h)

SRCDIR  = src
INCDIR  = inc
OBJDIR  = obj
BINDIR  = bin

INC     = -I$(INCDIR) -I$(OBJDIR)

CFLAGS  = -Wall --std=gnu18 -funsigned-char -Wno-unused-value
CFLAGS += -Wno-pointer-sign -Wno-main -Wno-int-conversion
CFLAGS += -Wno-unused-but-set-variable

# generated flex/bison sources are not ours to keep warning-free
GENFLAGS = -Wno-unused-function -Wno-sign-compare -Wno-unused-parameter -Wno-implicit-fallthrough

# GNU readline; macOS ships history inside readline, Linux wants it separate
LIBS    = -lreadline -lm
ifneq ($(OS),Darwin)
    LIBS += -lhistory
endif

####################################################################################
#
# file sets
#
####################################################################################

SRC     = $(wildcard $(SRCDIR)/*.c)
GENSRC  = $(OBJDIR)/lexer.c $(OBJDIR)/parser.c $(OBJDIR)/asm_lexer.c $(OBJDIR)/asm_parser.c
GENHDR  = $(OBJDIR)/parser.h $(OBJDIR)/asm_parser.h
OBJ     = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC)) $(GENSRC:.c=.o)
DEPS    = $(OBJ:.o=.d)
BIN     = $(BINDIR)/$(TARGET)

####################################################################################
#
# targets
#
####################################################################################

all: $(BIN)

$(BIN): $(OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $(INC) $(OBJ) -o $@ $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(GENHDR) | $(OBJDIR)
	$(CC) $(CFLAGS) $(INC) -MMD -MP -c $< -o $@

# command grammar: bison writes parser.c and parser.h, flex writes lexer.c (which
# includes parser.h for the token numbers)
$(OBJDIR)/parser.c: $(SRCDIR)/parser.y | $(OBJDIR)
	$(BISON) --defines=$(OBJDIR)/parser.h -o $@ $<

$(OBJDIR)/parser.h: $(OBJDIR)/parser.c ;

$(OBJDIR)/lexer.c: $(SRCDIR)/lexer.l $(GENHDR) | $(OBJDIR)
	$(FLEX) -o $@ $<

# live assembler: a second scanner/grammar pair, generated with the prefix 'asm'
$(OBJDIR)/asm_parser.c: $(SRCDIR)/asm_parser.y | $(OBJDIR)
	$(BISON) -p asm --defines=$(OBJDIR)/asm_parser.h -o $@ $<

$(OBJDIR)/asm_parser.h: $(OBJDIR)/asm_parser.c ;

$(OBJDIR)/asm_lexer.c: $(SRCDIR)/asm_lexer.l $(OBJDIR)/asm_parser.h | $(OBJDIR)
	$(FLEX) -P asm -o $@ $<

$(OBJDIR)/parser.o $(OBJDIR)/lexer.o $(OBJDIR)/asm_parser.o $(OBJDIR)/asm_lexer.o: $(OBJDIR)/%.o: $(OBJDIR)/%.c
	$(CC) $(CFLAGS) $(GENFLAGS) $(INC) -MMD -MP -c $< -o $@

$(OBJDIR) $(BINDIR):
	mkdir -p $@

# development build; object files are not rebuilt on flag changes, so run
# 'make clean' when switching between this, 'asan', and the default build
debug: CFLAGS += -DDEBUG -g
debug: $(BIN)

# sanitizer build (ASan + UBSan) for hunting memory errors
asan: CFLAGS += -g -fsanitize=address,undefined -fno-omit-frame-pointer
asan: $(BIN)

# refresh gen/, the flex/bison output that tools/build-nogen.sh (and the bootstrap archives of v32nix and V32DOS) build from
# without needing flex or bison: run after editing a .l or .y file, and commit gen/
gen: $(GENSRC) $(GENHDR)
	@mkdir -p gen && cp $(GENSRC) $(GENHDR) gen/ && echo "refreshed gen/"

# the tools that need the stock console logic: make oracle COMPUTERSOFTWARE=path/to/checkout
oracle:
	@test -n "$(COMPUTERSOFTWARE)" || { echo "make oracle COMPUTERSOFTWARE=<ComputerSoftware checkout>"; exit 2; }
	tools/oracle/build.sh "$(COMPUTERSOFTWARE)" tools/oracle/bin

# tests of the consolidated tools (needs V32TC, V32BIOS; ORACLE for the comparisons with the stock console logic)
test-harness: $(BIN)
	tests/run_harness_tests.sh

# make version            print the version
# make version VERSION=x  stamp x into inc/version.h (and the docs' "version" lines)
ifeq ($(origin VERSION),command line)
version:
	@sed 's/^\(\#define  *VERSION  *\)"[^"]*"/\1"$(VERSION)"/' inc/version.h > inc/version.h.tmp && mv inc/version.h.tmp inc/version.h
	@echo "$(TARGET) $(VERSION)"
else
version:
	@echo "$(TARGET) $(VERSION_STRING)"
endif

# install: the binary to ~/bin/bin.$(ARCH) or ~/bin, and the helper tools (v32sim-peek.sh, v32sim-golden.sh,
# v32sim-session.py, oracle/, ... : what the other projects call through $V32SIM_HOME/tools) to ~/share/v32sim.
# sysinstall: the same under $(PREFIX) (default /usr/local; DESTDIR for packaging). The tools find the binary
# beside themselves (../bin/v32sim), then on $PATH.
PREFIX ?= /usr/local

install: $(BIN)
	@if [ -d ~/bin/bin.$(ARCH) ]; then \
		echo "Installing $(TARGET) to ~/bin/bin.$(ARCH)/"; \
		install -m 755 $(BIN) ~/bin/bin.$(ARCH)/$(TARGET); \
	elif [ -d ~/bin ]; then \
		echo "Installing $(TARGET) to ~/bin/"; \
		install -m 755 $(BIN) ~/bin/$(TARGET); \
	else \
		echo "Skipping: neither ~/bin/bin.$(ARCH) nor ~/bin exist"; \
	fi
	@echo "Installing the tools to ~/share/v32sim/"; mkdir -p ~/share/v32sim && cp -R tools ~/share/v32sim/ && find ~/share/v32sim/tools -type f -name '*.o' -delete

uninstall:
	rm -f ~/bin/bin.$(ARCH)/$(TARGET) ~/bin/$(TARGET); rm -rf ~/share/v32sim

sysinstall: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/share/v32sim
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	cp -R tools $(DESTDIR)$(PREFIX)/share/v32sim/
	@echo "Installed $(TARGET) to $(PREFIX)/bin and the tools to $(PREFIX)/share/v32sim/tools"

sysuninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET); rm -rf $(DESTDIR)$(PREFIX)/share/v32sim

# the archive carries the date of the version: ../v32sim-project-YYYYMMDD.zip
ARCHIVE = ../v32sim-project-$(shell echo $(VERSION_STRING) | cut -c1-8).zip

archive: clean
	@rm -f $(ARCHIVE)
	@zip -r $(ARCHIVE) *

clean:
	@rm -vf .*.sw[op] *.save* *~ core errors *.patch *.zip ../v32sim-project.zip ../v32sim-project-[0-9]*.zip
	@rm -vf $(DEPS) $(GENSRC) $(GENHDR) $(BINDIR)/$(TARGET)
	@rm -rf $(OBJDIR)

.PHONY: all debug asan gen oracle test-harness version install uninstall sysinstall sysuninstall clean archive

-include $(DEPS)

