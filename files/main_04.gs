/cfont getconsolegstate getfont def
/foo "foo.fnt" readfile newfont def
/bar "bar.fnt" readfile newfont def

/text "ABC 12345 xyz # * % & § öäüß €" def

/image newgstate def
image "katze_800.jpg" readfile unpackimage setcanvas

0 0 setpos image getgstate exch blt
0x90000000 setcolor
image dim fillrect

0x00ffffff setcolor

20 20 setpos
getgstate cfont setfont
text show

20 60 setpos
getgstate bar setfont
text show

20 100 setpos
getgstate foo setfont
text show

