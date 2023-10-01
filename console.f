iram 7 + constant TIB		( faster )
: clear-tib
  0 tibidx !
  0 tibwordlen !
  0 tibwordidx !
  0 TIB ! ;


: quit
  clear-tib
  cr ." OK" cr
  clear-tib
  0
  begin
    drop
    key dup 
    TIB c!+
    dup 13 = over
    10 = or
  until drop
  1 TIB +!
  interpret
  dup 1 = if ." >"  drop quit then
  dup 2 = if ." Huh?" cr drop quit then
  dup 6 = if ." Abort!" cr drop quit then
  drop
  quit ;

