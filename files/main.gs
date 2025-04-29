## include system.gs
## files dejavu-sans-24.fnt katze_*.jpg katze_*.png sample_2.png

/System (
  /key 0
  /action 0

  # ( int_1 -- int_2 )
  # int_1: key
  # int_2: action
  #
  /keyevent {
    /key exch def

    key edit .input

    action
  }
) nil class def


# - - - - - - - - - - - - - - -

/console-font getcanvas getconsole setcanvas getfont exch setcanvas def

/dejavu-sans-24 "dejavu-sans-24.fnt" readfile newfont def

/title "ABC 12345ijklmn xyz # * % & § öäüß €" def

/katze [ getcanvas dim pop ] "katze_%04u.png" format readfile
 dup nil eq { console-font setfont 0 20 setpos 0xff0000 setcolor "Error: no backgound image" show return } if
unpackimage def

0 0 setpos
getcanvas katze blt

0xf0f000 setcolor
103 450 setpos 103 600 drawline
696 450 setpos 696 600 drawline
200 450 setpos 200 600 drawline
300 450 setpos 300 600 drawline
400 450 setpos 400 600 drawline
500 450 setpos 500 600 drawline

dejavu-sans-24 setfont

getcanvas dim pop title dim pop sub 2 div 50 setpos

0x90000000 setcolor
-10 -5 setrpos
title dim 20 10 add2 fillrect
10 5 setrpos

0xffffff setcolor
title show

/edit Edit ( /x 100 /y 550 /width 600 /height 30 ) new def

System ( ) new setsystem

/sample_2 "sample_2.png" readfile unpackimage def

0 0 setpos getcanvas sample_2 blt

getcompose [ sample_2 ] add setcompose

getcanvas sample_2 setcanvas 200 600 256 sub setlocation setcanvas

0 0 getcanvas dim updatescreen

0 0 setpos
