/rand_seed 0 def

# ( max -- int )
# int: 0 ... max-1
/rand {
  /rand_seed rand_seed 0xffffffffff and 104729 add 1299709 mul 3 shr def
  rand_seed exch mod
} def

/a [ ] def

300 {
  a 30 rand 40 rand string put
} repeat
