/font1 getconsolegstate getfont def
/font2 "font2.psfu" readfile newfont def
font2 font1 setparent
getgstate font1 setfont

-50 -20 setpos

getgstate 20 20 155 100 setregion

"XXX adasdas X\nXX€\n" show
"--\x00\x00--\n" show
"Some devices are designed to deny users access to install or run
modified versions of the software inside them, although the manufacturer
can do so.  This is fundamentally incompatible with the aim of
protecting users' freedom to change the software." show

/istate2 newgstate def
istate2 "katze_800.jpg" readfile unpackimage setcanvas
istate2 200 100 300 200 setregion

getgstate 0 0 1000 1000 setregion

50 50 setpos getgstate istate2 blt

getgstate 10 10 200 200 setregion

0xc0ff0000 setcolor

60 1 80 {
  /y exch def
  50 1 100 {
    y setpos putpixel getpixel pop
  } for
} for