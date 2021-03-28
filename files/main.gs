/cfont getconsolegstate getfont def
/foo "foo.fnt" readfile newfont def
/bar "bar.fnt" readfile newfont def

/text "ABC 12345 xyz # * % & § öäüß €" def

/image gstate def
image "katze_800.jpg" readfile unpackimage setcanvas

0 0 setpos
image getgstate exch blt
0x90000000 setcolor
image dim fillrect

0xffff00 setcolor

getgstate cfont setfont
50 50 setpos "Some font samples" show

0x00ffffff setcolor

getgstate cfont setfont
50 100 setpos text show

getgstate bar setfont
50 130 setpos text show

getgstate foo setfont
50 180 setpos text show
