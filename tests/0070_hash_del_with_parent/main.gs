/foo ( "a" 10 ) def
/bar ( "x" 20 ) def

foo bar setparent

foo "x" delete

# 20 or nil?
foo "x" get
bar "x" get
