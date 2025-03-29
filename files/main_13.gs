# clipping test

/console-font getcanvas getconsole setcanvas getfont exch setcanvas def

/dejavu-sans-24 "dejavu-sans-24.fnt" readfile newfont def
/hack-14 "hack-14.fnt" readfile newfont def

/foo-font "foo.fnt" readfile newfont def
/bar-font "bar.fnt" readfile newfont def

/text "ABC &%$ *+~ ÄÖß||\ 12345 xyA_z_AjzjxjJ # * % & § öäüß €" def

/test {
  setfont /y exch def /x exch def
  0xff000000 setbgcolor
  0x00ffffff setcolor x y setpos text show
  0x00206090 setcolor x 2 add y 30 add setpos text show
  0x90ffff20 setcolor x 4 add y 60 add setpos text show
  0x903070a0 setbgcolor
  0x0000ffff setcolor x 6 add y 90 add setpos text show
  0x00a06090 setcolor x 8 add y 120 add setpos text show
  0x702fff90 setcolor x 10 add y 150 add setpos text show
} def

0 1 799 {
  /x exch def
  0x000000 x 0x7f and 0x101 mul add setcolor
  x 0 setpos x 599 drawline
} for

10 10 dejavu-sans-24 test
10 200 console-font test
10 400 hack-14 test
#-10 400 foo-font test
#400 400 bar-font test

[ 'a' 'ä' ' ' 8364 '1' ] encodeutf8
"uü €2"

/setrpos {
  getpos rot add exch rot add exch setpos
} def

/csetpos {
  rot getcanvas 4 1 roll setcanvas setpos setcanvas
} def

/csetregion {
  5 -1 roll getcanvas 6 1 roll setcanvas setregion setcanvas
} def

/cv 300 100 newcanvas def

getcanvas cv setcanvas

0x800040 setcolor
300 100 fillrect

0x0050ff setcolor

0 0 setpos
299 0 drawline
299 99 drawline
0 99 drawline
0 0 drawline

0xffffff setcolor

-35 90 200 20 setregion

hack-14 setfont 0 0 setpos "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890" show

0 0 setpos 1200 0 drawline

100 100 setlocation

getcompose [ cv ] add setcompose


setcanvas 0 0 800 600 updatescreen

0xffff00 setcolor

99 99 setpos
400 99 drawline
400 200 drawline
99 200 drawline
99 99 drawline

