

Warning: Here be dragons...

Under active/frantic development...

(Toolboxforth is a partial-rewrite of uForth: Mainly to support Arduino...)

This is "toolboxforth".  It's a FORTH implementation that has been in my "toolbox"
since 2009 (under the name uForth). Back then, I used it primarily on an MSP430.

Toolboxforth is a FORTH implemented in 1 C file (just include "toolboxforth.h" for
easy inclusion into microcontroller projects).
It compiles so fast, why bother making it a separate C file or library???
Yeah, it's ugly.. But, I wanna spend less time abstracting/refactoring the C code and
more time on the FORTH code...

Porting is somewhat easy, but you are going to get your hands dirty. Look at
"toolbox-posix.c" for guidance.. (subject to change).

Toolboxforth is hardcoded for a 16 bit word dictionary 
(max 65535 words -- doesn't sound like a lot, but it has been quite adequate for some major
projects). However, toolboxforth uses 32 bit words for the stacks and RAM.

It has the concept of "tasks", but that code isn't completed yet...

This C code is gnarly, but defines just enough FORTH to bootstrap a more usable FORTH.
See *.f files for more insight into this opinionated implementation...

The FORTH code has direct (via indexing) access to the critical RAM and dictionary
structures/pointers in the C code via @ and dict@ respectively. Take a look at core.f.
You can do some interesting stuff (and a lot crashing) by playing with that. Sometimes
it is used to do some low level manipulation, but often C code is used instead to
be faster.
For example, you should be able to walk the dictionary and mess with it (see the definition
for "word" in util.f for a walking example.

As opinionated as this FORTH implementation may be, it has served me well (in actual
shippable embeded products!) since 2009.

The "posix" (e.g. Linux, OpenBSD, etc) build is really meant to help bootstrap the
microcontroller build and to test out apps before dropping down to a microcontroller.

For example: You use the posix build to generate a dictionary dump that can be
simply included for the microcontroller (currently Arduino) build.

Look at the Makefile for direction... You currently need Arduino IDE to build the
Arduino code... and the Makefile (simply, naively) copies the needed header files
to the subdir.

Right now, only ESP32 is supported..and minimally...more to follow.

You will need libreadline for the posix build...

This repo is mainly for my own "backup" and for the morbidly curious...
as it is moving/evolving rapidly.

About Me:  I'm a FORTHer since 1984.  Have never lost my love/need for FORTH.
I (still) use it for product development and hobbyist hacking...



 

