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

# a readline outside the compiler's default search path (macOS: the system "readline" is libedit; Homebrew's keg-only GNU
# readline is the one to use): make READLINE_PREFIX=$(brew --prefix readline)
ifneq ($(strip $(READLINE_PREFIX)),)
    INC  += -I$(READLINE_PREFIX)/include
    LIBS := -L$(READLINE_PREFIX)/lib $(LIBS)
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

# tests that need no toolchain (the cartridges are written by tests/kbd_probe.py): v32kbd delivery and --kbd-sync, --warn-halt, regex: patterns;
# the command line, help texts and exit statuses (run_cli_tests.sh); the built-in v32dev server: V32DEV_SERVER/V32DEV_CHn, serial:tcp,
# SPEC parsing, `dev format` types (run_v32dev_server_tests.sh)
test: $(BIN)
	tests/run_kbd_sync_tests.sh
	tests/run_cli_tests.sh
	tests/run_v32dev_server_tests.sh

# tests of the consolidated tools (needs V32TC, V32BIOS; ORACLE for the comparisons with the stock console logic)
test-harness: $(BIN)
	tests/run_harness_tests.sh

# make version            stamp the version in inc/version.h into TODO.md and every man page .TH line
# make version VERSION=x  write x into inc/version.h first, then stamp it the same way (V=x is the old spelling)
#                         that exists (man/v32sim.1, .es.1, .fr.1: the version, and the month in the page's language, spelled
#                         out here rather than through locales, which may not be installed). These are the only places the
#                         version is written. A VERSION= given with the same value changes nothing but the month of the man pages.
# Portable between GNU and BSD/macOS tools: no `sed -i` (BSD sed takes the script as the backup suffix).
MONTH_N  := $(shell date +%m | sed 's/^0//')
YEAR     := $(shell date +%Y)
MONTHS_EN = January February March April May June July August September October November December
MONTHS_ES = enero febrero marzo abril mayo junio julio agosto septiembre octubre noviembre diciembre
# (accented French months are written as groff \[uXXXX] escapes: the French page is pure ASCII, like the Spanish one)
MONTHS_FR = janvier f\\[u00E9]vrier mars avril mai juin juillet ao\\[u00FB]t septembre octobre novembre d\\[u00E9]cembre
MONTH_en := $(word $(MONTH_N),$(MONTHS_EN)) $(YEAR)
MONTH_es := $(word $(MONTH_N),$(MONTHS_ES)) de $(YEAR)
MONTH_fr := $(word $(MONTH_N),$(MONTHS_FR)) $(YEAR)
# V=x is an older spelling of VERSION=x
# With neither, `make version` stamps the version inc/version.h already has.
NEWVER := $(if $(filter command line,$(origin VERSION)),$(VERSION),$(if $(filter command line,$(origin V)),$(V),$(VERSION_STRING)))
ifneq ($(strip $(NEWVER)),)
version:
	@case '$(NEWVER)' in ''|*[!A-Za-z0-9._+-]*) echo "make version: '$(NEWVER)' is not a valid version (letters, digits and . _ + - only)" >&2; exit 1 ;; esac
	@sed 's/^\(\#define  *VERSION  *\)"[^"]*"/\1"$(NEWVER)"/' inc/version.h > inc/version.h.tmp && mv inc/version.h.tmp inc/version.h
	@sed 's/^\(Status as of version `\)[^`]*`/\1$(NEWVER)`/' TODO.md > TODO.md.tmp && mv TODO.md.tmp TODO.md
	@for l in en es fr; do \
		f=man/$(TARGET).$$l.1; [ $$l = en ] && f=man/$(TARGET).1; \
		[ -f $$f ] || continue; \
		case $$l in en) m='$(MONTH_en)' ;; es) m='$(MONTH_es)' ;; fr) m='$(MONTH_fr)' ;; esac; \
		sed "s/^\.TH V32SIM 1 \"[^\"]*\" \"v32sim [^\"]*\"/.TH V32SIM 1 \"$$m\" \"v32sim $(NEWVER)\"/" $$f > $$f.tmp && mv $$f.tmp $$f; \
	done
	@echo "$(TARGET) $(NEWVER)"
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
# Both also install the manual page (see MANPAGE above), gzipped, as v32sim.1.gz: to $(MANDIR) (sysinstall) or, for the per-user
# install, to $(USRMAN) (~/.local/share/man/man1, found by man-db when ~/.local/bin is on $PATH, from which it derives
# ~/.local/share/man; otherwise add it to MANPATH). It goes beside the helper tools in ~/.local, not into ~/bin, which holds only the binary.
PREFIX ?= /usr/local
VROOT  ?= $(PREFIX)/Vircon32/v32tools
USRLIB ?= $(HOME)/.local/lib/v32sim
MANDIR ?= $(PREFIX)/share/man/man1
USRMAN ?= $(HOME)/.local/share/man/man1

# The manual page. DEST_LANG=en, es or fr installs man/v32sim.$(DEST_LANG).1 as v32sim.1, so `man v32sim` shows it (the
# English page if that translation does not exist yet); unset or DEST_LANG=all installs the English page, man/v32sim.1.
# The page is installed gzipped (gzip -9cn: no name or time stamp, so the file is reproducible) as v32sim.1.gz.
LANGS     = en es fr
DOC_LANG := $(if $(strip $(DEST_LANG)),$(strip $(DEST_LANG)),all)
ifeq ($(filter $(DOC_LANG),all $(LANGS)),)
$(error DEST_LANG must be one of: $(LANGS) or all (got '$(DEST_LANG)'))
endif
ifneq ($(wildcard man/$(TARGET).$(DOC_LANG).1),)
MANPAGE   = man/$(TARGET).$(DOC_LANG).1
else
MANPAGE   = man/$(TARGET).1
endif

install: $(BIN)
	@if [ -d ~/bin/bin.$(ARCH) ]; then \
		echo "Installing $(TARGET) to ~/bin/bin.$(ARCH)/"; \
		install -m 755 $(BIN) ~/bin/bin.$(ARCH)/$(TARGET); \
	else \
		echo "Installing $(TARGET) to ~/bin/"; \
		mkdir -p ~/bin && install -m 755 $(BIN) ~/bin/$(TARGET); \
	fi
	@echo "Installing the manual page to $(USRMAN)"; mkdir -p $(USRMAN) && rm -f $(USRMAN)/$(TARGET).1 && \
		gzip -9cn $(MANPAGE) > $(USRMAN)/$(TARGET).1.gz && chmod 644 $(USRMAN)/$(TARGET).1.gz
	@echo "Installing the tools to $(USRLIB)/tools"; rm -rf $(USRLIB)/tools ~/share/v32sim; mkdir -p $(USRLIB) && \
		cp -R tools $(USRLIB)/ && find $(USRLIB)/tools -type f -name '*.o' -delete

uninstall:
	rm -f ~/bin/bin.$(ARCH)/$(TARGET) ~/bin/$(TARGET) $(USRMAN)/$(TARGET).1 $(USRMAN)/$(TARGET).1.gz; rm -rf $(USRLIB) ~/share/v32sim

sysinstall: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(VROOT)/v32sim $(DESTDIR)$(MANDIR)
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	rm -f $(DESTDIR)$(MANDIR)/$(TARGET).1 $(DESTDIR)$(MANDIR)/$(TARGET).1.gz
	gzip -9cn $(MANPAGE) > $(DESTDIR)$(MANDIR)/$(TARGET).1.gz
	chmod 644 $(DESTDIR)$(MANDIR)/$(TARGET).1.gz
	rm -rf $(DESTDIR)$(VROOT)/v32sim/tools $(DESTDIR)$(PREFIX)/share/v32sim
	cp -R tools $(DESTDIR)$(VROOT)/v32sim/ && find $(DESTDIR)$(VROOT)/v32sim/tools -type f -name '*.o' -delete
	@echo "Installed $(TARGET) to $(PREFIX)/bin, its manual page ($(MANPAGE), gzipped) to $(MANDIR) and the tools to $(VROOT)/v32sim/tools"
ifneq ($(filter es fr,$(DOC_LANG)),)
ifeq ($(MANPAGE),man/$(TARGET).1)
	@echo "(there is no man/$(TARGET).$(DOC_LANG).1 yet: the manual page is the English one)"
endif
endif

sysuninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET) $(DESTDIR)$(MANDIR)/$(TARGET).1 $(DESTDIR)$(MANDIR)/$(TARGET).1.gz; rm -rf $(DESTDIR)$(VROOT)/v32sim $(DESTDIR)$(PREFIX)/share/v32sim

# the archive carries the date of the version: ../v32sim-project-YYYYMMDD.zip; it holds everything in the project directory
# (src, inc, docs, man, tools, tests, ..., and the dotfiles `*` would skip: .github, .gitignore, .gitattributes)
ARCHIVE = ../v32sim-project-$(shell echo $(VERSION_STRING) | cut -c1-8).zip

archive: clean
	@rm -f $(ARCHIVE)
	@zip -r $(ARCHIVE) * $(wildcard .github .gitignore .gitattributes) -x '*.zip' '*.patch' 'obj/*' 'bin/v32sim' 'dist/*' 'tools/oracle/bin/*'

# make dist [DIST_OS=linux DIST_ARCH=x86_64]: a release zip, v32sim-<version>-<os>-<arch>.zip, laid out for unpacking into
# <Program Files>/Vircon32/v32tools (or /usr/local/Vircon32/v32tools), like the other v32 tools:
#     v32sim                       the binary (stripped by the release workflow), at the top
#     LICENSE
#     docs/v32sim/README*.md       every language, with docs/ beside them so their links work
#     docs/v32sim/docs/USAGE*.md
#     docs/v32sim/v32sim*.1        the manual pages (English, Spanish, French)
#     tools/v32sim/                the helper scripts; they find the binary through $V32SIM or $PATH
# It packages what is built (run `make` first); needs zip. The staging directory is dist/.
DIST_OS   ?= $(shell uname -s | tr 'A-Z' 'a-z' | sed 's/^darwin$$/macos/')
DIST_ARCH ?= $(shell uname -m | sed 's/^aarch64$$/arm64/;s/^amd64$$/x86_64/')
DISTZIP    = $(TARGET)-$(VERSION_STRING)-$(DIST_OS)-$(DIST_ARCH).zip

dist: $(BIN)
	@test -n "$(VERSION_STRING)" || { echo "dist: no VERSION in inc/version.h"; exit 1; }
	rm -rf dist $(DISTZIP)
	mkdir -p dist/docs/$(TARGET)/docs dist/tools
	cp $(BIN) dist/$(TARGET)
	cp README*.md dist/docs/$(TARGET)/
	if [ -f LICENSE ]; then cp LICENSE dist/; fi
	cp docs/USAGE*.md dist/docs/$(TARGET)/docs/
	cp man/*.1 dist/docs/$(TARGET)/
	cp -R tools dist/tools/$(TARGET) && rm -rf dist/tools/$(TARGET)/oracle/bin && find dist/tools -type f -name '*.o' -delete
	cd dist && zip -qr ../$(DISTZIP) .
	rm -rf dist
	@echo "built $(DISTZIP)"

clean:
	@rm -vf .*.sw[op] *.save* *~ core errors *.patch *.zip ../v32sim-project.zip ../v32sim-project-[0-9]*.zip
	@rm -vf $(DEPS) $(GENSRC) $(GENHDR) $(BINDIR)/$(TARGET)
	@rm -rf $(OBJDIR) dist

.PHONY: all debug asan gen oracle check test test-harness version dist install uninstall sysinstall sysuninstall clean archive

-include $(DEPS)

