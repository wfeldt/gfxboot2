getcanvas getconsole setcanvas getfont exch setcanvas setfont

/widget (
  /draw1 {
    10 y setpos
    text show
  }
) def

/win (
  /y 20
  /text "XX"
  /draw2 {
    debug
    10 y setpos
    text show
    /y y 20 add def
    /text text " XX" add def
  }
) def

win widget setparent

/win_i ( ) def
win_i win setparent

win_i .draw2
win_i .draw2
win_i .draw2

win_i .draw1
