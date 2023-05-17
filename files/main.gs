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

/csetpos {
  rot getcanvas 4 1 roll setcanvas setpos setcanvas
} def

/csetregion {
  5 -1 roll getcanvas 6 1 roll setcanvas setregion setcanvas
} def

# - - - - - - - - - - - - - - -

/kEnter 0x0d def
/kLeft  0x80004b def
/kRight 0x80004d def

/edit_class (
  /x 0
  /y 0
  /width 0
  /height 0
  /background nil
  /x_shift 0
  /cursor_index 0
  /cursor_x [ 0 ]
  /cursor_height 0
  /cursor_state false
  /cursor_back nil
  /buf [ ]

  /init {
    /height exch def
    /width exch def
    getpos
    /y exch def
    /x exch def
    /background width height newcanvas def

    /cursor_height getfont dim exch pop def

    getcolor
      0x90000000 setcolor
      width height fillrect
    setcolor

    background getcanvas blt

    getcanvas background blt

    /cursor_back 1 height newcanvas def

    cursor_on
  }

  /text {
    buf encodeutf8
  }

  /add_key_to_buffer {
    cursor_index buf length ge {
      # append
      buf cursor_index key insert
      cursor_x -1 get x add y setpos
      key show
      cursor_x [ getpos pop x sub ] add!
      cursor_index 1 add!
    } {
      # insert
      buf cursor_index key insert
    } ifelse
  }

  /cursor_on {
    cursor_state { return } { /cursor_state true def } ifelse

    cursor_x cursor_index get x add y setpos
    cursor_back getcanvas blt

    cursor_x cursor_index get x add dup y 1 add setpos y cursor_height 1 sub add drawline
  }

  /cursor_off {
    cursor_state { /cursor_state false def } { return } ifelse

    cursor_x cursor_index get x add y setpos
    getcanvas cursor_back blt
  }

  /input {
    /key exch def

    cursor_off

    key kEnter eq {
      text
      debug
      return
    } if

    key kLeft eq {
      cursor_index -1 add!
      cursor_on
      return
    } if

    key kRight eq {
      cursor_index 1 add!
      cursor_on
      return
    } if

    add_key_to_buffer

    cursor_on
  }
) freeze def

# - - - - - - - - - - - - - - -

/edit ( ) sticky def
edit edit_class setparent

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

100 470 setpos
600 30 edit .init

/eventhandler seteventhandler

0 0 setpos
