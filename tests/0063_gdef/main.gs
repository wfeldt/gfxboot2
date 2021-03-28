# global variable definition

/bar 10 def

/foo1 {
  /bar 20 gdef
} def

/foo2 {
  /bar 30 ldef
  bar
  foo1
  bar
} def

bar foo2 bar
