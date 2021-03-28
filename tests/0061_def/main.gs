# variable definition

/bar 10 def

/foo1 {
  /bar 20 def
} def

/foo2 {
  /bar 30 def
  bar
  foo1
  bar
} def

bar foo2 bar
