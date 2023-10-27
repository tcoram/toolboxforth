\ Nothing formal yet.. just a playground to play with a tasking/coroutine concept.
\

\ Three words define the whole coroutine capability:
\  make-task, start-task and yield.
\

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

\ For start-task,  uram! *must* be called directly. This just compiles it into your task
\ The black magic for tasking relies on the return stack...
\
: start-task ( tsk - )
    [compile] uram! ; immediate

\ Always Task 0 :  the console and main interpreter.
\
uram constant tsk0

\ We always "yield" control back to Task 0, which is resposible for handling
\ dispatching..
\
: yield tsk0 uram! ;

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
    
: co-c  start-task yield 5 1 do i . cr 500 wait loop  ;
: co-d  start-task 5 1 do i 64 + emit cr 500 wait loop ;

: docd
    tsk1 co-c
    tsk2 co-d
    begin  tsk1 (yield) tsk2 (yield) again ;

