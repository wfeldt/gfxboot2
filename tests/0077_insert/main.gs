/foo_1 [ 10 20 30 40 ] def

foo_1 0 100 insert pop
foo_1 -1 400 insert pop
foo_1 4 500 insert pop
foo_1 10 900 insert pop

/foo_2 ( "aa" 11 "bb" 22 "cc" 33 ) def

foo_2 "bb" 222 insert pop
foo_2 "dd" 444 insert pop

/foo_3 "12" "" add def

foo_3 0 'A' insert pop
foo_3 3 '5' insert pop
foo_3 -1 '6' insert pop
foo_3 19 'X' insert pop

foo_1 { } forall
foo_2 { } forall
foo_3
