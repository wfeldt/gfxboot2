getcanvas getconsole setcanvas getfont exch setcanvas setfont

/widget_class (
  /draw1 {
    10 y setpos
    text show
  }
) def

/win_class (
  /y 20
  /text "XX"
  /draw2 {
    10 y setpos
    text show
    /y y 20 add def
    /text text " XX" add def
  }
) def

win_class widget_class setparent

win_class freeze pop

/win ( ) sticky def
win win_class setparent

win .draw2
win .draw2
win .draw2

win .draw1

win { } forall
