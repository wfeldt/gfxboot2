/foo_1 { 10 20 } def
/foo_2 /foo_1 def
/foo_3 50 def
/foo_4 ( "foo_5" /foo_1 ) def
/foo_1 exec

{ 300 400 } exec

/foo_2 exec
foo_2 exec
/foo_3 exec

"abc" exec
30 exec
nil exec
foo_4 "foo_5" get exec
