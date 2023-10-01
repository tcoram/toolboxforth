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
  begin
    key dup 
    TIB c!+
    dup 13 = over
    10 = or
  until drop
  1 TIB +!
  interpret
  quit ;

