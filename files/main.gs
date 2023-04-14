getcanvas getconsole setcanvas 0x40405070 setbgcolor setcanvas

/cfont getconsole getfont def
/foo "foo.fnt" readfile newfont def
/bar "bar.fnt" readfile newfont def

/text "ABC 12345 xyz # * % & § öäüß €" def

/kater "kater_0800.jpg" readfile unpackimage def

/katze_orig "katze_0400.jpg" readfile unpackimage def
/katze katze_orig dim newcanvas def

getcanvas
  katze setcanvas
  0x80000000 setcolor
  1 setdrawmode
  getcanvas dim fillrect
  0 setdrawmode
setcanvas

katze katze_orig blt

katze 500 50 setlocation

/pilz "pilz_0400.jpg" readfile unpackimage def
pilz -180 -180 setlocation

[ getcanvas katze pilz ] setcompose

0 0 setpos getcanvas kater blt
0x90000000 setcolor
getcanvas dim fillrect

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
