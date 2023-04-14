getcanvas 50 20 400 400 setregion
0x00ffffff setcolor

# draw line relative to cursor position
/r {
  getpos
  4 1 roll add
  3 1 roll add exch
  drawline
} def

200 200 setpos  200    0 r

200 200 setpos  200   60 r
200 200 setpos  200  100 r
200 200 setpos  200  160 r

200 200 setpos  200  200 r

200 200 setpos  160  200 r
200 200 setpos  100  200 r
200 200 setpos   60  200 r

200 200 setpos    0  200 r

200 200 setpos  -60  200 r
200 200 setpos -100  200 r
200 200 setpos -160  200 r

200 200 setpos -200  200 r

200 200 setpos -200  160 r
200 200 setpos -200  100 r
200 200 setpos -200   60 r

200 200 setpos -200    0 r

200 200 setpos -200  -60 r
200 200 setpos -200 -100 r
200 200 setpos -200 -160 r

200 200 setpos -200 -200 r

200 200 setpos -160 -200 r
200 200 setpos -100 -200 r
200 200 setpos  -60 -200 r

200 200 setpos    0 -200 r

200 200 setpos   60 -200 r
200 200 setpos  100 -200 r
200 200 setpos  160 -200 r

200 200 setpos  200 -200 r

200 200 setpos  200 -160 r
200 200 setpos  200 -100 r
200 200 setpos  200  -60 r
