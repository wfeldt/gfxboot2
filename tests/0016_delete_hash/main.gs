/foo ( "aa" 11 "bb" 22 "cc" 33 "dd" 44 "ee" 55 ) def

foo "bb" delete
foo "dd" delete
foo "ff" delete

foo { } forall

foo "aa" delete
foo "ee" delete

foo { } forall

foo "cc" delete

foo "xx" delete

foo { } forall

foo "xx" 99 put

foo { } forall
