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

# Optimization. -O2 and link-time optimization are the default (the simulator is a hot loop spread over several
# files: LTO lets the memory, port and cycle helpers inline into it); 'make LTO=' builds without LTO, 'make OPT=-O3' etc.
# Do NOT add -ffast-math or -march=native: the GPU and the float instructions must give the same last bit as the
# DesktopEmulator on every host (-ffp-contract=off keeps compilers from fusing multiply-adds on hosts that have them).
OPT     ?= -O2
LTO     ?= $(shell for f in -flto=auto -flto; do echo 'int main(void){return 0;}' | $(CC) $$f -x c - -o /dev/null 2>/dev/null && { echo $$f; break; }; done)

CFLAGS  = -Wall --std=gnu18 -funsigned-char -Wno-unused-value $(OPT) $(LTO) -ffp-contract=off
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

# without flex and bison (tools/build-nogen.sh's situation) the checked-in output in gen/ is used
HAVE_GEN_TOOLS := $(shell command -v $(FLEX) >/dev/null 2>&1 && command -v $(BISON) >/dev/null 2>&1 && echo yes)

ifneq ($(HAVE_GEN_TOOLS),yes)
$(GENSRC) $(GENHDR): | $(OBJDIR)
	cp gen/$(notdir $@) $@
else
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
endif

$(OBJDIR)/parser.o $(OBJDIR)/lexer.o $(OBJDIR)/asm_parser.o $(OBJDIR)/asm_lexer.o: $(OBJDIR)/%.o: $(OBJDIR)/%.c
	$(CC) $(CFLAGS) $(GENFLAGS) $(INC) -MMD -MP -c $< -o $@

$(OBJDIR) $(BINDIR):
	mkdir -p $@

# development build; object files are not rebuilt on flag changes, so run
# 'make clean' when switching between this, 'asan', and the default build
debug: CFLAGS += -DDEBUG -g -O0 -fno-lto
debug: $(BIN)

# sanitizer build (ASan + UBSan) for hunting memory errors
asan: CFLAGS += -g -O1 -fno-lto -fsanitize=address,undefined -fno-omit-frame-pointer
asan: $(BIN)

# refresh gen/, the flex/bison output that tools/build-nogen.sh (and the bootstrap archives of v32nix and V32DOS) build from
# without needing flex or bison: run after editing a .l or .y file, and commit gen/
gen: $(GENSRC) $(GENHDR)
	@mkdir -p gen && cp $(GENSRC) $(GENHDR) gen/ && echo "refreshed gen/"

# the tools that need the stock console logic: make oracle COMPUTERSOFTWARE=path/to/checkout
oracle:
	@test -n "$(COMPUTERSOFTWARE)" || { echo "make oracle COMPUTERSOFTWARE=<ComputerSoftware checkout>"; exit 2; }
	tools/oracle/build.sh "$(COMPUTERSOFTWARE)" tools/oracle/bin

# every suite from one toolchain dir: make check TC=DIR [V32IO=DIR] [SRCMARK=FILE]
check: $(BIN)
	@test -n "$(TC)" || { echo "make check TC=<toolchain dir> [V32IO=<v32io project>] [SRCMARK=v32srcmark.py]"; exit 2; }
	tools/v32sim-check.sh "$(TC)" $(if $(V32IO),--v32io "$(V32IO)") $(if $(SRCMARK),--srcmark "$(SRCMARK)")

# tests of the consolidated tools (needs V32TC, V32BIOS; ORACLE for the comparisons with the stock console logic)
test-harness: $(BIN)
	tests/run_harness_tests.sh

# make version            print the version
# make version VERSION=x  stamp x into inc/version.h and TODO.md's "Status as of version" line (the only places the version is written)
ifeq ($(origin VERSION),command line)
version:
	@sed 's/^\(\#define  *VERSION  *\)"[^"]*"/\1"$(VERSION)"/' inc/version.h > inc/version.h.tmp && mv inc/version.h.tmp inc/version.h
	@sed 's/^\(Status as of version `\)[^`]*`/\1$(VERSION)`/' TODO.md > TODO.md.tmp && mv TODO.md.tmp TODO.md
	@echo "$(TARGET) $(VERSION)"
else
version:
	@echo "$(TARGET) $(VERSION_STRING)"
endif

# install: the binary to ~/bin/bin.$(ARCH) or ~/bin, and the helper tools (v32sim-peek.sh, v32sim-golden.sh,
# v32sim-session.py, oracle/, ... : what the other projects call through $V32SIM_HOME/tools) to $(USRLIB)/tools
# (~/.local/lib/v32sim/tools, as v32cc's `make install` uses ~/.local/lib/v32cc).
# sysinstall: the binary to $(PREFIX)/bin and the tools to $(VROOT)/v32sim/tools (/usr/local/Vircon32/v32tools/v32sim/tools:
# the Vircon32 tools tree v32cc and v32cxx install into). DESTDIR for packaging. The tools find the binary beside
# themselves (../bin/v32sim, in a checkout), then on $PATH.
# Both remove what an older v32sim installed in share/v32sim (~/share/v32sim, $(PREFIX)/share/v32sim).
PREFIX ?= /usr/local
VROOT  ?= $(PREFIX)/Vircon32/v32tools
USRLIB ?= $(HOME)/.local/lib/v32sim

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
	@echo "Installing the tools to $(USRLIB)/tools"; rm -rf $(USRLIB)/tools ~/share/v32sim; mkdir -p $(USRLIB) && \
		cp -R tools $(USRLIB)/ && find $(USRLIB)/tools -type f -name '*.o' -delete

uninstall:
	rm -f ~/bin/bin.$(ARCH)/$(TARGET) ~/bin/$(TARGET); rm -rf $(USRLIB) ~/share/v32sim

sysinstall: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(VROOT)/v32sim
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	rm -rf $(DESTDIR)$(VROOT)/v32sim/tools $(DESTDIR)$(PREFIX)/share/v32sim
	cp -R tools $(DESTDIR)$(VROOT)/v32sim/ && find $(DESTDIR)$(VROOT)/v32sim/tools -type f -name '*.o' -delete
	@echo "Installed $(TARGET) to $(PREFIX)/bin and the tools to $(VROOT)/v32sim/tools"

sysuninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET); rm -rf $(DESTDIR)$(VROOT)/v32sim $(DESTDIR)$(PREFIX)/share/v32sim

# the archive carries the date of the version: ../v32sim-project-YYYYMMDD.zip
ARCHIVE = ../v32sim-project-$(shell echo $(VERSION_STRING) | cut -c1-8).zip

archive: clean
	@rm -f $(ARCHIVE)
	@zip -r $(ARCHIVE) *

clean:
	@rm -vf .*.sw[op] *.save* *~ core errors *.patch *.zip ../v32sim-project.zip ../v32sim-project-[0-9]*.zip
	@rm -vf $(DEPS) $(GENSRC) $(GENHDR) $(BINDIR)/$(TARGET)
	@rm -rf $(OBJDIR)

.PHONY: all debug asan gen oracle check test-harness version install uninstall sysinstall sysuninstall clean archive

-include $(DEPS)

