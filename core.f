(create) : 1 iram ! (create) 1 iram ! here 2 iram + !  ;
: \ next-char 0 - 3 0skip? \ ; immediate
;

\ 
\ Toolboxforth - A tiny ROMable 16/32-bit FORTH-like scripting language
\ for microcontrollers.
\
\ ~(This file is structure in [Knit (http://maplefish.com/todd/knit.html)]
\ for [AFT  (http://maplefish.com/todd/aft.html)] documentation processing)~
\
\ *TOC
\ ^<<
\ Copyright © 2009-2023 Todd Coram, todd@maplefish.com, USA.
\
\ Permission is hereby granted, free of charge, to any person obtaining
\ a copy of this software and associated documentation files (the
\ "Software"), to deal in the Software without restriction, including
\ without limitation the rights to use, copy, modify, merge, publish,
\ distribute, sublicense, and/or sell copies of the Software, and to
\ permit persons to whom the Software is furnished to do so, subject to
\ the following conditions:
\ 
\ The above copyright notice and this permission notice shall be
\ included in all copies or substantial portions of the Software.
\ 
\ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
\ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
\ MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
\ NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
\ LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
\ OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
\ WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
\ ^>>
\
\ ** Preliminaries
\
\ The first line in this file defines our major defining word: ":"
\ The following line bootstraps in the ability to handle comments.
\ (Notice that is tail call recursive!)
\
\ We could define it clearer using words we define later as:
\ | (create) : is-compiling (create) is-compiling here compiling-word! ; |
\
\ The first is-compiling ensures that (create) is called when : is run
\ The second is-compiling then assures that we are in compilation.
\
\ (FWIW, the lone ";" on line 2, of this file,  is just for Emacs (Forth mode)
\ since the first line confuses it and looks like the ; is commented out.
\ Of course, it isn't...)

\ This file is meant as much to be read by you, dear reader, as by the Toolboxforth
\ interpreter. While not a "literate" program, I wanted to make this the canonical
\ document for how things work.    
\
\ In this file we shall see some primitive words commented out. This is because
\ they were more efficiently coded in C, but you could very well uncomment them
\ as a demonstration of how small the C core can be!

\ Generally, this documentation is a reminder to myself of how Toolboxforth (tbforth)
\ is implemented. 
    
\ Here is another useful comment word: stack effects. It cannot span multiple lines...
\

: ( next-char 41 - 3  0skip? ( ; immediate \ enable stack comments  ) ;

\ ** Dictionary and Memory
\
\ First off..
\ A RAM CELL is always 32 bits (4 bytes).
\ A Dictionary CELL is always 16 bits (2 bytes).
\
: RCELL 4 ;
: DCELL 2 ;
 
\ *** Dictionary
\ The dictionary starts at address 0. We have a single dictionary and the header
\ describes the essentials:
\
: dversion ( - u)  0 @ ;		  \ version of dictionary
: cellsize ( - u) 1 @ ;			  \ size of cell (always 2, for now at least)
: maxdictcells ( - u) 2 @ ;		  \ max size of dictionary in cells
: (here) ( - a) 3 ;			  \ address of here
: lwa  ( - a) 4 ;			  \ address of last (or current) word defined
: uram-used ( -- u) 5 @ ;		  \ size of used uram in RCELLs


\ *** IRAM
\
\ iram contains global information tied to the interpreter and compiler.
\
: state? ( - f) 0 iram + @  ;		  \ what state are we in?
: reset-state ( - ) 0 0 iram + !  ;	  \ force not compiling mode
: is-compiling ( - ) 1 0 iram + ! ;	  \ force compiling  mode
: compiling? ( - f) state? 1 = ;	  \ are we compiling?
: compiling-word! ( a --) 2 iram + ! ;	  \ save the address of word being compiled
: ramsize ( - u) 1 iram + @  ;		  \ size of ram

\ **** Terminal input buffer:
\
: tibidx iram 3 + ;
: tibwordidx iram 4 + ;
: tibwordlen iram 5 + ;
: tib iram 6 + ;		( starts with cnt... )
: tibbuf iram 7 + ;

\ *** URAM
\
\ uram is memory that will hold your application/task data.
\ It also contains the data and return stacks. 
\
: uram-size ( - u) uram 0 + @  ;	  	\ size of uram
: base ( - a) uram 1 +  ;			\ number base address
: fp-places ( - a) uram 2 + ;			\ dec places for fixed point (default: 5)
: sidx ( - u) uram 3 + @ ;			\ data stack pos
: ridx ( - u) uram 4 + @ ;			\ return stack pos
: dslen ( - u) uram 5 + @ ;			\ data stack length
: rslen ( - u) uram 6 + @ ;			\ return stack length
: dsa ( - a)  uram 7 + ;			\ data stack address
: rsa ( - a) dsa dslen + ;			\ return stack address
: T ( - a) dsa sidx + 1- ;			\ top of data stack
: R ( - a) dsa ridx + 1+ ;			\ top of return stack


\ ** Core words in C
\
\	* lit    ( - u )  - pulls the next item in dictionary as a 16 bit number
\	* dlit   (  - u ) - pulls the next item in dictionary as a 32 bit number
\	* drop   ( u - )  - drops top item from stack
\	* jmp    ( a - ) - Jumps to dictionary address
\	* 0jmp?  ( a f - ) - Jumps to address if f is 0
\	* 0skip? ( u f - ) - Skips ahead u DCELLS if f is 0
\	* ,      ( u - ) - lays 16 bit number into dictionary in increments here
\	* d,     ( u - ) - lays 32 bit number into dictionary in increments here (by DCELL)
\	* + * /  - and or xor ( u1 u2 - u3) - binary operators
\	* lshift rshift ( u b - u) - shift u left/right b bits
\	*  */ ( u1 u2 u3 - u4) - multiply u1 and u2 (to a temp 64 bit result) and divide by u3
\	* 0= 0< ( u - f) - comparisons against 0
\	* >r r>  - push or pop value to/from return stack
\	* !  @ - store/retrieve 32 bit values to/from either RAM or dictionary address
\	* +c! +c@ - Store/retrieve a byte to "counted" variable at address and increment count
\	* A! ( a - ) - Store RAM or dictionary address in A (mainly to access as 8 bit values)
\	* A+ ( u - ) - Adjust address pointer in A by u bytes
\	* (c@) ( - c) - Retrieve a byte from address pointed to by A
\	* (c!) ( c -) - Store a byte into  address pointed to by A
\	* (c@+) ( - c) - Retrieve a byte from address pointed to by A and incr A
\	* (c!+) ( - c) - Store a byte at address pointed to by A and incr A
\	* (create) ( <name> ) - Create a dictionary entry with name
\	* next-char ( - c) - retrieve next character from tib
\	* next-word ( - a u) - retrieve next word from tib and return addr and count
\	* (find) ( a u - h c) - Find head & code of word definition for counted word string at a 
\	* ; ( - ) - terminate a word definition (go out of compile state)
\	* immediate - mark last compiled word as "immediate"
\	* (allot1) - allocate 1 RCELL in RAM
\	* bcopy -
\	* bstr= -
\	* >num - 
\	* u>string -
\	* exec  ( a - ) - execute code address on stack
\	* uram ( - a) - address of user ram (e.g. stacks, variables, etc)
\	* uram! ( a - ) - set address of user ram  (e.g. stacks, variables, etc)
\	* iram ( - a) - address of iram
\	* interpret ( - f) - run outer intepreter on contents of tib and return status
\	* cf - ( ... u - ?) - execute extended C function
\	* here (  - a) - address of top of dictionary
\	* cold ( - ) reset dictionary to original state and restart interpreter
\	* abort ( - ) abort to top level interpretr
\
\ *** Optional C core
\
\ The following words are only defined in C for speed. They could just as well
\ be easily defined here.
\ ^<<
\ : 1+ 1 + ;
\ : 1- 1 - ;
\ : rpick  dsa ridx + 1+ 1+ + @ ;
\ : r@ ( - u ) R @ ;
\ : dup ( u - u u ) T @ ;
\ : over ( a b - a b a) T 1- @ ;
\ : swap ( a b - b a) >r >r 1 rpick r> r> drop ;
\ : rot ( a b c - c b a) >r >r >r 2 rpick 1 rpick r> r> r> drop drop ;
\ : mod ( y x  - u) over swap ( y y x) dup >r  ( y y x)  / r> * - ;
\ : = ( a b - f) - 0= ;
\ : 0> ( a - f ) swap 0< ;
\ : < ( a b - f) - 0< ;
\ : > ( a b - f) swap < ;
\ : >= ( a b - ) - 0< 0= ;
\ ^>>

\ Stack item count
\
: depth ( -- u) sidx 1  + ; 

\ Useful numeric short cuts.
\
: negate -1 * ;
: not 0= ;
: 2+ 2 + ;
: 2* 2 * ;
: 2/ 2 / ;


\ Stack manipulation
\
: 2dup ( x y - x y x y)  over over ;
: 2drop ( x y - ) drop drop ;
: nip ( x y - y) over xor xor ;
: 2swap ( x1 y1 x2 y2 - x2 y2 x1 y1) rot >r rot r> ;

\ Misc useful stuff
\
: +! ( u a - ) dup >r @ + r> ! ;
: incr ( a - ) 1 swap +! ;
: decr ( a - ) -1 swap +! ;
: /mod ( u u - r q)  2dup / >r mod r> ;

\ ** Word Creation and Addressing
\
: (find-head) (find)  drop ;
: (find-code) (find) swap drop ;

\
\ Index to exec'able code of a word
\
: ' ( -<word>- a) next-word (find-code) ;
: ['] next-word (find-code)  1 ,  , ; immediate \ hack: LIT *is* 1

\ Index to header of word.
\
: '& ( -<word>- a) next-word (find-head) ;
: ['&] next-word (find-head)  1 ,  , ; immediate \ hack: LIT *is* 1

\ postpone and [compile]
\ Helper words for building the below constructs. There isn't much difference between
\ \[compile] and postpone, but the extra level of indirection is useful: postpone
\ "postpones" an (immediate) word's invocation
\ while \[compile] explicitely lays down a word into the caller's definition.
\
: postpone next-word (find-code) , ; immediate

: [compile]
    postpone [']			\ for later: get word
    ['] ,  ,				\ store a comma (for later get word)
; immediate

\ A create that simply returns address of here after created word.  There isn't currently
\ a DOES> companion... I'm not sure it is needed.
\
: create
    (create)
    [compile] lit
    here 2+ ,				\ just pass the exit (here after def)
    [compile] ;
;

\ Variable names are in the dictionary...however their values are in RAM.
\
: variable
    (create)
    [compile] dlit
    (allot1)  d,
    [compile] ;
;

\ constants are stored in dictionary as two 16 bit dictionary cells.
\
: constant ( d -<name>- )
    (create)
    [compile] dlit d,
    [compile] ; 
;

\ More explicit stuff... for when we really, really want to be specific
\ that we are manipulating dictionary space and not RAM space.
\ It's worth noting here (and now), that we are a Harvard architecture Forth.
\ Dictionary and RAM are two different spaces.  This makes it easier to port
\ to microcontrollers with distinct Flash and RAM (e.g. if you wish to execute
\ the dictionary from Flash, etc).
\
\ @ and !  are context sensitive. A RAM address is a 32 bit word with the leftmost bit
\ set. A Dictionary address is a 32 bit word without the leftmost bit set.  This still
\ allows for a massive amount of RAM to be addressed (536,870,911 RCELLS).
\
: is-ram? ( a - f) $80000000 and ;

\ Deprecated dictionary storge words.. use ! and @  these may go away...
\
: dict@ ( a - u) [compile] @ ; immediate
: dict! ( u a - ) [compile] ! ; immediate
: ddict@ ( a - u) dup dict@ swap 1+ dict@ swap 16 lshift or ;
: ddict! ( d a - )
    over 16 rshift over dict!
    swap $0000ffff and swap 1+ dict! ;

\ ** Conditionals...
\
\ Here is the traditional if else then.  I am going to try and
\ reduce the usage of 'else' as it tends to make for verbose word definitions
\ (refactor!), but it is here if you need it.
\
: if ( -- ifaddr)
    [compile] lit			\ precede placeholder
    here				\ push 'if' address on the stack
    0 ,					\ placeholder for 'then' address
    [compile] 0jmp?			\ compile a conditional jmp to 'then'
; immediate

: else ( ifaddr -- elseaddr )
    here 3 +				\ after lit and jmp
    swap dict!				\ fix 'if' to point to else block
    [compile] lit			\ precede placeholder
    here				\ push 'else' address on stack
    0 ,					\ place holder for 'then' address
    [compile] jmp			\ compile an uncoditional jmp to 'then'
; immediate

: then ( ifaddr|elseaddr -- )
    here swap dict!			\ resolve 'if' or 'else' jmp address
; immediate

\ I mentioned earlier that I would like to remove excessive uses of "else", well one
\ way to do this is to simple "exit"  before then and put the "else" words after then.
\
\ : exit  r> drop ;
: exit  [compile] ;  ; immediate

\ **Looping
\

\ *** Counted Looping
\
\ First "for". It's simpler and a bit faster than do loop... but not as flexible
\ It also always execute at least once.. regardless of value. For uses a
\ count down timer stored on the return stack, so you need to be careful to not
\ keep any of your data on the return stack by the time "next" is called.
\
: for ( cnt -- faddr)
  [compile] 1-
  [compile] >r
  here ; immediate

: next ( faddr -- )
  [compile] r>
  [compile] 1-
  [compile] dup
  [compile] >r
  [compile] 0<
  [compile] lit
  ,					\ store for's here
  [compile] 0jmp?
  [compile] r> [compile] drop ; immediate

\ Now, the traditional do loop. 
\ It gets verbose/complicated pretty quickly.
\
: do ( limit start -- doaddr)
    [compile] swap			( -- start limit)
    [compile] >r  [compile] >r		\ store them on the return stack
    here  				\ push address of 'do' onto stack
; immediate ( -- r:limit r:start)

: i ( -- inner_idx)
    [compile] lit 0 , [compile] rpick	\ pick index from return stack
; immediate

: j ( -- outer_idx)
    [compile] lit 2 , [compile] rpick	\ pick index from return stack
; immediate

\ We need a temporary variable for resolving how/where to leave...
\
variable _leaveloop
: leave ( -- )
    [compile] lit			\ precede placeholder
    here _leaveloop !			\ store address for +loop resolution
    0 ,				\ placeholder for 'leave' address
    [compile] jmp			\ jmp out of here
; immediate

\ Remember, no decrementing loops... need a -loop for this. Surprisingly, I
\ think this matches the standard...
\
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

\ This is the proper way to prematurely exit a do loop.  You just can't
\ call "exit", you have to undo the changes to the return stack.
\
: unloop ( -- )
    [compile] r>   [compile] r>		\ clean up
    [compile] drop [compile] drop
; immediate

\ *** Begin style  Looping
\
\ Here are the Begin loop variations They are more straight forward than counting loops
\ (and more flexible!).  They always begin with:
\
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


\ ** Miscelanious Memory stuff
\
\ Let's formalize how we allocate RCELLs and bytes.
\
: allot ( n -- )  0 do (allot1) drop loop ;
: byte-allot ( n -- ) RCELL /mod + allot  ;

\ Point to internally allocated (scratch) PAD
\
dsa  dslen + rslen + constant pad


\ Scratch variable (be careful, it doesn't nest and may be modified by anyone)
\
variable R1

\ Implement traditional forth "word" using the scratch pad.
\
: word ( delim -- addr)
    R1 !
    0 pad !
    begin
	next-char dup R1 @ = over 0= or if drop pad exit then
	pad c!+
    again ;

\ Compile-time interpreting... Not sure if this will have a lot of practical
\ use, but is easy enough to provide:
\
: [ reset-state ; immediate
: ] is-compiling ;
