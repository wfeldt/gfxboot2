/eventhandler_vars ( /type 0 /key 0 /action 0 ) def

/eventhandler {
  eventhandler_vars setdict

  /type exch def
  /key exch def

  key edit .input

  action
} def

/setrpos {
  getpos rot add exch rot add exch setpos
} def

# - - - - - - - - - - - - - - -

/edit (
  /x 0
  /y 0
  /width 0
  /height 0
  /background nil
  /x_shift 0
  /cursor_index 0
  /cursor_x [ 0 ]
  /buf [ ]

  /init {
    /height exch def
    /width exch def
    getpos
    /y exch def
    /x exch def
    /background width height newcanvas def
    getcolor
      0x90000000 setcolor
      width height fillrect
    setcolor
    getregion
      x y width height setregion
      background getcanvas blt
    setregion
    x y setpos
    getcanvas background blt
  }

  /text {
    buf encodeutf8
  }

  /input {
    /key exch def

    key 0x0d eq {
      debug
      text
      return
    } if

    /buf buf [ key ] add def
    cursor_x -1 get x add y setpos
    key show
    /cursor_x cursor_x [ getpos pop x sub ] add def
  }
) def

# - - - - - - - - - - - - - - -

/console-font getcanvas getconsole setcanvas getfont exch setcanvas def

/dejavu-sans-24 "dejavu-sans-24.fnt" readfile newfont def

/title "ABC 12345ijklmn xyz # * % & § öäüß €" def

/katze "katze_%04u.jpg" [ getcanvas dim pop ] format readfile
 dup nil eq { console-font setfont 0 20 setpos 0xff0000 setcolor "Error: no backgound image" show return } if
unpackimage def

0 0 setpos
getcanvas katze blt

dejavu-sans-24 setfont

getcanvas dim pop title dim pop sub 2 div 50 setpos

0x90000000 setcolor
-10 -5 setrpos
title dim 10 add exch 20 add exch fillrect
10 5 setrpos

0xffffff setcolor
title show

100 460 setpos
600 30 edit .init

/eventhandler seteventhandler

0 0 setpos
