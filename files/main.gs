## files dejavu-sans-24.fnt katze_*.jpg katze_*.png sample_2.png

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
/kCtrlL 0x0c def
/kCtrlR 0x12 def

/Edit (
  /x 0
  /y 0
  /width 0
  /height 0
  /margin 4 def
  /background nil
  /x_shift 0			# 0 or positive
  /tail_d 0			# 0 or negative
  /cursor_index 0
  /cursor_x [ 0 ]
  /cursor_height 0
  /cursor_state false
  /cursor_back nil
  /buf [ ]
  /orig_region nil

  /init {
    x y setpos

    /background width height newcanvas def

    /cursor_height getfont dim exch pop def

    getcolor
      0x90000000 setcolor
      width height fillrect
    setcolor

    background getcanvas blt

    /cursor_back 1 height newcanvas def

    cursor_on
  }

  /text {
    buf encodeutf8
  }

  # set drawing region to edit object
  /switch_region {
    /orig_region [ getregion ] def
    x margin add y width margin dup add sub height setregion
  }

  # restore original drawing region
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
      buf cursor_index key insert pop
      cursor_x cursor_index get x_shift sub 0 setpos
      key show
      /cursor_index 1 add!
      cursor_x cursor_index getpos pop x_shift add put
    } {
      # insert
      buf cursor_index key insert pop
      cursor_x cursor_index get x_shift sub 0 setpos
      getpos pop
      key show
      getpos pop sub
      /d exch ldef
      cursor_x cursor_index getpos pop x_shift add d add insert pop
      /cursor_index 1 add!
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

    cursor_index 1 sub _align { pop 0 } if _redraw

    /tail_d 0 def

    restore_region
  }

  # ( start_index -- )
  # region already set
  /_redraw {
    # start 1 pos left, but at least 0
    1 sub 0 max

    buf length 0 gt {
      1 buf length 1 sub {
        /i exch ldef
        background cursor_x i get x_shift sub margin add 0 buf i get dim pop height csetregion
        cursor_x i get x_shift sub 0 setpos
        getcanvas background blt
        buf i get show
      } for
    } { pop } ifelse

    # copy (-tail_d) + 1 column background after string end
    background cursor_x -1 get x_shift sub margin add 0 1 tail_d sub height csetregion
    cursor_x -1 get x_shift sub 0 setpos
    getcanvas background blt
  }

  # ( -- true|false )
  /_align {
    /new_shift x_shift ldef
    /_width width margin dup add 1 add sub ldef
    /_length cursor_x buf length get ldef

    /cursor_pos cursor_x cursor_index get ldef

    cursor_pos x_shift lt {
      /new_shift cursor_pos def
    } {
      cursor_pos x_shift sub _width gt {
        /new_shift cursor_pos _width sub def
      } if
    } ifelse

    new_shift 0 gt {
      /_delta _width _length new_shift sub sub ldef
      _delta 0 gt {
        /new_shift new_shift _delta new_shift min sub def
      } if
    } if

    new_shift x_shift ne

    /x_shift new_shift def
  }

  /align+redraw {
    _align { switch_region 0 _redraw restore_region } if
  }

  /cursor_on {
    cursor_state { return } { /cursor_state true def } ifelse

    switch_region

    cursor_x cursor_index get x_shift sub 0 setpos
    cursor_back getcanvas blt

    cursor_x cursor_index get x_shift sub dup 1 setpos cursor_height 1 sub drawline

    restore_region
  }

  /cursor_off {
    cursor_state { /cursor_state false def } { return } ifelse

    switch_region

    cursor_x cursor_index get x_shift sub 0 setpos
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
      cursor_index 0 ne { /cursor_index -1 add! } if
      align+redraw
      cursor_on
      return
    } if

    key kRight eq {
      cursor_index buf length lt { /cursor_index 1 add! } if
      align+redraw
      cursor_on
      return
    } if

    key kCtrlL eq {
      /x_shift -5 add!
      /tail_d width neg def
      switch_region 0 _redraw restore_region
      cursor_on
      return
    } if

    key kCtrlR eq {
      /x_shift 5 add!
      /tail_d width neg def
      switch_region 0 _redraw restore_region
      cursor_on
      return
    } if

    key kHome eq {
      /cursor_index 0 def
      align+redraw
      cursor_on
      return
    } if

    key kEnd eq {
      /cursor_index buf length def
      align+redraw
      cursor_on
      return
    } if

    key kDel eq {
      del_key
      align+redraw
      cursor_on
      return
    } if

    key kBack eq {
      cursor_index 0 ne { /cursor_index -1 add! del_key } if
      align+redraw
      cursor_on
      return
    } if

    printable { add_key_to_buffer align+redraw } if

    cursor_on
  }
) nil class def

# - - - - - - - - - - - - - - -

# new class syntax:
#   /Edit ( ... ) nil class def
#   /edit Edit ( /x 100 /y 550 /width 600 /height 30 ) new def

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
title dim 10 add exch 20 add exch fillrect
10 5 setrpos

0xffffff setcolor
title show

/edit Edit ( /x 100 /y 550 /width 600 /height 30 ) new def

/eventhandler seteventhandler

/sample_2 "sample_2.png" readfile unpackimage def

0 0 setpos getcanvas sample_2 blt

getcompose [ sample_2 ] add setcompose

getcanvas sample_2 setcanvas 200 600 256 sub setlocation setcanvas

0 0 getcanvas dim updatescreen

0 0 setpos
