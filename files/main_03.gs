/image "katze_800.jpg" readfile unpackimage def
/istate newgstate def
getgstate getcanvas setcanvas istate setgstate image setcanvas setgstate

10 10 200 200 setregion
getgstate istate blt
0x00ff0000 setcolor

/rlineto {
  getpos
  4 1 roll add
  3 1 roll add exch
  lineto
} def

100 100 setpos 100     0 rlineto

100 100 setpos 100    30 rlineto
100 100 setpos 100    50 rlineto
100 100 setpos 100    80 rlineto

100 100 setpos 100   100 rlineto

100 100 setpos  80   100 rlineto
100 100 setpos  50   100 rlineto
100 100 setpos  30   100 rlineto

100 100 setpos   0   100 rlineto

100 100 setpos -30   100 rlineto
100 100 setpos -50   100 rlineto
100 100 setpos -80   100 rlineto

100 100 setpos -100  100 rlineto

100 100 setpos -100   80 rlineto
100 100 setpos -100   50 rlineto
100 100 setpos -100   30 rlineto

100 100 setpos -100    0 rlineto

100 100 setpos -100  -30 rlineto
100 100 setpos -100  -50 rlineto
100 100 setpos -100  -80 rlineto

100 100 setpos -100 -100 rlineto

100 100 setpos  -80 -100 rlineto
100 100 setpos  -50 -100 rlineto
100 100 setpos  -30 -100 rlineto

100 100 setpos    0 -100 rlineto

100 100 setpos   30 -100 rlineto
100 100 setpos   50 -100 rlineto
100 100 setpos   80 -100 rlineto

100 100 setpos  100 -100 rlineto

100 100 setpos  100  -80 rlineto
100 100 setpos  100  -50 rlineto
100 100 setpos  100  -30 rlineto
