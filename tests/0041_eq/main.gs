5 5 eq
5 6 eq
6 5 eq
"abc" "abc" eq
"abc" "abd" eq
"abc" "abc1" eq
"abd" "abc" eq
"abc1" "abc" eq
"" "" eq
"" "a" eq
"a" "" eq
/foo "foo" eq
nil nil eq
nil 3 eq
5 "abc" eq
{ 10 } { 10 } eq
/foo_1 [ 1 2 3 ] def
/foo_2 foo_1 def
foo_1 foo_2 eq
[ 6 ] [ 6 ] eq
