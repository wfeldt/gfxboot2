/csetpos {
  rot getcanvas 4 1 roll setcanvas setpos setcanvas
} def

/csetregion {
  5 -1 roll getcanvas 6 1 roll setcanvas setregion setcanvas
} def

/foo1 600 400 newcanvas def
foo1 20 10 300 100 csetregion
foo1 100 200 csetpos

/foo2 700 300 newcanvas def
foo2 30 40 300 100 csetregion
foo2 -10 -20 csetpos

foo1 foo2 blt
