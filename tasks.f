\ Nothing formal yet.. just a playground to play with a tasking/coroutine concept.
\

\ Always Task 0 :  the console and main interpreter.
\
uram constant tsk0

\ We always "yield" control back to Task 0, which is resposible for handling
\ dispatching..
\
: (yield) uram! ;
: yield tsk0 uram! ;

\ make a task.. rstack size datastack size number of cell and a variable for task ref
\ Are loaded into RAM by allocating the proper amount *right after* the variable
\ is defined...
\
: make-task ( rssz dssz varcells addr - )
    >r >r 2dup r> + +  ( rssz dssz total - )
    dup allot
    r@ ! ( r d ) 
    10 r@ 1+ !  
    -1 r@ 2 + !  
    2dup + r@ 3 + ! 
    r@ 4 + ! 
    r@ 5 + ! r> drop  ;

: end-task ( task -)
    yield
    >r
    -1 2 r@ + !			\ reset data stack
    4 r@ + @ 5 r@ +  @ + 3 r> + ! ;	\ reset return stack


\ For start-task,  uram! *must* be called directly. This just compiles it into your task
\ The black magic for tasking relies on the return stack...
\
: start-task ( tsk - )
    [compile] dup [compile] end-task
    [compile] uram! ; immediate



\ Take a look at was made
: .task ( addr - )
    dup @          ." Total RAM = " . cr
    dup 1+ @       ." Base      = " . cr
    dup 2 + @      ." Stack idx = " . cr
    dup 3 + @      ." Rstck idx = " . cr
    dup 4 + @      ." Stack size= " . cr
    dup 5 + @      ." Rstck size= " . cr drop ;

\ -------------------------------------------------------------------
\ Examples
\

\ You *must* make the task right after declaring its variable...
\
variable tsk1  50 50 10 tsk1 make-task
tsk1 .task


variable tsk2 50 50 10 tsk2 make-task 
tsk2 .task


variable tsk3 10 10 2 tsk3 make-task 
tsk3 .task


: sub-a 15 1 do i . cr yield  loop  ;

: co-a  start-task yield sub-a ;
: co-b  start-task 15 1 do i 64 + emit cr yield loop ;

\ Assign words to each task and loop executing each task in a round robin fashion.
\ Note, yes, this will only implement 5 cycles (not 15).
: doab
    tsk1 co-a
    tsk2 co-b
    5 0 do  tsk1 (yield) tsk2 (yield) loop ;

\ Time based tasking...
\
: (wait) ( ms - ) 0 0 rot poll drop ;
: wait ( ms - )  0 do 1 (wait) yield loop ;
    
: co-c  start-task yield 10 1 do i . cr 500 wait loop  ;
: co-d  start-task yield 10 1 do i 64 + emit cr 500 wait loop ;
: co-e  start-task yield begin [char] . emit 100 wait again  ;

: docde
    tsk1 co-c
    tsk2 co-d
    tsk3 co-e
    begin  tsk1 (yield) tsk2 (yield) tsk3 (yield) again ;


: test-do-no-task  10000000 0 do i drop loop ;
: test-do  10000000 0 do i drop yield loop ;
: test-for  10000000 for r@ drop yield next ;
: test-begin-until 10000000  A ! begin -1 A +! A @ yield 0= until ;
: test-begin-again 10000000  A ! begin -1 A +! A @ 0= if exit then yield again ;
: test-begin-until-stack 10000000 begin 1- dup yield 0= until drop ;
: tail-call 1- dup 0> if yield tail-call then drop ;
: test-tail-call 10000000 tail-call ;

: time-test
   ['] test-do-no-task time-it
   ['] test-do time-it
    tsk1 start-task 
    ['] test-do time-it ;
: time-test-task
    time-test begin tsk1 (yield) again ;

: loop-tests1
    start-task
    yield
    ." testing for .. next: "
    ['] test-for time-it 
    ." testing tail call: "
    ['] test-tail-call time-it
    ." testing do .. loop: "
    ['] test-do time-it ;

: loop-tests2
    start-task
    yield
    ." testing begin .. until: "
    ['] test-begin-until time-it 
    ." testing begin .. again: "
    ['] test-begin-again time-it  
    ." testing begin .. until-stack: "
    ['] test-begin-until-stack time-it ;

: loop-tests-tasks
    ." testing 10 million iteration loops" cr
    tsk1 loop-tests1
    tsk2 loop-tests2
    begin  tsk1 (yield) tsk2 (yield) again ;


