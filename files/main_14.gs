/f {
  /zapp 7 def

  true {
    /a 200 def
    /xxx self gdef
  } if
} def

/Object (
  /type { class }
) nil class def

/Edit (
  /xfoo 0
  /ybar 0

  /foo { f }

  /init { /ybar 200 def /xfoo xfoo 2 mul def }
) Object class def

/edit Edit ( /xfoo 10 /ybar 20 ) new def

edit .foo
edit .type

edit 200 =xfoo
edit "ybar" 200 put

/zapp 10 def
/zapp 2 add!
