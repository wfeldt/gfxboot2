/cfont getconsole getfont def
/bar "bar.fnt" readfile newfont def

/text "ABC 12345 xyz # * % & § öäüß €" def

/kater "kater_0800.jpg" readfile unpackimage def

0 0 setpos
getcanvas kater blt

0x90000000 setcolor

getcanvas cfont setfont
0xffffff setcolor 0xff000000 setbgcolor
50 300 setpos text show

0xffffff setcolor 0 setbgcolor
50 320 setpos text show

getcanvas bar setfont
0xffffff setcolor 0xff000000 setbgcolor
50 500 setpos text show
0xffffff setcolor 0 setbgcolor
50 520 setpos text show

/a ( /x 100 /y { /x 10 ldef debug } ) def

a .x
a .y
