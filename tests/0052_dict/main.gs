getdict

/foo_0 99 def

/foo_1 {
  getdict
} def

/foo_2 {
  ( ) setdict
  /bar_2 {
    11
  } def
  getdict
  bar_2
  foo_0
} def

foo_1
foo_2
bar_2
