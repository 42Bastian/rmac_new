#
# RMAC - Renamed Macro Assembler for all Atari computers
# Copyright (C) 199x Landon Dyer, 2011-2021 Reboot & Friends
# MAKEFILE for *nix
#

STD := c99

# Detect old, shitty platforms that aren't C99/POSIX compliant
OSTYPE := $(shell uname -a)

# Should catch MinGW
ifeq "$(findstring MINGW, $(OSTYPE))" "MINGW"
STD := gnu99
endif

# If we're cross compiling using MXE, we're still fooooooooooked
ifneq "$(CROSS)" ""
STD := gnu99
endif


RM = /bin/rm -f
CC = $(CROSS)gcc
HOSTCC = gcc

#CFLAGS = -std=$(STD) -D_DEFAULT_SOURCE -g -D__GCCUNIX__ -I. -O2 -MMD
CFLAGS = -std=$(STD) -D_DEFAULT_SOURCE -g -D__GCCUNIX__ -I. -O2
CFLAGS+= -Wno-pointer-sign -Wno-deprecated-non-prototype

OBJS = 6502.o amode.o debug.o direct.o dsp56k.o dsp56k_amode.o dsp56k_mach.o eagen.o error.o expr.o fltpoint.o listing.o mach.o macro.o mark.o object.o op.o procln.o riscasm.o rmac.o sect.o symbol.o token.o dirent_lose.o

#
# Build everything
#

#all: mntab.h 68ktab.h kwtab.h risckw.h 6502kw.h opkw.h dsp56ktab.h rmac
all: rmac
	@echo
	@echo "Don't forget to bump the version number before commiting!"
	@echo

#
# Generated sources for state machines and keyword, directive and mnemonic
# definitions
#

68ktab.h 68k.tab: 68k.mch 68kgen
	./68kgen 68k.tab <68k.mch >68ktab.h

dsp56ktab.h dsp56k.tab: dsp56k.mch dsp56kgen
	./dsp56kgen dsp56k.tab <dsp56k.mch >dsp56ktab.h


mntab.h: direct.tab 68k.tab kwgen
	cat direct.tab 68k.tab | ./kwgen mn >mntab.h

kwtab.h: kw.tab kwgen
	./kwgen kw <kw.tab >kwtab.h

6502kw.h: 6502.tab kwgen
	./kwgen mp <6502.tab >6502kw.h

risckw.h: risc.tab kwgen
	./kwgen mr <risc.tab >risckw.h

opkw.h: op.tab kwgen
	./kwgen mo <op.tab >opkw.h

68kregs.h: 68kregs.tab kwgen
	./kwgen reg68 <68kregs.tab >68kregs.h

56kregs.h: 56kregs.tab kwgen
	./kwgen reg56 <56kregs.tab >56kregs.h

6502regs.h: 6502regs.tab kwgen
	./kwgen reg65 <6502regs.tab >6502regs.h

riscregs.h: riscregs.tab kwgen
	./kwgen regrisc <riscregs.tab >riscregs.h

unarytab.h: unary.tab kwgen
	./kwgen unary <unary.tab >unarytab.h

# Looks like this is not needed...
dsp56kkw.h: dsp56k.tab kwgen
	./kwgen dsp <dsp56k.tab >dsp56kkw.h

#
# Build tools
#

%gen: %gen.c
	$(HOSTCC) $(CFLAGS) -c $<
	$(HOSTCC) $(CFLAGS) -o $*gen $<

#
# Build RMAC executable
#

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

rmac: $(OBJS)
	$(CC) $(CFLAGS) -o rmac $(OBJS) -lm

#
# Clean build environment
#

clean:
	$(RM) $(OBJS) kwgen.o 68kgen.o rmac kwgen 68kgen 68k.tab kwtab.h 68ktab.h mntab.h risckw.h 6502kw.h opkw.h dsp56kgen dsp56kgen.o dsp56k.tab dsp56kkw.h dsp56ktab.h 68kregs.h 56kregs.h 6502regs.h riscregs.h unarytab.h

#
# Dependencies
#
6502.o: 6502.c direct.h rmac.h symbol.h token.h expr.h error.h mach.h \
 procln.h riscasm.h sect.h kwtab.h 6502regs.h
68kgen: 68kgen.c
amode.o: amode.c amode.h rmac.h symbol.h error.h expr.h mach.h procln.h \
 token.h sect.h riscasm.h kwtab.h mntab.h parmode.h 68kregs.h
debug.o: debug.c debug.h rmac.h symbol.h amode.h direct.h token.h expr.h \
 mark.h sect.h riscasm.h
direct.o: direct.c direct.h rmac.h symbol.h token.h 6502.h amode.h \
 error.h expr.h fltpoint.h listing.h mach.h macro.h mark.h procln.h \
 riscasm.h sect.h kwtab.h 56kregs.h riscregs.h
dsp56k.o: dsp56k.c rmac.h symbol.h dsp56k.h sect.h riscasm.h
dsp56k_amode.o: dsp56k_amode.c dsp56k_amode.h rmac.h symbol.h amode.h \
 error.h token.h expr.h procln.h sect.h riscasm.h kwtab.h mntab.h
dsp56k_mach.o: dsp56k_mach.c dsp56k_mach.h rmac.h symbol.h dsp56k_amode.h \
 amode.h direct.h token.h dsp56k.h sect.h riscasm.h error.h kwtab.h \
 dsp56ktab.h
dsp56kgen: dsp56kgen.c
eagen.o: eagen.c eagen.h rmac.h symbol.h amode.h error.h fltpoint.h \
 mach.h mark.h riscasm.h sect.h token.h eagen0.c
error.o: error.c error.h rmac.h symbol.h listing.h token.h
expr.o: expr.c expr.h rmac.h symbol.h direct.h token.h error.h listing.h \
 mach.h procln.h riscasm.h sect.h kwtab.h
fltpoint.o: fltpoint.c fltpoint.h
kwgen: kwgen.c
listing.o: listing.c listing.h rmac.h symbol.h error.h procln.h token.h \
 sect.h riscasm.h version.h
mach.o: mach.c mach.h rmac.h symbol.h amode.h direct.h token.h eagen.h \
 error.h expr.h procln.h riscasm.h sect.h kwtab.h 68ktab.h
macro.o: macro.c macro.h rmac.h symbol.h debug.h direct.h token.h error.h \
 expr.h listing.h procln.h
mark.o: mark.c mark.h rmac.h symbol.h error.h object.h riscasm.h sect.h
object.o: object.c object.h rmac.h symbol.h 6502.h direct.h token.h \
 error.h mark.h riscasm.h sect.h
op.o: op.c op.h direct.h rmac.h symbol.h token.h error.h expr.h \
 fltpoint.h mark.h procln.h riscasm.h sect.h opkw.h
procln.o: procln.c procln.h rmac.h symbol.h token.h 6502.h amode.h \
 direct.h dsp56kkw.h error.h expr.h listing.h mach.h macro.h op.h riscasm.h \
 sect.h kwtab.h mntab.h risckw.h 6502kw.h opkw.h
riscasm.o: riscasm.c riscasm.h rmac.h symbol.h amode.h direct.h token.h \
 error.h expr.h mark.h procln.h sect.h risckw.h kwtab.h
rmac.o: rmac.c rmac.h symbol.h 6502.h debug.h direct.h token.h error.h \
 expr.h listing.h mark.h macro.h object.h procln.h riscasm.h sect.h \
 version.h
sect.o: sect.c sect.h rmac.h symbol.h riscasm.h 6502.h direct.h token.h \
 error.h expr.h listing.h mach.h mark.h riscregs.h
symbol.o: symbol.c symbol.h error.h rmac.h listing.h object.h procln.h \
 token.h
token.o: token.c token.h rmac.h symbol.h direct.h error.h macro.h \
 procln.h sect.h riscasm.h kwtab.h unarytab.h
dirent_lose.o: dirent_lose.h
