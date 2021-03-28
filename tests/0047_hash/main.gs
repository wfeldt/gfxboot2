/foo ( "aa" 11 "cc" 33 "bb" 22 ) def
foo "bb" get
foo "bb" 99 put

/bar ( ) def

bar "bb" 200 put
bar "cc" 300 put
bar "aa" 100 put

bar { } forall

1000

bar "aa" delete
bar "cc" delete
bar "bb" delete
bar "xx" delete

bar { } forall
