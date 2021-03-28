/foo_1 [ 10 20 30 40 ] def

foo_1 0 100 put
foo_1 -1 400 put
foo_1 4 500 put
foo_1 8 900 put

/foo_2 ( "aa" 11 "bb" 22 "cc" 33 ) def

foo_2 "bb" 222 put
foo_2 "dd" 444 put

/foo_3 5 string def

foo_3 0 'A' put
foo_3 3 '5' put
foo_3 -1 '6' put

foo_1 { } forall
foo_2 { } forall
foo_3 { } forall
