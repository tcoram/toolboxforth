: emit ( c -- )   1 cf ;
: key ( -- c )   2 cf ;
: save-image ( -- )   3 cf ;
: include ( <filename> -- )   4 cf ;
: openfile ( <filename> mode -- fd )   5 cf ;
: closefile ( fd -- )   6 cf ;
: inb ( fd -- c )   7 cf ;
: outb ( c fd -- )   8 cf ;
: ms ( -- milliseconds )   9 cf ;
