/foo {
  { true { 10000 20000 exit 30000 40000 } if } loop
  50000
} def

{ true { 10 20 exit 30 40 } if } loop
2 { true { 100 200 exit 300 400 } if } repeat
1000 2000 5000 { true { exit } if } for
foo
