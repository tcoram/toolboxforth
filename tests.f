mark TESTS

variable (bench)
: bench{ ms (bench) ! ;
: }bench ms (bench) @ - . ." ms elapsed" ;


: test-do  10000000 0 do i drop loop ;
: test-for  10000000 for r@ drop next ;
: test-begin-until 10000000  R1 ! begin -1 R1 +! R1 @ 0= until ;
: test-begin-again 10000000  R1 ! begin -1 R1 +! R1 @ 0= if exit then again ;
: test-begin-until-stack 10000000 begin 1- dup 0= until drop ;
: tail-call 1- dup 0> if tail-call then drop ;
: test-tail-call 10000000 tail-call ;

: loop-tests
    ." testing 10 million iteration loops" cr
    ." testing for .. next: "
    ['] test-for time-it 
    ." testing tail call: "
    ['] test-tail-call time-it
    ." testing do .. loop: "
    ['] test-do time-it 
    ." testing begin .. until: "
    ['] test-begin-until time-it 
    ." testing begin .. again: "
    ['] test-begin-again time-it  
    ." testing begin .. until-stack: "
    ['] test-begin-until-stack time-it  ;

