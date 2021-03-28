/foo_2 {
  true { 30 return 40 } if
} def

/foo_3 {
  3 { 7 debug } repeat 
} def

foo_2

foo_3
