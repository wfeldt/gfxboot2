5 5 cmp
5 6 cmp
6 5 cmp
"abc" "abc" cmp
"abc" "abd" cmp
"abc" "abc1" cmp
"abd" "abc" cmp
"abc1" "abc" cmp
"" "" cmp
"" "a" cmp
"a" "" cmp
/foo "foo" cmp
nil nil cmp
nil 3 cmp
5 "abc" cmp
{ 10 } { 10 } cmp
/foo_1 [ 1 2 3 ] def
/foo_2 foo_1 def
foo_1 foo_2 cmp
[ 6 ] [ 6 ] cmp
