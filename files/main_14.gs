/Object (
  /type { class }
) nil class def

/Edit (
  /xfoo 0
  /ybar 0

  /foo { xfoo }

  /init { /ybar 200 def /xfoo xfoo 2 mul def }
) Object class def

/edit Edit ( /xfoo 10 /ybar 20 ) new def

edit .foo
edit .type
