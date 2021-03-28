/foo ( "a" 10 ) def
/bar ( "x" 20 ) def

foo bar setparent

foo "x" 30 put

# the same or different?
foo "x" get
bar "x" get
