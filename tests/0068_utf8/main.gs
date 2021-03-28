/s "ab \xa0 cd \x00 öä € " def

/a s utf8decode def

/e a utf8encode def

e s eq
