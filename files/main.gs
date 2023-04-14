getcanvas getconsole setcanvas 0x40405070 setbgcolor setcanvas

/cfont getconsole getfont def
/foo "foo.fnt" readfile newfont def
/bar "bar.fnt" readfile newfont def

/text "ABC 12345 xyz # * % & § öäüß €" def

/image "katze_800.jpg" readfile unpackimage def
/x1 "x1.jpg" readfile unpackimage def
x1 400 50 setlocation

/x2 "x2.jpg" readfile unpackimage def
x2 -80 -80 setlocation

[ getcanvas x1 x2 ] setcompose

0 0 setpos getcanvas image blt
0x90000000 setcolor
image dim fillrect

0xffff00 setcolor

getcanvas cfont setfont
50 50 setpos "Some font samples" show

0x00ffffff setcolor

getcanvas cfont setfont
50 100 setpos text show

getcanvas bar setfont
50 130 setpos text show

getcanvas foo setfont
50 180 setpos text show
