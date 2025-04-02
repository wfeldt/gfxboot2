/num_1 1000 def
/num_1 2000 add!

/bool_1 true def
/bool_1 false add!

/string_1 "1234" "" add def
/string_1 "abc" add!

/array_1 [ 10 20 30 40 ] def
/array_1 [ "abc" 100 200 ] add!

/hash_1 ( "aa" 100 "ee" 200 "dd" 300 ) def
/hash_1 ( "cc" 400 "bb" 500 ) add!

num_1
bool_1
string_1
array_1 { } forall
hash_1 { } forall
