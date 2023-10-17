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


\ exit if stack value is < 1
\
: exit-if0- ( n -- ) 1 < if r> drop then ;

\ exit if stack value is >= 0
\
: exit-if0+ ( n -- ) -1 > if r> drop then ;


\ Allocate a character buffer 
\
\ : c-allot ( n -- ) CELL / allot ;

\ Variable used to keep track of how many 'of' clauses we have.
\
variable _endof

: case ( n -- )
    0 _endof !
   [compile] >r
; immediate

: of ( n -- )
    [compile] r@
    [compile] =
    postpone if
; immediate

: endof ( -- )
    _endof incr
    postpone else
; immediate

: endcase ( -- )
    _endof @ 0 do postpone then loop
    [compile] r>
    [compile] drop
; immediate


\ Counts of RAM strings
\
: count ( addr -- addr cnt ) dup @ swap 1+  swap ;

\ Counts of dict strings
\
: dict-count ( addr -- addr cnt ) dup dict@ swap 1+  swap ;

: hex 16 base ! ;
: decimal 10  base ! ;
: binary 2 base ! ;

: abs ( n -- n) dup exit-if0+ -1 * ;

: min ( a b -- a|b) over over > if swap drop else drop then ;
: max ( a b -- a|b) over over < if swap drop else drop then ;

: cr 13 emit 10 emit ;
: space 32 emit ;

: char ( <char> -- c)
    32 word 1+ @ 255 and ;

: [char]
    [compile] lit
    32 word 1+ @ ,
; immediate

: s"   compiling?
    if 
	postpone ," ['] dict-count , 
    else
	34 word count
    then
; immediate

: c"   compiling?
    if 
	postpone ," 
    else
	34 word
    then
; immediate

: type ( addr count - )
     0 do  dup i +c@ emit  loop drop ;

: dict-type ( addr count - )
    0 do  dup i +dict-c@ emit  loop drop ;


: ."   compiling?
    if
	postpone ," ['] dict-count , ['] dict-type ,
    else
	34 word count type
    then
; immediate


\ Print out top item on stack
\
: . ( n -- ) sidx 1+ exit-if0- >string count type 32 emit ;

: u. ( u -- ) sidx 1+ exit-if0- u>string count type 32 emit ;
: h. ( u -- ) hex u. decimal ;

: .s
    [char] < emit sidx 1+ . [char] > emit 32 emit
    sidx 1+ exit-if0-  sidx 1+ 0 do  dsa i + @ . loop ;

\ Interactive debugging help
\
: debug-pre cr ." debug: " ;
: debug  cr ."     stack -> [" .s ." ]" cr key drop ;
: debug" [compile] debug-pre
    postpone ," [compile] dict-count [compile] dict-type  [compile] debug ; immediate

\ if not true, print out a string and abort to top level
\
: assert" ( f -- )
    [compile] 0=
    postpone if
    postpone ," [compile] dict-count [compile] dict-type
    [compile] abort
    postpone then
; immediate

\ Version stuff. Show off how we handle strings.
\
: memory
    9 emit
    ." Dict: " here .  ." cells (" here 2* . ." bytes) used out of "
    maxdict . ." cells."
    ."  (" here 100 * maxdict / . ." % used)." cr
    9 emit
    ." Total RAM : " ramsize . ." cells (" ramsize wordsize * . ." bytes)" cr
    9 emit
    ." Data Stack: " dslen . ." cells. " 
    ." Return Stack: " rslen . ." cells."  cr
    9 emit
    ." User RAM: " uram-top@ .
    ." cells (" uram-top@ wordsize * . ." bytes) used out of " uram-size .
    ." cells" cr ;

\ Words
\
variable _cc			\ keep track of # of characters on a line
: words ( -- )
    cr
    0 _cc !
    lwa	dict@				\ pointer to last word
    begin
	dup				\ keep it on the stack 
	\ Words look like this: 
	\ [prevlink] [immediate/primitive name length] [name] [code] 
        \ name lengths are stored in the lower 6 bits -> 0x3F=63
	1+ dict-count 63 and dup		\ get length of word's name
	_cc @ + 74 > if cr 0 _cc ! then \ 74 characters per line
	dup _cc +! dict-type 32 emit 32 emit	\ print word's name
	2 _cc +!                        \ add in spaces
	dict@				\ get prev link from last word
	dup 0 =                        \ 0 link means no more words!
    until
    drop cr ;

: dbytes>pad ( dict_addr -- )
    dict-count 0 do dup i +dict-c@ pad c!+ loop ;

: ?dup   dup 0= if drop then ;
: is-immediate? ( addr -- addr flag)
    dup 1+ 128 and ;


\ Function pointers!
\ example:
\   defer WORDS
\   ' words is WORDS

: defer
    (create)
    [compile] lit
    (allot1) ,
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
	[compile] dict@
	[compile] !
    else
	postpone ' 1+ dict@ !
    then
; immediate

\ Synonym of a constant...
\
: value constant ;

\ While you can do defer/is for constants too (constants are just "words")...
\ you can modify a constant directly if you wish.  
\
: to  ( u -<name> )
    compiling? if
	[compile] lit  postpone ' , [compile] 1+ [compile] dict!
    else
	postpone ' 1+ dict!
    then ; immediate


\ Place a marker
\
: mark ( <marker> ) create ;

\ forget everything down to (and including) marker
\
: forget-to-mark ( <marker)  next-word dup (find-head)
    dup if dict@ >r (find-code) exec (here) dict! r> lwa dict! else 2drop  then  ;


: time-it ( addr - )
  ms >r exec ms r> - . ."  ms elapsed" cr ;


: init ;
