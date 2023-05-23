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
/kHome  0x800047 def
/kEnd   0x80004f def
/kDel   0x800053 def
/kBack  0x08 def

/edit_class (
  /x 0
  /y 0
  /width 0
  /height 0
  /x_0 4 def
  /background nil
  /x_shift 0
  /tail_d 0
  /cursor_index 0
  /cursor_x [ x_0 ]
  /cursor_height 0
  /cursor_state false
  /cursor_back nil
  /buf [ ]
  /orig_region nil

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

  /switch_region {
    /orig_region [ getregion ] def
    x y width height setregion
  }

  /restore_region {
    orig_region { } forall setregion
  }

  /printable {
    key ' ' lt { false return } if
    key 0x1fffff gt { false return } if
    true
  }

  /add_key_to_buffer {
    switch_region

    cursor_index buf length ge {
      # append
      buf cursor_index key insert
      cursor_x cursor_index get 0 setpos
      key show
      cursor_index 1 add!
      cursor_x cursor_index getpos pop put
    } {
      # insert
      buf cursor_index key insert
      cursor_x cursor_index get 0 setpos
      getpos pop
      key show
      getpos pop sub
      /d exch ldef
      cursor_x cursor_index getpos pop d add insert
      cursor_index 1 add!
      cursor_index 1 buf length {
        cursor_x exch over over get d sub put
      } for
      cursor_index 1 sub _redraw
    } ifelse

    restore_region
  }

  /del_key {
    cursor_index buf length eq { return } if

    switch_region

    buf cursor_index delete

    /tail_d cursor_x cursor_index get cursor_x cursor_index 1 add get sub def

    cursor_x cursor_index delete

    cursor_index 1 cursor_x length 1 sub {
      cursor_x exch over over get tail_d add put
    } for

    cursor_index 1 sub _redraw
    /tail_d 0 def

    restore_region
  }

  # region already set
  /_redraw {
    dup 0 gt { 1 sub } {
      # in case it's less than 0, ensure it's 0
      pop 0
      background 0 0 x_0 height csetregion
      0 0 setpos
      getcanvas background blt
    } ifelse
    buf length 0 gt {
      1 buf length 1 sub {
        /i exch ldef
        background cursor_x i get 0 buf i get dim pop height csetregion
        cursor_x i get 0 setpos
        getcanvas background blt
        buf i get show
      } for
    } { pop } ifelse

    background cursor_x -1 get 0 tail_d neg height csetregion
    cursor_x -1 get 0 setpos
    getcanvas background blt
  }

  /cursor_on {
    cursor_state { return } { /cursor_state true def } ifelse

    switch_region

    cursor_x cursor_index get 0 setpos
    cursor_back getcanvas blt

    cursor_x cursor_index get dup 1 setpos cursor_height 1 sub drawline

    restore_region
  }

  /cursor_off {
    cursor_state { /cursor_state false def } { return } ifelse

    switch_region

    cursor_x cursor_index get 0 setpos
    getcanvas cursor_back blt

    restore_region
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
      cursor_index 0 ne { cursor_index -1 add! } if
      cursor_on
      return
    } if

    key kRight eq {
      cursor_index buf length lt { cursor_index 1 add! } if
      cursor_on
      return
    } if

    key kHome eq {
      /cursor_index 0 def
      cursor_on
      return
    } if

    key kEnd eq {
      /cursor_index buf length def
      cursor_on
      return
    } if

    key kDel eq {
      del_key
      cursor_on
      return
    } if

    key kBack eq {
      cursor_index 0 ne { cursor_index -1 add! del_key } if
      cursor_on
      return
    } if

    printable { add_key_to_buffer } if

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
