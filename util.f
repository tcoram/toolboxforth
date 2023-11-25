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

\ ** Utilities
\
\ Things not strictly needed, but may be useful.
\

\ exit if stack value is < 1
\
: exit-if0- ( n -- ) 1 < if r> drop then ;

\ exit if stack value is >= 0
\
: exit-if0+ ( n -- ) -1 > if r> drop then ;


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


\ Counts of strings (RAM or dictionary)
\
: count ( addr -- addr cnt ) dup @ swap 1+  swap ;
: dict-count count ;


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
	postpone ," ['] count , 
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
    dup 0> if 0 do  dup i +c@ emit  loop else drop then drop ;

: dict-type type ;
 

: ."   compiling?
    if
	postpone ," ['] count , ['] type ,
    else
	34 word count type
    then
; immediate


\ Print out top item on stack
\
: (.) ( n - ) sidx 1+ exit-if0- >string count type ;
: . ( n - ) (.) 32 emit ;

\ unsigned
\
: (u.) ( u - ) sidx 1+ exit-if0- u>string count type ;
: u. ( u - ) (u.) 32 emit ;

\ hex and binary
\
: h. ( u - ) base @ swap  hex [char] $ emit . base ! ;
: b. ( u - ) base @ swap binary [char] % emit u. base ! ;

\ fixed point
\
: f. ( f - ) 1000000 /mod (.) [char] . emit abs . ;
: fu. ( f - ) 1000000 /mod (u.) [char] . emit u. ;

: .s
    [char] < emit sidx 1+ (.) [char] > emit 32 emit
    sidx 1+ exit-if0-  sidx 1+ 0 do  dsa i + @ . loop ;

\ Traditional "recurse" word for instrumenting recursion.
\
: recurse lwa @ 1+  count 63 and 2 /mod + + [compile] lit , [compile] exec ; immediate

\ Create a word to access memory via +c@ and +c!
\ 
: caddr (  idx addr -<name>-)
  (create) [compile] dlit d, [compile] lit , [compile] ; ;

\ Complimentary to above... instead store the caddr word into A for (c@) and (c!)
\
: A+! ( caddr idx - ) swap A! A+ ;


\ if not true, print out a string and abort to top level
\
: assert" ( f -- )
    [compile] 0=
    postpone if
    postpone ," [compile] count [compile] type
    [compile] abort
    postpone then ; immediate


: dbytes>pad ( dict_addr -- )
    count 0 do dup i +c@ pad c!+ loop ;

: ?dup   dup 0= if drop then ;

: is-immediate? ( addr -- addr flag)
    dup 1+ 128 and ;

: is-primitive? ( addr -- addr flag)
    dup 1+ 64 and ;


\ Function pointers!
\ example:
\   defer WORDS
\   ' words is WORDS

: defer
    (create)
    [compile] dlit
    (allot1) $80000000 or d,
    [compile] @
    [compile] exec
    [compile] ;
;


\ bug.. this doesn't work for primitves!! ???
\
: is ( xt --)
    compiling? if
	[compile] dlit
	postpone '
	d,
	[compile] 1+
	[compile] ddict@
	[compile] !
    else
	postpone ' 1+ ddict@ !
    then ; immediate

\ Synonym of a constant...
\
: value constant ;

\ While you can do defer/is for constants too (constants are just "words")...
\ you can modify a constant directly if you wish.  
\
: to  ( u -<name> )
    compiling? if
	[compile] dlit  postpone ' d, [compile] 1+ [compile] ddict!
    else
	postpone ' 1+ ddict!
    then ; immediate


\ Place a marker
\
: mark here (create) [compile] lit , [compile] ; ;

\ forget everything down to (and including) marker
\
: forget-to-mark ( <marker)  next-word dup (find-head)
    dup if dict@ >r (find-code) exec (here) dict! r> lwa dict! else 2drop  then  ;

: time-it ( addr - )
  ms >r exec ms r> - . ."  ms elapsed" cr ;

\ Step debugger...
\ If DEBUG is 0 -> do nothing
\ If DEBUG is 1 -> print stuff
\ If DEBUG is 2 -> print stuff and stack
\ If DEBUG is 3 -> print stuff and stack and wait for  a key pres
\
0 value DEBUG

: (debug") ( addr count - )
    DEBUG 0 = if drop drop exit then
   ." #DBG " secs . ." : "
    DEBUG 0 > if type then
    DEBUG 1 > if [char] : emit 32 emit .s then
    DEBUG 2 > if key [char] q = if r> exit then then cr ;
    
: debug" ( <string> )
    postpone s" [compile] (debug") ; immediate

: (debug-exec) ( addr - )
    DEBUG 0 = if drop exit then
   ." #DBG " secs . ." : "
    DEBUG 0 > if exec then
    DEBUG 1 > if [char] : emit 32 emit .s then
    DEBUG 2 > if key [char] q = if r> exit then then cr ;

: debug-exec ( <word> )
    postpone ['] [compile] (debug-exec) ; immediate

: .debug ( n - n )
    DEBUG 0 =  if exit then
    DEBUG 0 > if ." #DBG : " dup . cr then ;

: memory
    9 emit
    ." Dict: " here .  ." cells (" here 2* . ." bytes) used out of "
    maxdictcells . ." cells."
    ."  (" here 100 * maxdictcells / . ." % used)." cr
    9 emit
    ." Total RAM : " ramsize . ." cells (" ramsize RCELL * . ." bytes)" cr
    9 emit
    ." Data Stack: " dslen . ." cells. " 
    ." Return Stack: " rslen . ." cells."  cr
    9 emit
    ." User RAM: " uramsize .
    ." cells (" uramsize RCELL * . ." bytes) used out of " uram-size .
    ." cells" cr ;

\ Words  -- this is the longest definition here, but mostly comments...
\   It still should be refactored..
\
: words ( -- )
    cr
    0 R1 !
    lwa	@				\ pointer to last word
    begin
	dup				\ keep it on the stack 
	\ Words look like this: 
	\ [prevlink] [immediate/primitive name length] [name] [code] 
        \ name lengths are stored in the lower 6 bits -> 0x3F=63
	1+ count 63 and dup		\ get length of word's name
	R1 @ + 74 > if cr 0 R1 ! then \ 74 characters per line
	dup R1 +! type 32 emit 32 emit	\ print word's name
	2 R1 +!                        \ add in spaces
	@				\ get prev link from last word
	dup 0 =                        \ 0 link means no more words!
    until
    drop cr ;

: (@+) (c@+) (c@+) 8 lshift + ;

: dump-mem ( cnt &a - )
    A! for (@+) h. r@ 16 mod 0= if cr then next ;

: init ;

