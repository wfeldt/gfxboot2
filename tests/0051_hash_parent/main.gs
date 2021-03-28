
/foo ( "aa" 11 "bb" 22 "cc" 33 ) def
/bar ( "xx" 88 "yy" 99 ) def

foo bar setparent

foo

foo "bb" get
foo "abc" get
foo "xx" get

# contruct loop
bar foo setparent

foo "cc" get
bar "aa" get
foo "xyz" get

# break loop for gc
bar nil setparent
