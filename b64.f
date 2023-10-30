create b64invs
62 , -1 , -1 , -1 , 63 , 52 , 53 , 54 , 55 , 56 , 57 , 58 ,
59 , 60 , 61 , -1 , -1 , -1 , -1 , -1 , -1 , -1 , 0 , 1 , 2 , 3 , 4 , 5 ,
6 , 7 , 8 , 9 , 10 , 11 , 12 , 13 , 14 , 15 , 16 , 17 , 18 , 19 , 20 ,
21 , 22 , 23 , 24 , 25 , -1 , -1 , -1 , -1 , -1 , -1 , 26 , 27 , 28 ,
29 , 30 , 31 , 32 , 33 , 34 , 35 , 36 , 37 , 38 , 39 , 40 , 41 , 42 ,
43 , 44 , 45 , 46 , 47 , 48 , 49 , 50 , 51 ,


pad value b64pt

defer +b64dc@
: (b64decode) ( dict-caddr len coudrr - )
    dup 0 swap !
    to b64pt
    0 do
	dup i +b64dc@ 43 - b64invs + dict@  6 lshift over ( caddr v caddr )   
	i 1+ +b64dc@ 43 - b64invs + dict@  or ( caddr v) 
	6 lshift  
	over i 2 + +b64dc@  dup [char] = =
	if drop else 43 -  b64invs + dict@ or then ( caddr v ) 
	6 lshift  
	over i 3 + +b64dc@  dup [char] = =
	if drop else 43 - b64invs + dict@  or then ( caddr v ) 
	dup 16 rshift $ff and b64pt  c!+ ( caddr v) 
	over i 2 + +b64dc@ [char] = =  not
	if dup 8 rshift $ff and b64pt c!+ then ( caddr v) 
	over i 3 + +b64dc@ [char] = = not
	if dup $ff and b64pt c!+ then ( caddr v) drop ( caddr) 
    4 +loop 
    drop ;

\ 'is' doesn't work with primitives, so we need to spoof the primitive
\
: _+c@  +c@ ;
: _+dict-c@ +dict-c@ ;

\ Decode from RAM to RAM
\
: b64decode ( caddr len dest-caddr - )
    ['] _+c@ is +b64dc@
    (b64decode) ;

\ Decode from dictionary to RAM
\
: b64decode-dict ( diccaddr len dest-caddr - )
    ['] _+dict-c@ is +b64dc@
    (b64decode) ;

    
variable bitsbuff
variable b64len

: alphabase ( u -- c ) $3F and ," ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" 1+ swap  +dict-c@ ;
: storecode ( u f -- ) if drop 61 else alphabase then  b64pt c!+  ;

: 3bytesin ( addr idx -- addr cnt)
   0 bitsbuff !
   3 0 do
      2dup i +  +b64dc@ bitsbuff @ 8 lshift + bitsbuff !
   loop  
   b64len @  swap - ;

: 4chars! ( n -- ) ( n=# of bytes )
   4 0 do
      dup i - 0 < 
      bitsbuff @ 18 rshift swap storecode
      bitsbuff @ 6 lshift bitsbuff !
   loop drop ;

: (b64encode) ( addr len dest - )
    to b64pt
    0 b64pt !
    dup b64len ! 
    0 do
	i 3bytesin  4chars!
    3 +loop drop ;
    

\ Encode RAM to RAM
\
: b64encode ( c-addr len dest - )
    ['] _+c@ is +b64dc@
    (b64encode) ;

\ Encode Dictionary to RAM
\
: b64encode-dict ( c-addr len dest - )
    ['] _+dict-c@ is +b64dc@
    (b64encode) ;

: test-b64encode
    s" hello world!!"  pad b64encode-dict  pad  count type cr ;
