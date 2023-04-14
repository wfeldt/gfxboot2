# syntax erros are ok
"error ok" pop

/foo_1 99 def

/bar {
  foo_1

  /foo_1 11 def
  /foo_2 22 def

  foo_1
  foo_2
} def

bar

foo_1
foo_2