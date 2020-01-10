https://ccrma.stanford.edu/software/snd/snd/s7.html

shack, a Scheme interpreter

Documentation is in shack.h and shack.html.
shacktest.scm is a regression test.
glistener.c is a gtk-based listener.
repl.scm is a vt100-based listener.
cload.scm and lib*.scm tie in various C libraries.
lint.scm checks Scheme code for infelicities.
r7rs.scm implements some of r7rs (small).
write.scm currrently has pretty-print.
mockery.scm has the mock-data definitions.
reactive.scm has reactive-set and friends.
stuff.scm has some stuff.
profile.scm has code to display profile data.
timing tests are in the shack tools directory

shack.c is organized as follows:
    structs and type flags
    constants
    GC
    stacks
    symbols and keywords
    environments
    continuations
    numbers
    characters
    strings
    ports
    format
    lists
    vectors
    hash-tables
    c-objects
    functions
    equal?
    generic length, copy, reverse, fill!, append
    error handlers
    sundry leftovers
    the optimizers
    multiple-values, quasiquote
    eval
    multiprecision arithmetic
    *shack* environment
    initialization
    repl

naming conventions: shack_* usually are C accessible (shack.h), g_* are scheme accessible (FFI),
    H_* are documentation strings, Q_* are procedure signatures,
    *_1 are ancillary functions, big_* refer to gmp,
    scheme "?" corresponds to C "is_", scheme "->" to C "_to_".
