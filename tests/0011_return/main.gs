/foo_1 {
  4 5 return 6
} def

/foo_2 {
  true { 30 40 return 50 } if
} def

/foo_3 {
  { true { 100 200 return 300 } if } loop
} def

/foo_4 {
  2 { true { 1000 foo_1 2000 return 3000 } if } repeat
} def

/foo_5 {
  [ 1 2 3 ] { true { 10000 20000 foo_1 30000 foo_4 40000 return } if } forall
  50000
} def

foo_1
foo_2
foo_3
foo_4
foo_5

7 8 return 9
