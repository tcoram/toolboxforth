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
: dversion ( - u)  0 dict@ ;
: wordsize ( - u) 1 dict@ ;
: maxdict ( - u) 2 dict@ ;
: (here) ( - a) 3 ;
: lwa  ( - a) 4 ;
: uram-top@ ( -- u) 5 dict@ ;


\ Low level memory access. This is tightly bound to the tbforth implementation
\ of memory (structure members).  Some may not be very useful (especially return stack stuf)
\

\ Machine stuff (iram)
\
: state? ( - f) 0 iram + @  ;
: reset-state ( - ) 0 0 iram + !  ;
: compiling? ( - f) state? 1 = ;
: ramsize ( - f) 1 iram + @  ;
: compiling-word-addr ( -- a) 2 iram + @ ;

\ Task based RAM (uram)
\
: uram-size ( - u) uram 0 + @  ;
: base ( - a) uram 1 +  ;  \ number base address
: sidx ( - u) uram 2 + @ ;  \ data stack pos
: ridx ( - u) uram 3 + @ ; \ return stack pos
: dslen ( - u) uram 4 + @ ; \ data stack length
: rslen ( - u) uram 5 + @ ; \ return stack length
: dsa ( - a)  uram 6 + ; \ data stack address
: rsa ( - a) dsa dslen + ; \ return stack address
: T ( - a) dsa sidx + 1- ; \ top of data stack
: R ( - a) dsa ridx + 1+ ; \ top of return stack

\ Terminal input buffer...
\
: tibidx iram 3 + ;
: tibwordidx iram 4 + ;
: tibwordlen iram 5 + ;
: tib iram 6 + ;		( starts with cnt... )
: tibbuf iram 7 + ;

\ Works but much slower than builtin C version
\ : rpick  dsa ridx + 1+ 1+ + @ ;

\ Stack item count
: depth ( -- u) sidx 1 + ;


\ no operation
\
: nop ;

\ Useful number stuff
\
: negate -1 * ;
: not 0= ;
: 2+ 2 + ;
: 2* 2 * ;
: 2/ 2 / ;


\ Stack manipulation
\
\ : dup ( a -- a a) 0 pick ;
\ : over ( a b -- a b a) 1 pick ;
\ : swap ( a b -- b a) >r >r 1 rpick r> r> drop ;
\ : rot ( a b c -- c b a) >r >r >r 2 rpick 1 rpick r> r> r> 2drop ;
: 2dup ( x y - x y x y)  over over ;
: 2drop ( x y - ) drop drop ;
: nip ( x y - y) over xor xor ;
: 2swap ( a b c d - c d a b) rot >r rot r> ;
: r@ ( - u) 1 rpick ; \ It's 1 cause 0 is the return for this word call...

\ Comparisons
\
\ : > swap < ;
\ : <= 1+ < ;
\ : >= swap <= ;

\ Misc useful stuff
\
: +! ( u a - ) dup >r @ + r> ! ;
: incr ( a - ) 1 swap +! ;
: decr ( a - ) -1 swap +! ;
\ : mod ( y x  -- u)
\    over swap ( y y x) dup >r  ( y y x)
\    / r> * - ;
\ : = ( a b -- f) - 0= ;

: /mod ( u u - r q)
  2dup / >r mod r> ;

\ Index to exec'able code
\
: ' ( -<word>- a) next-word (find-code) ;
: ['] next-word (find-code)  1 ,  , ; immediate \ hack: LIT *is* 1

\ Index to header of word
\
: '& ( -<word>- a) next-word (find-head) ;
: ['&] next-word (find-head)  1 ,  , ; immediate \ hack: LIT *is* 1

\ Helper word for building the below constructs.
\
: [compile]
    postpone [']			\ for later: get word
    ['] ,  ,				\ store a comma (for later get word)
; immediate


\ A create that simply returns address of here after created word.
\
: create
    (create)
    [compile] lit
    here 2+ ,				\ just pass the exit (here after def)
    [compile] ;
;

: variable
    (create)
    [compile] lit
    (allot1) ,
    [compile] ;
;


\ constants are stored in dictionary and are thus limited to 16 bits.
\
: constant ( n -<name>- )
    (create)
    [compile] lit ,
    [compile] ; 
; 


\ dconstants are stored in dictionary as two 16 bit dictionary cells.
\
: dconstant ( d -<name>- )
    (create)
    [compile] dlit d,
    [compile] ; 
;

: dict-d@ ( a - u)
  dup dict@ swap 1+ dict@ swap 16 lshift or ;
  
: dict-d! ( d a - )
    over 16 rshift over dict!
    swap $0000ffff and swap 1+ dict! ;

\ Conditionals

: if ( -- ifaddr)
    [compile] lit			\ precede placeholder
    here				\ push 'if' address on the stack
    0 ,				\ placeholder for 'then' address
    [compile] 0jmp?			\ compile a conditional jmp to 'then'
; immediate

: else ( ifaddr -- elseaddr )
    here 3 +				\ after lit and jmp
    swap dict!				\ fix 'if' to point to else block
    [compile] lit			\ precede placeholder
    here				\ push 'else' address on stack
    0 ,				\ place holder for 'then' address
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
    0 ,				\ placeholder for 'leave' address
    [compile] jmp			\ jmp out of here
; immediate

: +loop ( doaddr n -- )
    [compile] r>			\ ( -- start{idx})
    [compile] +				\ ( -- idx+n)
    [compile] dup			\ ( -- idx idx)
    [compile] >r			\ ( -- idx) store new
    [compile] lit  1 ,
    [compile] rpick			\ ( -- idx limit)
    [compile] >=			\ if idx >= limit, we are done.
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
    0 , \ place holder for later.
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

: dup? dup 0= 0= if dup then ;

: allot ( n -- )  0 do (allot1) drop loop ;

: byte-allot ( n -- ) RAMC /mod + allot  ;

\ Counted byte buffer
\
: cbyte-allot ( n --) 1 allot byte-allot ;
    
\ Point to internally allocated (scratch) PAD
\
uram 6 + dslen + rslen + constant pad

\ Scratch variable (be careful, it doesn't nest and may be modified by anyone)
\
variable A

\ Implement traditional forth "word" using the scratch pad.
\
: word ( delim -- addr)
    A !
    0 pad !
    begin
	next-char dup A @ = over 0= or if drop pad exit then
	pad c!+
    again ;
