# a sample 'class' implementation
#
# ( word_1 hash_1 hash_2 -- )
#
# Define hash_2 as word_1 and make hash_1 its parent. word_1 is created in
# the global context.
#
/klass {
  2 index exch gdef
  exch exec exch setparent
} def


getcanvas getconsole setcanvas getfont exch setcanvas setfont

/widget (
  /draw1 {
    10 y setpos
    text show
  }
) def

/win widget (
  /y 20
  /text "XX"
  /draw2 {
    10 y setpos
    text show
    /y y 20 add def
    /text text " XX" add def
  }
) klass

win .draw2
win .draw2
win .draw2

win .draw1
