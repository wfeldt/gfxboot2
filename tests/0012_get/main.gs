/foo_1 [ 10 20 30 40 ] def

foo_1 0 get
foo_1 3 get
foo_1 4 get
foo_1 -5 get
foo_1 -1 get
foo_1 -4 get

/foo_2 ( "aa" 11 "bb" 22 "cc" 33 ) def

foo_2 "bb" get
foo_2 "dd" get

/foo_3 "ABCDE12345" def

foo_3 0 get
foo_3 9 get
foo_3 10 get
foo_3 -11 get
foo_3 -1 get
foo_3 -10 get
