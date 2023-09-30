\ Simple driver for LIS3DH ESP32
\
forget-to-marker LIS3DH
mark LIS3DH

\ ESP32
20 constant lis-cs
9 constant lis-miso
10 constant lis-mosi
8 constant lis-clk

: lis{  lis-cs 0 1 8000000 spi{ ;
: }lis  lis-cs }spi ;
  
: lis-rd  ( reg - n)
  lis{ $80 or spi! spi@ }lis ;

: lis-rds ( cnt reg - n..)
  lis{ $c0 or spi!  0 do  spi@ loop  }lis ;

: lis-wr ( val reg - )
  lis{ spi! spi! }lis ;

: lis-id ( -- n)  $0f lis-rd ;

: lis-xyz ( -- x x y y z z) 6 $28 lis-rds ;

: /lis
  lis-mosi lis-miso lis-clk spi-cfg
  0 $21 lis-wr 
  0 $23 lis-wr
  $8f $20 lis-wr ;

