https://ccrma.stanford.edu/software/snd/snd/s7.html

## shack, a Scheme interpreter

#### history
2020-01-11 : change macro FILENAME_MAX to MAX_PATH_LEN, judge _CRT_FUNCTIONS_REQUIRED, 
change qsort_s param type, build on windows 10
<br/>
2020-01-10 : the begining, change "s7*" to "shack*", build on linux(debian 8)
<br/>
<br/>

#### overview
Documentation is in shack.h and shack.html.
<br/>
shacktest.scm is a regression test.
<br/>
glistener.c is a gtk-based listener.
<br/>
repl.scm is a vt100-based listener.
<br/>
cload.scm and lib*.scm tie in various C libraries.
<br/>
lint.scm checks Scheme code for infelicities.
<br/>
r7rs.scm implements some of r7rs (small).
<br/>
write.scm currrently has pretty-print.
<br/>
mockery.scm has the mock-data definitions.
<br/>
reactive.scm has reactive-set and friends.
<br/>
stuff.scm has some stuff.
<br/>
profile.scm has code to display profile data.
<br/>
timing tests are in the shack tools directory
<br/>
<br/>

#### shack.c is organized as follows:
&ensp;&ensp;  structs and type flags
<br/>&ensp;&ensp;  constants
<br/>&ensp;&ensp;  GC
<br/>&ensp;&ensp;  stacks
<br/>&ensp;&ensp;  symbols and keywords
<br/>&ensp;&ensp;  environments
<br/>&ensp;&ensp;  continuations
<br/>&ensp;&ensp;  numbers
<br/>&ensp;&ensp;  characters
<br/>&ensp;&ensp;  strings
<br/>&ensp;&ensp;  ports
<br/>&ensp;&ensp;  format
<br/>&ensp;&ensp;  lists
<br/>&ensp;&ensp;  vectors
<br/>&ensp;&ensp;  hash-tables
<br/>&ensp;&ensp;  c-objects
<br/>&ensp;&ensp;  functions
<br/>&ensp;&ensp;  equal?
<br/>&ensp;&ensp;  generic length, copy, reverse, fill!, append
<br/>&ensp;&ensp;  error handlers
<br/>&ensp;&ensp;  sundry leftovers
<br/>&ensp;&ensp;  the optimizers
<br/>&ensp;&ensp;  multiple-values, quasiquote
<br/>&ensp;&ensp;  eval
<br/>&ensp;&ensp;  multiprecision arithmetic
<br/>&ensp;&ensp;  *shack* environment
<br/>&ensp;&ensp;  initialization
<br/>&ensp;&ensp;  repl
<br/>
<br/>

#### build
in Linux:  gcc shack.c -o shack -DWITH_MAIN -I. -O2 -g -ldl -lm -Wl,-export-dynamic<br/>
in *BSD:   gcc shack.c -o shack -DWITH_MAIN -I. -O2 -g -lm -Wl,-export-dynamic<br/>
in OSX:    gcc shack.c -o shack -DWITH_MAIN -I. -O2 -g -lm<br/>
(clang also needs LDFLAGS="-Wl,-export-dynamic" in Linux and "-fPIC")<br/>
<br/>

#### naming conventions:
shack_* usually are C accessible (shack.h), g_* are scheme accessible (FFI), <br/>
    H_* are documentation strings, Q_* are procedure signatures, <br/>
    *_1 are ancillary functions, big_* refer to gmp, <br/>
    scheme "?" corresponds to C "is_", scheme "->" to C "_to_".
