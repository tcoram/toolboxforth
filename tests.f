mark TESTS
: test-do  1000000 0 do i drop loop ;
: test-for  1000000 for r@ drop next ;
: test-begin-until 1000000  _cc ! begin -1 _cc +! _cc @ 0= until ;
: test-begin-again 1000000  _cc ! begin -1 _cc +! _cc @ 0= if exit then again ;
: test-begin-until-stack 1000000 begin 1- dup 0= until ;

: loop-tests
    1 to DEBUG
    debug" testing do .. loop: "
    ['] test-do time-it 
    debug" testing for .. next: "
    ['] test-for time-it 
    debug" testing begin .. until: "
    ['] test-begin-until time-it 
    debug" testing begin .. again: "
    ['] test-begin-again time-it  
    debug" testing begin .. until-stack: "
    ['] test-begin-until-stack time-it
    0 to DEBUG ;

