getcanvas getconsole setcanvas getfont exch setcanvas setfont

/Widget1 (
  /draw1 {
    10 y setpos
    text show
  }

  /f { self .class }
) nil class def

/Widget2 (
  /y 0
  /text ""
  /draw2 {
    10 y setpos
    text show
    /y y 20 add def
    /text text " XX" add def
  }

  /init { /text "ZZ" def }
) Widget1 class def

/win Widget2 ( /y 20 ) new def

win .draw2
win .draw2
win .draw2

win .draw1

# should be "Widget2"
win .f
