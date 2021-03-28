/foo 99 def

/bar {
  foo

  ( /foo nil ) setdict

  foo

  /foo 11 def
  foo
} def

bar

foo
