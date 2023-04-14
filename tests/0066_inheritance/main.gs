getcanvas getconsole getfont setfont

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
    10 y setpos
    text show
    /y y 20 add def
    /text text " XX" add def
  }
) def

win widget setparent

win .draw2
win .draw2
win .draw2

win .draw1
