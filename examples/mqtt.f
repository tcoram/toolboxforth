\ MQTT client

variable mq-fd
variable tcp-addr 2 allot

: mq-local-open ( port - fd or error)
    c" 127.0.0.1" swap connect-tcp  dup mq-fd ! 0> if tcp-addr ! tcp-addr 1+ ! then mq-fd @ ;

: mq-close ( - )
    mq-fd @ 0> if tcp-addr 1+ @ tcp-addr @ mq-fd @  close-tcp  -1 mq-fd ! then ;

: mq-rb ( - c) mq-fd @ read-byte dup 0< if ." read error" cr abort then  ;
: mq-wb ( c -)  mq-fd @ write-byte  0< if ." write error" abort then ;


: mq-wait-ms ( ms - c | -1)
    mq-fd @ 1 rot poll ;

: mq-write-rmlen ( n - )
    R1 !
    begin
	R1 @ 128 mod R1 @ 128 / dup R1 !
	0> if $80 or then mq-wb
    0 R1 @ >=  until ;

: mq-len ( N - n n)
    dup 255 and swap 8 rshift  ;
	    
: pkt-flags  ( b - flgs type) dup 15 and swap 4 rshift 15 and ;

variable pklen
variable mult
: (mq-read-fixed-hdr)  ( -- len flags type | -1)
    mq-rb dup 0> if
	pkt-flags 
	1 mult !
	0 pklen !
	begin
	    mq-rb mult @ 
	    dup  127 and mult @ * pklen +!
	128 and 0= until
	pklen @ rot
	exit
    then drop -1 ;

0 value mq-buf

: mq-read-pkt ( - flags type )
    0 mq-buf !
    (mq-read-fixed-hdr) dup 0> if 
	mq-buf 1+ A! 
	rot dup mq-buf ! for (c!+) next
    then ;

: vhdr-16 ( N - )
    dup 8 rshift (c!+) $ff and (c!+) ;

: cstr>vhdr ( caddr - len)		  \ length including mq-len (2+)
    count dup mq-len (c!+) (c!+) >r r@ 0 do dup i +c@  (c!+) loop  drop r> 2+ ;

: cstr>vhdr-no-len ( caddr - len)		  \ length including mq-len (2+)
    count >r r@ 0 do dup i +c@  (c!+) loop  drop r> ;

: mq-conn-varpay ( p u id buf - )
    to mq-buf
    mq-buf 1+  A!
    $00 (c!+)  $04 (c!+)  $4d (c!+)
    $51 (c!+)  $54 (c!+)  $54 (c!+)
    $04 (c!+) %11000010 (c!+) 0 (c!+) 0 (c!+)
    10 mq-buf !
    3  for
	cstr>vhdr mq-buf +!
    next ;
    
: mq-connect ( p u id buf - )
    mq-conn-varpay
    $10 mq-wb
    mq-buf @ mq-write-rmlen \ fixed header
    mq-buf 1+ A! mq-buf @  for  (c@+) mq-wb  next ;

: mq-conn-ack ( - t|f)
    (mq-read-fixed-hdr) 0> if 
	drop drop drop
	mq-rb drop \ ack
	mq-rb 0= exit
    then 0 ;

variable _pid
1 _pid !
: new-pub-id _pid @ 1 _pid +! ;

: mq-pub-varpay ( msga topa - )
    cstr>vhdr mq-buf +!
    new-pub-id vhdr-16 2 mq-buf +!
    cstr>vhdr-no-len  mq-buf +! ;

: mq-ping
    $c0 mq-wb 0 mq-wb ;

: mq-pub ( msga topa qos ret - )
    swap 1 lshift or $30 or mq-wb
    0 mq-buf !
    mq-buf 1+  A!
    mq-pub-varpay  
    mq-buf @  mq-write-rmlen 
    mq-buf 1+ A! mq-buf @  for  (c@+) mq-wb next ;

: mq-sub ( topa cnt qos - )
    R1 !
    $82 mq-wb 
    0 mq-buf !
    mq-buf 1+  A!
    for cstr>vhdr 1+ mq-buf +! R1 @  (c!+) next
    mq-buf @  mq-write-rmlen 
    mq-buf 1+ A! mq-buf @  for  (c@+) mq-wb next ;

: mq-parse-pub ( -- topicidx topiclen payloadidx payloadlen)
    mq-buf 1+ A!
    mq-buf 1+ 2 + ( topicidx)
    (c@+) 8 rshift (c@+) or ( topicidx topiclen)
    dup A+! 2dup + ( topicidx topiclen payloadidx )
    mq-buf @ over - ;



\ --- test stuff
variable tbuf 500 byte-allot
create tuser ," anon"
create tpass ," nopass"
create tid ," foo"

: test-connect
    1883 mq-local-open 0> if
	." Connecting!" cr
	tpass tuser tid tbuf mq-connect
	mq-conn-ack if ." Yay!" else ." Boo" then cr
	mq-ping 
	mq-read-pkt  0> if . cr then 
	mq-ping 
	mq-read-pkt  0> if . cr then 
	mq-ping 
	mq-read-pkt  0> if . cr then 
	," Test Message!" ," test/a"  1 0 mq-pub
	mq-read-pkt  0> if . cr then 
	mq-close
    then ;
