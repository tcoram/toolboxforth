forget-to-mark TESTS
mark TESTS
: test-do  1000000 0 do i drop loop ;
: test-for  1000000 for r@ drop next ;
: test-begin-until 1000000  A ! begin -1 A +! A @ 0= until ;
: test-begin-again 1000000  A ! begin -1 A +! A @ 0= if exit then again ;
: test-begin-until-stack 1000000 begin 1- dup 0= until ;


: loop-tests
  ." testing do .. loop: "
  ['] test-do time-it 
  ." testing for .. next: "
  ['] test-for time-it 
  ." testing begin .. until: "
  ['] test-begin-until time-it 
  ." testing begin .. again: "
  ['] test-begin-again time-it  
  ." testing begin .. until-stack: "
  ['] test-begin-until-stack time-it  ;

