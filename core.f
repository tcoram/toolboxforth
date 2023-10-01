: \ next-char 0 - 3 0skip? \ ; immediate
: ( next-char 41 - 3  0skip? ( ; immediate ( enable stack comments )
: { next-char 125 - 3  0skip? { ; immediate { Alternate comment }

\ The above 3 lines bootstrap in the ability to handle comments.

\  toolboxforth - A tiny ROMable 16/32-bit FORTH-like scripting language
\          for microcontrollers.
\
\  Copyright © 2009-2023 Todd Coram, todd@maplefish.com, USA.
\
\  Permission is hereby granted, free of charge, to any person obtaining
\  a copy of this software and associated documentation files (the
\  "Software"), to deal in the Software without restriction, including
\  without limitation the rights to use, copy, modify, merge, publish,
\  distribute, sublicense, and/or sell copies of the Software, and to
\  permit persons to whom the Software is furnished to do so, subject to
\  the following conditions:
\  
\  The above copyright notice and this permission notice shall be
\  included in all copies or substantial portions of the Software.
\  
\  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
\  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
\  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
\  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
\  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
\  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
\  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


\ A RAM CELL is always 32 bits (4 bytes).
\ A Dictionary CELL is always 16 bits (2 bytes).
\
: RAMC 4 ;
: DCELL 2 ;
 
\ Low level dictionary access.
\
: dversion 0 dict@ ;
: wordsize 1 dict@ ;
: maxdict 2 dict@ ;
: _here 3 ;
: lwa 4 ;
: uram-top@ ( -- n) 5 dict@ ;


\ Low level memory access. This is tightly bound to the tbforth implementation
\ of memory (structure members).  Some may not be very useful (especially return stack stuf)
\
: iram ram 0 + ;
: state? ( -- flag) 0 iram + @  ;
: reset-state ( -- ) 0 0 iram + !  ;
: compiling? ( -- flag) state? 1 = ;
: ramsize ( -- flag) 1 iram + @  ;
: compiling-word-addr ( -- addr) 2 iram + @ ;

: uram-size ( -- uram_length) uram 0 + @  ;
: base ( -- base_addr) uram 1 +  ;
: sidx ( -- dstack_idx) uram 2 + @ ;
: ridx ( -- rtack_idx) uram 3 + @ ;
: dslen ( -- dstack_len) uram 4 + @ ;
: rslen ( -- rstack_len) uram 5 + @ ;
: dsa ( -- dstack_addr)  uram 6 + ;
: rsa ( -- rstack_addr) dsa dslen + ;
: T ( -- n) dsa sidx + 1- ;
: R ( -- n) dsa ridx + 1+ ;

: tibidx iram 4 + ;
: tibwordidx iram 5 + ;
: tibwordlen iram 6 + ;
: tib iram 7 + ;		( starts with cnt... )
: tibbuf iram 8 + ;

\ Works but much slower than builtin C version
\ : rpick  dsa ridx + 1+ 1+ + @ ;

\ Stack item count
: depth ( -- n) sidx 1 + ;


\ no operation
\
: nop ;

\ Useful number stuff
\
: negate -1 * ;
: not 0= ;
: 2+ 2 + ;
: 2* 2 * ;


\ Stack manipulation
\
\ : dup ( a -- a a) 0 pick ;
\ : over ( a b -- a b a) 1 pick ;
\ : swap ( a b -- b a) >r >r 1 rpick r> r> drop ;
\ : rot ( a b c -- c b a) >r >r >r 2 rpick 1 rpick r> r> r> 2drop ;
: 2dup ( a b -- a b a b) over over ;
: 2drop ( a b -- ) drop drop ;
: nip ( a b -- b) over xor xor ;

: r@ ( -- a) 1 rpick ; \ It's 1 cause 0 is the return for this word call...

\ Comparisons
\
: < ( a b -- f) - <0 ;
: > swap < ;

: <= 1+ < ;
: >= swap <= ;

\ Misc useful stuff
\
: +! ( n addr -- ) dup >r @ + r> ! ;
: incr ( addr -- ) 1 swap +! ;
: decr ( addr -- ) -1 swap +! ;
: mod ( y x  -- n)
    over swap ( y y x) dup >r  ( y y x)
    / r> * - ;
\ : = ( a b -- flag) - 0= ;

: ' ( -- addr) next-word (find) ;
: ['] next-word (find)  1 ,  , ; immediate \ hack: LIT *is* 1
: '& ( -- addr) next-word (find&) ;
: ['&] next-word (find&)  1 ,  , ; immediate \ hack: LIT *is* 1

\ Helper word for building the below constructs.
\
: [compile]
    postpone [']			\ for later: get word
    ['] ,  ,				\ store a comma (for later get word)
; immediate


\ A create that simply returns address of here after created word.
\
: create
    _create
    [compile] lit
    here 2+ ,				\ just pass the exit (here after def)
    [compile] ;
;

: variable
    _create
    [compile] lit
    _allot1 ,
    [compile] ;
;

: constant ( n -- )
    _create
    [compile] lit ,
    [compile] ; 
; 

: dconstant ( d -- )
    _create
    [compile] dlit d,
    [compile] ; 
; 

    
\ Conditionals

: if ( -- ifaddr)
    [compile] lit			\ precede placeholder
    here				\ push 'if' address on the stack
    dummy,				\ placeholder for 'then' address
    [compile] 0jmp?			\ compile a conditional jmp to 'then'
; immediate

: else ( ifaddr -- elseaddr )
    here 3 +				\ after lit and jmp
    swap dict!				\ fix 'if' to point to else block
    [compile] lit			\ precede placeholder
    here				\ push 'else' address on stack
    dummy,				\ place holder for 'then' address
    [compile] jmp			\ compile an uncoditional jmp to 'then'
; immediate


: then ( ifaddr|elseaddr -- )
    here swap dict!			\ resolve 'if' or 'else' jmp address
; immediate


\ Looping
\

\ First "for". It's simpler and faster than do loop... but not as flexible
\
: for ( cnt -- faddr)
  [compile] >r
  here ; immediate

: next ( faddr -- )
  [compile] r>
  [compile] 1-
  [compile] >r
  [compile] lit 0 ,
  [compile] rpick   
  [compile] 0=
  [compile] lit
  ,					\ store for's here
  [compile] 0jmp?
  [compile] r> [compile] drop ; immediate

variable _leaveloop

: do ( limit start -- doaddr)
    [compile] swap			( -- start limit)
    [compile] >r  [compile] >r		\ store them on the return stack
    here				\ push address of 'do' onto stack
; immediate ( -- r:limit r:start)

: i ( -- inner_idx)
    [compile] lit 0 , [compile] rpick	\ pick index from return stack
; immediate

: j ( -- outer_idx)
    [compile] lit 2 , [compile] rpick	\ pick index from return stack
; immediate

: leave ( -- )
    [compile] lit			\ precede placeholder
    here _leaveloop !			\ store address for +loop resolution
    dummy,				\ placeholder for 'leave' address
    [compile] jmp			\ jmp out of here
; immediate

: +loop ( doaddr n -- )
    [compile] r>			\ ( -- start{idx})
    [compile] +				\ ( -- idx+n)
    [compile] dup			\ ( -- idx idx)
    [compile] >r			\ ( -- idx) store new
    [compile] lit  1 ,
    [compile] rpick			\ ( -- idx limit)
    [compile] swap			\ ( -- limit idx)
    [compile] -
    [compile] 0=			\ if idx == limit, we are done.
    [compile] lit
    ,					\ store doaddr
    [compile] 0jmp?
    _leaveloop @ 0 > if			\ if we have a leave, resolve it
	here  _leaveloop @ dict!	\ This is where 'leave' wants to be
	0 _leaveloop !
    then
    [compile] r>   [compile] r>		\ clean up
    [compile] drop [compile] drop
; immediate

: loop  ( doaddr -- )
    [compile] lit 1 ,
    postpone +loop
; immediate

: unloop ( -- )
    [compile] r>   [compile] r>		\ clean up
    [compile] drop [compile] drop
; immediate

: exit  r> drop ;

: begin  here ; immediate

: again ( here -- )
    [compile] lit
    ,
    [compile] jmp
; immediate

: while
    [compile] lit
    here
    dummy, \ place holder for later.
    [compile] 0jmp?
; immediate

: repeat
    [compile] lit
    >r ,
    [compile] jmp
    here r> dict! \ fix WHILE
; immediate

: until
    [compile] lit
    ,
    [compile] 0jmp?
; immediate


\ Function pointers!

: defer
    _create
    [compile] lit
    _allot1 ,
    [compile] @
    [compile] exec
    [compile] ;
;

: is ( xt --)
    compiling? if
	[compile] lit
	postpone '
	,
	[compile] 1+
	[compile] @
	[compile] !
    else
	postpone ' 1+ @ !
    then
; immediate

: dup? dup 0= 0= if dup then ;

: allot ( n -- )  0 do _allot1 drop loop ;


\ Allocate 160 bytes... should be enough...otherwise bump it up
\
variable pad 160 RAMC / allot
variable _delim
: word ( delim -- addr)
    _delim !
    0 pad !
    begin
	next-char dup _delim @ = over 0= or if drop pad exit then
	pad c!+
    again ;
