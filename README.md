# Warning: Here be dragons...

## Toolboxforth is... say what?

A throwback to an 80s style of roll-your-own-even-if-you-shouldn't language design.

This is "toolboxforth".  It's a FORTH implementation that has been in my "toolbox"
since 2009 (under the name uForth). Back then, I used it primarily on an MSP430.
Today it targets the ESP32, Arduino 32 bit (Cortex M)  and POSIX compliant operating systems.  Backport to MSP430
coming soon!!!

## Features

* ROMable. The dictionary and RAM are separate address spaces (even if dictionary is in RAM).
* Internal data structures/dictionary are FORTH hackable. Use @ ! for all system RAM stuff and dict! dict@ to mess with the dictionary. Dangerous. Expect to crash a lot.
* A good portion is (self) implemented in FORTH. Just enuf C for speed and a minimal FORTH kernel.
* Portable. Just need C. Don't even need a filesystem. MCU dictionaries can be bootraped from POSIX build generated header file.
* 16 and 32 bit MCU friendly. Orignally started life on an MSP430... Tune as you see fit.
* Full interactive console on MCU. Just connect via terminal program at 115200 baud.
* MCU Stub words. Develop MCU targeted apps on desktop and just deploy the image to MCU with minimal dev on MCU.
* FORTH "macros" and compilation via [compile], immediate and postpone. Rewrite the compiler on the fly!
* Tail call optimized recursion.
* Hackable. Everything is hackable. In fact, almost everything is hackable from FORTH itself.
* Inspired by the dozens of FORTHs I've used over the years, but it is it's own thing. Don't expect any standards compliance.
* Ugly. Yeah, this needs not only code refactoring, but conceptual refactoring.
* Stable. Yeah I've shipped stuff using it's prior incarnation as "uforth"
* Constant refactoring: I'm trying to balance making it:
  * Minimal. Never quite as minimal as ColorForth or ArrayForth, but..
  * Fast. Coding essential frequently called words in C
  * Flexible. Defining as much FORTH in FORTH as I can.

Is it for you? Probably not... but if you think so, here it is...

Something to play with or gawk at how wrong it feels..

## Details

I'm gonna use the term POSIX below, but you can subsitute Linux, MacOSX, OpenBSD, etc...

Toolboxforth is a FORTH implemented in 1 C source file.

Porting is somewhat easy, but you are going to get your hands dirty. Look at
"tbforth-posix.c" for guidance.. (subject to change).

Toolboxforth is hardcoded for a 16 bit word dictionary 
(max 65535 words -- doesn't sound like a lot, but it has been quite adequate for some major
projects). However, toolboxforth uses 32 bit words for the stacks and RAM.

All numbers are stored in RAM as 32 bits.  Characters and strings are packed into
32 bit cells, so they have special access words like +c! +c@ and bcopy, where you
must provide an "index" (byte count) into the 32 bit words.

It has the concept of "tasks", but that code isn't completed yet...

This C code is gnarly, but defines just enough FORTH to bootstrap a more usable FORTH.
See *.f files for more insight into this opinionated implementation...

The FORTH code has direct (via indexing) access to the critical RAM and dictionary
structures/pointers in the C code via @ and dict@ respectively. Take a look at core.f.
You can do some interesting stuff (and a lot crashing) by playing with that. 

For example, you should be able to walk the dictionary and mess with it (see the definition
for "words" in util.f for a dictionary reading example.

For a more destructive example, the following (from util.f) implements a "forget" that will
erase words down to a "marker".  See util.f for an example using a marker called TEST

```
: mark ( <name> ) create ;
: forget-to-marker ( <marker> ) next-word dup (find-head)
		   dup if lwa dict! (find-code) exec (here) dict! else drop  then drop ;
```

Yeah, that's gnarly. No documentation yet, but the best way to explore is to start
with core.f and util.f.

As opinionated as this FORTH implementation may be, it has served me well (in actual
shippable embeded products!) since 2009.

The "POSIX" (e.g. Linux, OpenBSD, etc) build is really meant to help bootstrap the
microcontroller build and to test out apps before dropping down to a microcontroller.

For example: You use the POSIX build to generate a dictionary dump that can be
simply included for the microcontroller (currently Arduino ESP32) build.

Look at the Makefile for direction... You currently need Arduino IDE to build the
Arduino ESP32 code... and the Makefile (simply, naively) copies the needed files
to the subdir.

Right now, only ESP32 is supported..and minimally...more to follow.

You will need libreadline for the POSIX build...

This repo is mainly for my own "backup" and for the morbidly curious...
as it is moving/evolving rapidly.

## Who? What?

About Me:  I've been a  FORTHer since 1984.  Yeah, I have never lost my love/need for FORTH.
I (still) use it for product development and hobbyist hacking...



 

