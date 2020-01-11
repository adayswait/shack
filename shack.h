#ifndef __SHACK_H__
#define __SHACK_H__

#define SHACK_VERSION "0.1"
#define SHACK_DATE "2020-01-10"
#include <stdint.h> /* for int64_t */

typedef int64_t shack_int;   /* This sets the size of integers in Scheme; it needs to be big enough to accomodate a C pointer. */
typedef double shack_double; /*   similarly for Scheme reals; only double works in C++ */

#ifndef __cplusplus
#ifndef _MSC_VER
#include <stdbool.h>
#else
#ifndef true
#define bool unsigned char
#define true 1
#define false 0
#endif
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct shack_scheme shack_scheme;
  typedef struct shack_cell *shack_pointer;

  shack_scheme *shack_init(void);

  /* shack_scheme is our interpreter
   * shack_pointer is a Scheme object of any (Scheme) type
   * shack_init creates the interpreter.
   */

  typedef shack_pointer (*shack_function)(shack_scheme *sc, shack_pointer args); /* that is, obj = func(shack, args) -- args is a list of arguments */

  shack_pointer shack_f(shack_scheme *sc);                        /* #f */
  shack_pointer shack_t(shack_scheme *sc);                        /* #t */
  shack_pointer shack_nil(shack_scheme *sc);                      /* () */
  shack_pointer shack_undefined(shack_scheme *sc);                /* #<undefined> */
  shack_pointer shack_unspecified(shack_scheme *sc);              /* #<unspecified> */
  bool shack_is_unspecified(shack_scheme *sc, shack_pointer val); /*     returns true if val is #<unspecified> */
  shack_pointer shack_eof_object(shack_scheme *sc);               /* #<eof> */
  bool shack_is_null(shack_scheme *sc, shack_pointer p);          /* null? */

  /* these are the Scheme constants; they do not change in value during a run,
   *   so they can be safely assigned to C global variables if desired. 
   */

  bool shack_is_valid(shack_scheme *sc, shack_pointer arg); /* does 'arg' look like an shack object? */
  bool shack_is_c_pointer(shack_pointer arg);               /* (c-pointer? arg) */
  bool shack_is_c_pointer_of_type(shack_pointer arg, shack_pointer type);
  void *shack_c_pointer(shack_pointer p);
  shack_pointer shack_c_pointer_type(shack_pointer p);
  shack_pointer shack_make_c_pointer(shack_scheme *sc, void *ptr); /* these are for passing uninterpreted C pointers through Scheme */
  shack_pointer shack_make_c_pointer_with_type(shack_scheme *sc, void *ptr, shack_pointer type, shack_pointer info);

  shack_pointer shack_eval_c_string(shack_scheme *sc, const char *str); /* (eval-string str) */
  shack_pointer shack_eval_c_string_with_environment(shack_scheme *sc, const char *str, shack_pointer e);
  shack_pointer shack_object_to_string(shack_scheme *sc, shack_pointer arg, bool use_write);
  /* (object->string obj) */
  char *shack_object_to_c_string(shack_scheme *sc, shack_pointer obj); /* same as object->string but returns a C char* directly */
                                                                       /*   the returned value should be freed by the caller */

  shack_pointer shack_load(shack_scheme *sc, const char *file); /* (load file) */
  shack_pointer shack_load_with_environment(shack_scheme *sc, const char *filename, shack_pointer e);
  shack_pointer shack_load_path(shack_scheme *sc);                                                      /* *load-path* */
  shack_pointer shack_add_to_load_path(shack_scheme *sc, const char *dir);                              /* (set! *load-path* (cons dir *load-path*)) */
  shack_pointer shack_autoload(shack_scheme *sc, shack_pointer symbol, shack_pointer file_or_function); /* (autoload symbol file-or-function) */

  /* the load path is a list of directories to search if load can't find the file passed as its argument.
   *
   *   shack_load and shack_load_with_environment can load shared object files as well as scheme code.
   *   The scheme (load "somelib.so" (inlet 'init_func 'somelib_init)) is equivalent to
   *     shack_load_with_environment(shack, "somelib.so", shack_inlet(shack, shack_list(shack, 2, shack_make_symbol(shack, "init_func"), shack_make_symbol(shack, "somelib_init"))))
   *   shack_load_with_environment returns NULL if it can't load the file.
   */
  void shack_quit(shack_scheme *sc);
  /* this tries to break out of the current evaluation, leaving everything else intact */

  void (*shack_begin_hook(shack_scheme *sc))(shack_scheme *sc, bool *val);
  void shack_set_begin_hook(shack_scheme *sc, void (*hook)(shack_scheme *sc, bool *val));
  /* call "hook" at the start of any block; use NULL to cancel.
   *   shack_begin_hook returns the current begin_hook function or NULL.
   */

  shack_pointer shack_eval(shack_scheme *sc, shack_pointer code, shack_pointer e); /* (eval code e) -- e is the optional environment */
  void shack_provide(shack_scheme *sc, const char *feature);                       /* add feature (as a symbol) to the *features* list */
  bool shack_is_provided(shack_scheme *sc, const char *feature);                   /* (provided? feature) */
  void shack_repl(shack_scheme *sc);

  shack_pointer shack_error(shack_scheme *sc, shack_pointer type, shack_pointer info);
  shack_pointer shack_wrong_type_arg_error(shack_scheme *sc, const char *caller, shack_int arg_n, shack_pointer arg, const char *descr);
  /* set arg_n to 0 to indicate that caller takes only one argument (so the argument number need not be reported */
  shack_pointer shack_out_of_range_error(shack_scheme *sc, const char *caller, shack_int arg_n, shack_pointer arg, const char *descr);
  shack_pointer shack_wrong_number_of_args_error(shack_scheme *sc, const char *caller, shack_pointer args);

  /* these are equivalent to (error ...) in Scheme
   *   the first argument to shack_error is a symbol that can be caught (via (catch tag ...))
   *   the rest of the arguments are passed to the error handler (if in catch) 
   *   or printed out (in the default case).  If the first element of the list
   *   of args ("info") is a string, the default error handler treats it as
   *   a format control string, and passes it to format with the rest of the
   *   info list as the format function arguments.
   *
   *   shack_wrong_type_arg_error is equivalent to shack_error with a type of 'wrong-type-arg
   *   and similarly shack_out_of_range_error with type 'out-of-range.
   *
   * catch in Scheme is taken from Guile:
   *
   *  (catch tag thunk handler)
   *
   *  evaluates 'thunk'.  If an error occurs, and the type matches 'tag' (or if 'tag' is #t),
   *  the handler is called, passing it the arguments (including the type) passed to the
   *  error function.  If no handler is found, the default error handler is called,
   *  normally printing the error arguments to current-error-port.
   */

  shack_pointer shack_stacktrace(shack_scheme *sc);
  shack_pointer shack_history(shack_scheme *sc);                             /* the current (circular backwards) history buffer */
  shack_pointer shack_add_to_history(shack_scheme *sc, shack_pointer entry); /* add entry to the history buffer */
  bool shack_history_enabled(shack_scheme *sc);
  bool shack_set_history_enabled(shack_scheme *sc, bool enabled);

  shack_pointer shack_gc_on(shack_scheme *sc, bool on); /* (gc on) */
  void shack_set_gc_stats(shack_scheme *sc, bool on);   /* (set! (*shack* 'gc-stats) on) */

  shack_int shack_gc_protect(shack_scheme *sc, shack_pointer x);
  void shack_gc_unprotect_at(shack_scheme *sc, shack_int loc);
  shack_pointer shack_gc_protected_at(shack_scheme *sc, shack_int loc);
  shack_pointer shack_gc_protect_via_stack(shack_scheme *sc, shack_pointer x);
  shack_pointer shack_gc_unprotect_via_stack(shack_scheme *sc, shack_pointer x);
  shack_pointer shack_gc_protect_via_location(shack_scheme *sc, shack_pointer x, shack_int loc);
  shack_pointer shack_gc_unprotect_via_location(shack_scheme *sc, shack_int loc);

  /* any shack_pointer object held in C (as a local variable for example) needs to be
   *   protected from garbage collection if there is any chance the GC may run without
   *   an existing Scheme-level reference to that object.  shack_gc_protect places the
   *   object in a vector that the GC always checks, returning the object's location
   *   in that table.  shack_gc_unprotect_at unprotects the object (removes it from the
   *   vector) using the location passed to it.  shack_gc_protected_at returns the object 
   *   at the given location.
   * 
   * You can turn the GC on and off via shack_gc_on.
   *
   * There is a built-in lag between the creation of a new object and its first possible GC
   *    (the lag time is set indirectly by GC_TEMPS_SIZE in shack.c), so you don't need to worry about
   *    very short term temps such as the arguments to shack_cons in:
   *
   *    shack_cons(shack, shack_make_real(shack, 3.14), 
   *                shack_cons(shack, shack_make_integer(shack, 123), shack_nil(shack)));
   */

  bool shack_is_eq(shack_pointer a, shack_pointer b);                           /* (eq? a b) */
  bool shack_is_eqv(shack_pointer a, shack_pointer b);                          /* (eqv? a b) */
  bool shack_is_equal(shack_scheme *sc, shack_pointer a, shack_pointer b);      /* (equal? a b) */
  bool shack_is_equivalent(shack_scheme *sc, shack_pointer x, shack_pointer y); /* (equivalent? x y) */

  bool shack_is_boolean(shack_pointer x);                     /* (boolean? x) */
  bool shack_boolean(shack_scheme *sc, shack_pointer x);      /* Scheme boolean -> C bool */
  shack_pointer shack_make_boolean(shack_scheme *sc, bool x); /* C bool -> Scheme boolean */

  /* for each Scheme type (boolean, integer, string, etc), there are three
   *   functions: shack_<type>(...), shack_make_<type>(...), and shack_is_<type>(...):
   *
   *   shack_boolean(shack, obj) returns the C bool corresponding to the value of 'obj' (#f -> false)
   *   shack_make_boolean(shack, false|true) returns the shack boolean corresponding to the C bool argument (false -> #f)
   *   shack_is_boolean(shack, obj) returns true if 'obj' has a boolean value (#f or #t).
   */

  bool shack_is_pair(shack_pointer p);                                          /* (pair? p) */
  shack_pointer shack_cons(shack_scheme *sc, shack_pointer a, shack_pointer b); /* (cons a b) */

  shack_pointer shack_car(shack_pointer p); /* (car p) */
  shack_pointer shack_cdr(shack_pointer p); /* (cdr p) */

  shack_pointer shack_set_car(shack_pointer p, shack_pointer q); /* (set-car! p q) */
  shack_pointer shack_set_cdr(shack_pointer p, shack_pointer q); /* (set-cdr! p q) */

  shack_pointer shack_cadr(shack_pointer p); /* (cadr p) */
  shack_pointer shack_cddr(shack_pointer p); /* (cddr p) */
  shack_pointer shack_cdar(shack_pointer p); /* (cdar p) */
  shack_pointer shack_caar(shack_pointer p); /* (caar p) */

  shack_pointer shack_caadr(shack_pointer p); /* etc */
  shack_pointer shack_caddr(shack_pointer p);
  shack_pointer shack_cadar(shack_pointer p);
  shack_pointer shack_caaar(shack_pointer p);
  shack_pointer shack_cdadr(shack_pointer p);
  shack_pointer shack_cdddr(shack_pointer p);
  shack_pointer shack_cddar(shack_pointer p);
  shack_pointer shack_cdaar(shack_pointer p);

  shack_pointer shack_caaadr(shack_pointer p);
  shack_pointer shack_caaddr(shack_pointer p);
  shack_pointer shack_caadar(shack_pointer p);
  shack_pointer shack_caaaar(shack_pointer p);
  shack_pointer shack_cadadr(shack_pointer p);
  shack_pointer shack_cadddr(shack_pointer p);
  shack_pointer shack_caddar(shack_pointer p);
  shack_pointer shack_cadaar(shack_pointer p);
  shack_pointer shack_cdaadr(shack_pointer p);
  shack_pointer shack_cdaddr(shack_pointer p);
  shack_pointer shack_cdadar(shack_pointer p);
  shack_pointer shack_cdaaar(shack_pointer p);
  shack_pointer shack_cddadr(shack_pointer p);
  shack_pointer shack_cddddr(shack_pointer p);
  shack_pointer shack_cdddar(shack_pointer p);
  shack_pointer shack_cddaar(shack_pointer p);

  bool shack_is_list(shack_scheme *sc, shack_pointer p);                                               /* (list? p) -> (or (pair? p) (null? p)) */
  bool shack_is_proper_list(shack_scheme *sc, shack_pointer p);                                        /* (proper-list? p) */
  shack_int shack_list_length(shack_scheme *sc, shack_pointer a);                                      /* (length a) */
  shack_pointer shack_list(shack_scheme *sc, shack_int num_values, ...);                               /* (list ...) */
  shack_pointer shack_list_nl(shack_scheme *sc, shack_int num_values, ...);                            /* (list ...) arglist should be NULL terminated (more error checks than shack_list) */
  shack_pointer shack_reverse(shack_scheme *sc, shack_pointer a);                                      /* (reverse a) */
  shack_pointer shack_append(shack_scheme *sc, shack_pointer a, shack_pointer b);                      /* (append a b) */
  shack_pointer shack_list_ref(shack_scheme *sc, shack_pointer lst, shack_int num);                    /* (list-ref lst num) */
  shack_pointer shack_list_set(shack_scheme *sc, shack_pointer lst, shack_int num, shack_pointer val); /* (list-set! lst num val) */
  shack_pointer shack_assoc(shack_scheme *sc, shack_pointer obj, shack_pointer lst);                   /* (assoc obj lst) */
  shack_pointer shack_assq(shack_scheme *sc, shack_pointer obj, shack_pointer x);                      /* (assq obj lst) */
  shack_pointer shack_member(shack_scheme *sc, shack_pointer obj, shack_pointer lst);                  /* (member obj lst) */
  shack_pointer shack_memq(shack_scheme *sc, shack_pointer obj, shack_pointer x);                      /* (memq obj lst) */
  bool shack_tree_memq(shack_scheme *sc, shack_pointer sym, shack_pointer tree);                       /* (tree-memq sym tree) */

  bool shack_is_string(shack_pointer p);                                                         /* (string? p) */
  const char *shack_string(shack_pointer p);                                                     /* Scheme string -> C string (do not free the string) */
  shack_pointer shack_make_string(shack_scheme *sc, const char *str);                            /* C string -> Scheme string (str is copied) */
  shack_pointer shack_make_string_with_length(shack_scheme *sc, const char *str, shack_int len); /* same as shack_make_string, but provides strlen */
  shack_pointer shack_make_string_wrapper(shack_scheme *sc, const char *str);
  shack_pointer shack_make_permanent_string(shack_scheme *sc, const char *str); /* make a string that will never be GC'd */
  shack_int shack_string_length(shack_pointer str);                             /* (string-length str) */

  bool shack_is_character(shack_pointer p);                        /* (character? p) */
  uint8_t shack_character(shack_pointer p);                        /* Scheme character -> unsigned C char */
  shack_pointer shack_make_character(shack_scheme *sc, uint8_t c); /* unsigned C char -> Scheme character */

  bool shack_is_number(shack_pointer p);                             /* (number? p) */
  bool shack_is_integer(shack_pointer p);                            /* (integer? p) */
  shack_int shack_integer(shack_pointer p);                          /* Scheme integer -> C integer (shack_int) */
  shack_pointer shack_make_integer(shack_scheme *sc, shack_int num); /* C shack_int -> Scheme integer */

  bool shack_is_real(shack_pointer p);                               /* (real? p) */
  shack_double shack_real(shack_pointer p);                          /* Scheme real -> C double */
  shack_pointer shack_make_real(shack_scheme *sc, shack_double num); /* C double -> Scheme real */
  shack_pointer shack_make_mutable_real(shack_scheme *sc, shack_double n);
  shack_double shack_number_to_real(shack_scheme *sc, shack_pointer x); /* x can be any kind of number */
  shack_double shack_number_to_real_with_caller(shack_scheme *sc, shack_pointer x, const char *caller);
  shack_int shack_number_to_integer(shack_scheme *sc, shack_pointer x);
  shack_int shack_number_to_integer_with_caller(shack_scheme *sc, shack_pointer x, const char *caller);

  bool shack_is_rational(shack_pointer arg);                                             /* (rational? arg) -- integer or ratio */
  bool shack_is_ratio(shack_pointer arg);                                                /* true if arg is a ratio, not an integer */
  shack_pointer shack_make_ratio(shack_scheme *sc, shack_int a, shack_int b);            /* returns the Scheme object a/b */
  shack_pointer shack_rationalize(shack_scheme *sc, shack_double x, shack_double error); /* (rationalize x error) */
  shack_int shack_numerator(shack_pointer x);                                            /* (numerator x) */
  shack_int shack_denominator(shack_pointer x);                                          /* (denominator x) */
  shack_double shack_random(shack_scheme *sc, shack_pointer state);                      /* (random x) */
  shack_pointer shack_random_state(shack_scheme *sc, shack_pointer seed);                /* (random-state seed) */
  shack_pointer shack_random_state_to_list(shack_scheme *sc, shack_pointer args);        /* (random-state->list r) */
  void shack_set_default_random_state(shack_scheme *sc, shack_int seed, shack_int carry);

  bool shack_is_complex(shack_pointer arg);                                           /* (complex? arg) */
  shack_pointer shack_make_complex(shack_scheme *sc, shack_double a, shack_double b); /* returns the Scheme object a+bi */
  shack_double shack_real_part(shack_pointer z);                                      /* (real-part z) */
  shack_double shack_imag_part(shack_pointer z);                                      /* (imag-part z) */
  char *shack_number_to_string(shack_scheme *sc, shack_pointer obj, shack_int radix); /* (number->string obj radix) */

  bool shack_is_vector(shack_pointer p);            /* (vector? p) */
  shack_int shack_vector_length(shack_pointer vec); /* (vector-length vec) */
  shack_int shack_vector_rank(shack_pointer vect);  /* number of dimensions in vect */
  shack_int shack_vector_dimension(shack_pointer vec, shack_int dim);
  shack_pointer *shack_vector_elements(shack_pointer vec); /* a pointer to the array of shack_pointers */
  shack_int *shack_int_vector_elements(shack_pointer vec);
  shack_double *shack_float_vector_elements(shack_pointer vec);
  bool shack_is_float_vector(shack_pointer p); /* (float-vector? p) */
  bool shack_is_int_vector(shack_pointer p);   /* (int-vector? p) */

  shack_pointer shack_vector_ref(shack_scheme *sc, shack_pointer vec, shack_int index);                                  /* (vector-ref vec index) */
  shack_pointer shack_vector_set(shack_scheme *sc, shack_pointer vec, shack_int index, shack_pointer a);                 /* (vector-set! vec index a) */
  shack_pointer shack_vector_ref_n(shack_scheme *sc, shack_pointer vector, shack_int indices, ...);                      /* multidimensional vector-ref */
  shack_pointer shack_vector_set_n(shack_scheme *sc, shack_pointer vector, shack_pointer value, shack_int indices, ...); /* multidimensional vector-set! */
  shack_int shack_vector_dimensions(shack_pointer vec, shack_int *dims, shack_int dims_size);                            /* vector dimensions */
  shack_int shack_vector_offsets(shack_pointer vec, shack_int *offs, shack_int offs_size);

  shack_pointer shack_make_vector(shack_scheme *sc, shack_int len); /* (make-vector len) */
  shack_pointer shack_make_int_vector(shack_scheme *sc, shack_int len, shack_int dims, shack_int *dim_info);
  shack_pointer shack_make_float_vector(shack_scheme *sc, shack_int len, shack_int dims, shack_int *dim_info);
  shack_pointer shack_make_float_vector_wrapper(shack_scheme *sc, shack_int len, shack_double *data, shack_int dims, shack_int *dim_info, bool free_data);
  shack_pointer shack_make_and_fill_vector(shack_scheme *sc, shack_int len, shack_pointer fill); /* (make-vector len fill) */

  void shack_vector_fill(shack_scheme *sc, shack_pointer vec, shack_pointer obj); /* (vector-fill! vec obj) */
  shack_pointer shack_vector_copy(shack_scheme *sc, shack_pointer old_vect);
  shack_pointer shack_vector_to_list(shack_scheme *sc, shack_pointer vect); /* (vector->list vec) */
  /* 
   *  (vect i) is the same as (vector-ref vect i)
   *  (set! (vect i) x) is the same as (vector-set! vect i x)
   *  (vect i j k) accesses the 3-dimensional vect
   *  (set! (vect i j k) x) sets that element (vector-ref and vector-set! can also be used)
   *  (make-vector (list 2 3 4)) returns a 3-dimensional vector with the given dimension sizes
   *  (make-vector '(2 3) 1.0) returns a 2-dim vector with all elements set to 1.0
   */

  bool shack_is_hash_table(shack_pointer p);                             /* (hash-table? p) */
  shack_pointer shack_make_hash_table(shack_scheme *sc, shack_int size); /* (make-hash-table size) */
  shack_pointer shack_hash_table_ref(shack_scheme *sc, shack_pointer table, shack_pointer key);
  /* (hash-table-ref table key) */
  shack_pointer shack_hash_table_set(shack_scheme *sc, shack_pointer table, shack_pointer key, shack_pointer value);
  /* (hash-table-set! table key value) */

  shack_pointer shack_hook_functions(shack_scheme *sc, shack_pointer hook);                              /* (hook-functions hook) */
  shack_pointer shack_hook_set_functions(shack_scheme *sc, shack_pointer hook, shack_pointer functions); /* (set! (hook-functions hook) ...) */

  bool shack_is_input_port(shack_scheme *sc, shack_pointer p);         /* (input-port? p) */
  bool shack_is_output_port(shack_scheme *sc, shack_pointer p);        /* (output-port? p) */
  const char *shack_port_filename(shack_scheme *sc, shack_pointer x);  /* (port-filename p) */
  shack_int shack_port_line_number(shack_scheme *sc, shack_pointer p); /* (port-line-number p) */

  shack_pointer shack_current_input_port(shack_scheme *sc);                         /* (current-input-port) */
  shack_pointer shack_set_current_input_port(shack_scheme *sc, shack_pointer p);    /* (set-current-input-port) */
  shack_pointer shack_current_output_port(shack_scheme *sc);                        /* (current-output-port) */
  shack_pointer shack_set_current_output_port(shack_scheme *sc, shack_pointer p);   /* (set-current-output-port) */
  shack_pointer shack_current_error_port(shack_scheme *sc);                         /* (current-error-port) */
  shack_pointer shack_set_current_error_port(shack_scheme *sc, shack_pointer port); /* (set-current-error-port port) */
  void shack_close_input_port(shack_scheme *sc, shack_pointer p);                   /* (close-input-port p) */
  void shack_close_output_port(shack_scheme *sc, shack_pointer p);                  /* (close-output-port p) */
  shack_pointer shack_open_input_file(shack_scheme *sc, const char *name, const char *mode);
  /* (open-input-file name mode) */
  shack_pointer shack_open_output_file(shack_scheme *sc, const char *name, const char *mode);
  /* (open-output-file name mode) */
  /* mode here is an optional C style flag, "a" for "alter", etc ("r" is the input default, "w" is the output default) */
  shack_pointer shack_open_input_string(shack_scheme *sc, const char *input_string);
  /* (open-input-string str) */
  shack_pointer shack_open_output_string(shack_scheme *sc);                      /* (open-output-string) */
  const char *shack_get_output_string(shack_scheme *sc, shack_pointer out_port); /* (get-output-string port) -- current contents of output string */
  /*    don't free the string */
  void shack_flush_output_port(shack_scheme *sc, shack_pointer p); /* (flush-output-port port) */

  typedef enum
  {
    SHACK_READ,
    SHACK_READ_CHAR,
    SHACK_READ_LINE,
    SHACK_READ_BYTE,
    SHACK_PEEK_CHAR,
    SHACK_IS_CHAR_READY
  } shack_read_t;
  shack_pointer shack_open_output_function(shack_scheme *sc, void (*function)(shack_scheme *sc, uint8_t c, shack_pointer port));
  shack_pointer shack_open_input_function(shack_scheme *sc, shack_pointer (*function)(shack_scheme *sc, shack_read_t read_choice, shack_pointer port));

  shack_pointer shack_read_char(shack_scheme *sc, shack_pointer port);                   /* (read-char port) */
  shack_pointer shack_peek_char(shack_scheme *sc, shack_pointer port);                   /* (peek-char port) */
  shack_pointer shack_read(shack_scheme *sc, shack_pointer port);                        /* (read port) */
  void shack_newline(shack_scheme *sc, shack_pointer port);                              /* (newline port) */
  shack_pointer shack_write_char(shack_scheme *sc, shack_pointer c, shack_pointer port); /* (write-char c port) */
  shack_pointer shack_write(shack_scheme *sc, shack_pointer obj, shack_pointer port);    /* (write obj port) */
  shack_pointer shack_display(shack_scheme *sc, shack_pointer obj, shack_pointer port);  /* (display obj port) */
  const char *shack_format(shack_scheme *sc, shack_pointer args);                        /* (format ... */

  bool shack_is_syntax(shack_pointer p);                               /* (syntax? p) */
  bool shack_is_symbol(shack_pointer p);                               /* (symbol? p) */
  const char *shack_symbol_name(shack_pointer p);                      /* (symbol->string p) -- don't free the string */
  shack_pointer shack_make_symbol(shack_scheme *sc, const char *name); /* (string->symbol name) */
  shack_pointer shack_gensym(shack_scheme *sc, const char *prefix);    /* (gensym prefix) */

  bool shack_is_keyword(shack_pointer obj);                                   /* (keyword? obj) */
  shack_pointer shack_make_keyword(shack_scheme *sc, const char *key);        /* (string->keyword key) */
  shack_pointer shack_keyword_to_symbol(shack_scheme *sc, shack_pointer key); /* (keyword->symbol key) */

  shack_pointer shack_rootlet(shack_scheme *sc); /* (rootlet) */
  shack_pointer shack_shadow_rootlet(shack_scheme *sc);
  shack_pointer shack_set_shadow_rootlet(shack_scheme *sc, shack_pointer let);
  shack_pointer shack_curlet(shack_scheme *sc);                                                               /* (curlet) */
  shack_pointer shack_set_curlet(shack_scheme *sc, shack_pointer e);                                          /* returns previous curlet */
  shack_pointer shack_outlet(shack_scheme *sc, shack_pointer e);                                              /* (outlet e) */
  shack_pointer shack_sublet(shack_scheme *sc, shack_pointer env, shack_pointer bindings);                    /* (sublet e ...) */
  shack_pointer shack_inlet(shack_scheme *sc, shack_pointer bindings);                                        /* (inlet ...) */
  shack_pointer shack_varlet(shack_scheme *sc, shack_pointer env, shack_pointer symbol, shack_pointer value); /* (varlet env symbol value) */
  shack_pointer shack_let_to_list(shack_scheme *sc, shack_pointer env);                                       /* (let->list env) */
  bool shack_is_let(shack_pointer e);                                                                         /* )let? e) */
  shack_pointer shack_let_ref(shack_scheme *sc, shack_pointer env, shack_pointer sym);                        /* (let-ref e sym) */
  shack_pointer shack_let_set(shack_scheme *sc, shack_pointer env, shack_pointer sym, shack_pointer val);     /* (let-set! e sym val) */
  shack_pointer shack_openlet(shack_scheme *sc, shack_pointer e);                                             /* (openlet e) */
  bool shack_is_openlet(shack_pointer e);                                                                     /* (openlet? e) */
  shack_pointer shack_method(shack_scheme *sc, shack_pointer obj, shack_pointer method);

  shack_pointer shack_name_to_value(shack_scheme *sc, const char *name); /* name's value in the current environment (after turning name into a symbol) */
  shack_pointer shack_symbol_table_find_name(shack_scheme *sc, const char *name);
  shack_pointer shack_symbol_value(shack_scheme *sc, shack_pointer sym);
  shack_pointer shack_symbol_set_value(shack_scheme *sc, shack_pointer sym, shack_pointer val);
  shack_pointer shack_symbol_local_value(shack_scheme *sc, shack_pointer sym, shack_pointer local_env);
  bool shack_for_each_symbol_name(shack_scheme *sc, bool (*symbol_func)(const char *symbol_name, void *data), void *data);
  bool shack_for_each_symbol(shack_scheme *sc, bool (*symbol_func)(const char *symbol_name, void *data), void *data);

  /* these access the current environment and symbol table, providing
   *   a symbol's current binding (shack_name_to_value takes the symbol name as a char*,
   *   shack_symbol_value takes the symbol itself, shack_symbol_set_value changes the
   *   current binding, and shack_symbol_local_value uses the environment passed
   *   as its third argument).
   *
   * To iterate over the complete symbol table, use shack_for_each_symbol_name,
   *   and shack_for_each_symbol.  Both call 'symbol_func' on each symbol, passing it
   *   the symbol or symbol name, and the uninterpreted 'data' pointer.
   *   the current binding. The for-each loop stops if the symbol_func returns true, 
   *   or at the end of the table.
   */

  shack_pointer shack_dynamic_wind(shack_scheme *sc, shack_pointer init, shack_pointer body, shack_pointer finish);

  bool shack_is_immutable(shack_pointer p);
  shack_pointer shack_immutable(shack_pointer p);

  void shack_define(shack_scheme *sc, shack_pointer env, shack_pointer symbol, shack_pointer value);
  bool shack_is_defined(shack_scheme *sc, const char *name);
  shack_pointer shack_define_variable(shack_scheme *sc, const char *name, shack_pointer value);
  shack_pointer shack_define_variable_with_documentation(shack_scheme *sc, const char *name, shack_pointer value, const char *help);
  shack_pointer shack_define_constant(shack_scheme *sc, const char *name, shack_pointer value);
  shack_pointer shack_define_constant_with_documentation(shack_scheme *sc, const char *name, shack_pointer value, const char *help);
  /* These functions add a symbol and its binding to either the top-level environment
   *    or the 'env' passed as the second argument to shack_define.
   *
   *    shack_define_variable(sc, "*features*", sc->NIL);
   *
   * in shack.c is equivalent to the top level form
   *
   *    (define *features* ())
   *
   * shack_define_variable is simply shack_define with string->symbol and the global environment.
   * shack_define_constant is shack_define but makes its "definee" immutable.
   * shack_define is equivalent to define in Scheme.
   */

  bool shack_is_function(shack_pointer p);
  bool shack_is_procedure(shack_pointer x);               /* (procedure? x) */
  bool shack_is_macro(shack_scheme *sc, shack_pointer x); /* (macro? x) */
  shack_pointer shack_closure_body(shack_scheme *sc, shack_pointer p);
  shack_pointer shack_closure_let(shack_scheme *sc, shack_pointer p);
  shack_pointer shack_closure_args(shack_scheme *sc, shack_pointer p);
  shack_pointer shack_funclet(shack_scheme *sc, shack_pointer p);            /* (funclet x) */
  bool shack_is_aritable(shack_scheme *sc, shack_pointer x, shack_int args); /* (aritable? x args) */
  shack_pointer shack_arity(shack_scheme *sc, shack_pointer x);              /* (arity x) */
  const char *shack_help(shack_scheme *sc, shack_pointer obj);               /* (help obj) */
  shack_pointer shack_make_continuation(shack_scheme *sc);                   /* call/cc... (see example below) */

  const char *shack_documentation(shack_scheme *sc, shack_pointer p); /* (documentation x) if any (don't free the string) */
  const char *shack_set_documentation(shack_scheme *sc, shack_pointer p, const char *new_doc);
  shack_pointer shack_setter(shack_scheme *sc, shack_pointer obj);                         /* (setter obj) */
  shack_pointer shack_set_setter(shack_scheme *sc, shack_pointer p, shack_pointer setter); /* (set! (setter p) setter) */
  shack_pointer shack_signature(shack_scheme *sc, shack_pointer func);                     /* (signature obj) */
  shack_pointer shack_make_signature(shack_scheme *sc, shack_int len, ...);                /* procedure-signature data */
  shack_pointer shack_make_circular_signature(shack_scheme *sc, shack_int cycle_point, shack_int len, ...);

  shack_pointer shack_make_function(shack_scheme *sc, const char *name, shack_function fnc, shack_int required_args, shack_int optional_args, bool rest_arg, const char *doc);
  shack_pointer shack_make_safe_function(shack_scheme *sc, const char *name, shack_function fnc, shack_int required_args, shack_int optional_args, bool rest_arg, const char *doc);
  shack_pointer shack_make_typed_function(shack_scheme *sc, const char *name, shack_function f,
                                          shack_int required_args, shack_int optional_args, bool rest_arg, const char *doc, shack_pointer signature);

  shack_pointer shack_define_function(shack_scheme *sc, const char *name, shack_function fnc, shack_int required_args, shack_int optional_args, bool rest_arg, const char *doc);
  shack_pointer shack_define_safe_function(shack_scheme *sc, const char *name, shack_function fnc, shack_int required_args, shack_int optional_args, bool rest_arg, const char *doc);
  shack_pointer shack_define_typed_function(shack_scheme *sc, const char *name, shack_function fnc,
                                            shack_int required_args, shack_int optional_args, bool rest_arg,
                                            const char *doc, shack_pointer signature);
  shack_pointer shack_define_unsafe_typed_function(shack_scheme *sc, const char *name, shack_function fnc,
                                                   shack_int required_args, shack_int optional_args, bool rest_arg,
                                                   const char *doc, shack_pointer signature);

  shack_pointer shack_make_function_star(shack_scheme *sc, const char *name, shack_function fnc, const char *arglist, const char *doc);
  shack_pointer shack_make_safe_function_star(shack_scheme *sc, const char *name, shack_function fnc, const char *arglist, const char *doc);
  void shack_define_function_star(shack_scheme *sc, const char *name, shack_function fnc, const char *arglist, const char *doc);
  void shack_define_safe_function_star(shack_scheme *sc, const char *name, shack_function fnc, const char *arglist, const char *doc);
  void shack_define_typed_function_star(shack_scheme *sc, const char *name, shack_function fnc, const char *arglist, const char *doc, shack_pointer signature);
  shack_pointer shack_define_macro(shack_scheme *sc, const char *name, shack_function fnc, shack_int required_args, shack_int optional_args, bool rest_arg, const char *doc);

  /* shack_make_function creates a Scheme function object from the shack_function 'fnc'.
   *   Its name (for shack_describe_object) is 'name', it requires 'required_args' arguments,
   *   can accept 'optional_args' other arguments, and if 'rest_arg' is true, it accepts
   *   a "rest" argument (a list of all the trailing arguments).  The function's documentation
   *   is 'doc'.
   *
   * shack_define_function is the same as shack_make_function, but it also adds 'name' (as a symbol) to the
   *   global (top-level) environment, with the function as its value.  For example, the Scheme
   *   function 'car' is essentially:
   *
   *     shack_pointer g_car(shack_scheme *sc, shack_pointer args) 
   *       {return(shack_car(sc, shack_car(sc, args)));}
   *
   *   then bound to the name "car":
   *
   *     shack_define_function(sc, "car", g_car, 1, 0, false, "(car obj)");
   *                                          one required arg, no optional arg, no "rest" arg
   *
   * shack_is_function returns true if its argument is a function defined in this manner.
   * shack_apply_function applies the function (the result of shack_make_function) to the arguments.
   *
   * shack_define_macro defines a Scheme macro; its arguments are not evaluated (unlike a function),
   *   but its returned value (assumed to be some sort of Scheme expression) is evaluated.
   *
   * Use the "unsafe" definer if the function might call the evaluator itself in some way (shack_apply_function for example),
   *   or messes with shack's stack.
   */

  /* In shack, (define* (name . args) body) or (define name (lambda* args body))
   *   define a function that takes optional (keyword) named arguments.
   *   The "args" is a list that can contain either names (normal arguments),
   *   or lists of the form (name default-value), in any order.  When called,
   *   the names are bound to their default values (or #f), then the function's
   *   current arglist is scanned.  Any name that occurs as a keyword (":name")
   *   precedes that argument's new value.  Otherwise, as values occur, they
   *   are plugged into the environment based on their position in the arglist
   *   (as normal for a function).  So,
   *   
   *   (define* (hi a (b 32) (c "hi")) (list a b c))
   *     (hi 1) -> '(1 32 "hi")
   *     (hi :b 2 :a 3) -> '(3 2 "hi")
   *     (hi 3 2 1) -> '(3 2 1)
   *
   *   :rest causes its argument to be bound to the rest of the arguments at that point.
   *
   * The C connection to this takes the function name, the C function to call, the argument 
   *   list as written in Scheme, and the documentation string.  shack makes sure the arguments
   *   are ordered correctly and have the specified defaults before calling the C function.
   *     shack_define_function_star(sc, "a-func", a_func, "arg1 (arg2 32)", "an example of C define*");
   *   Now (a-func :arg1 2) calls the C function a_func(2, 32). See the example program in shack.html.
   *
   * In shack Scheme, define* can be used just for its optional arguments feature, but that is
   *   included in shack_define_function.  shack_define_function_star implements keyword arguments
   *   for C-level functions (as well as optional/rest arguments).
   */

  shack_pointer shack_apply_function(shack_scheme *sc, shack_pointer fnc, shack_pointer args);
  shack_pointer shack_apply_function_star(shack_scheme *sc, shack_pointer fnc, shack_pointer args);

  shack_pointer shack_call(shack_scheme *sc, shack_pointer func, shack_pointer args);
  shack_pointer shack_call_with_location(shack_scheme *sc, shack_pointer func, shack_pointer args, const char *caller, const char *file, shack_int line);
  shack_pointer shack_call_with_catch(shack_scheme *sc, shack_pointer tag, shack_pointer body, shack_pointer error_handler);

  /* shack_call takes a Scheme function (e.g. g_car above), and applies it to 'args' (a list of arguments) returning the result.
   *   shack_integer(shack_call(shack, g_car, shack_cons(shack, shack_make_integer(shack, 123), shack_nil(shack))));
   *   returns 123.
   *
   * shack_call_with_location passes some information to the error handler.
   * shack_call makes sure some sort of catch exists if an error occurs during the call, but
   *   shack_apply_function does not -- it assumes the catch has been set up already.
   * shack_call_with_catch wraps an explicit catch around a function call ("body" above);
   *   shack_call_with_catch(sc, tag, body, err) is equivalent to (catch tag body err).
   */

  bool shack_is_dilambda(shack_pointer obj);
  shack_pointer shack_dilambda(shack_scheme *sc,
                               const char *name,
                               shack_pointer (*getter)(shack_scheme *sc, shack_pointer args),
                               shack_int get_req_args, shack_int get_opt_args,
                               shack_pointer (*setter)(shack_scheme *sc, shack_pointer args),
                               shack_int set_req_args, shack_int set_opt_args,
                               const char *documentation);
  shack_pointer shack_typed_dilambda(shack_scheme *sc,
                                     const char *name,
                                     shack_pointer (*getter)(shack_scheme *sc, shack_pointer args),
                                     shack_int get_req_args, shack_int get_opt_args,
                                     shack_pointer (*setter)(shack_scheme *sc, shack_pointer args),
                                     shack_int set_req_args, shack_int set_opt_args,
                                     const char *documentation,
                                     shack_pointer get_sig, shack_pointer set_sig);

  shack_pointer shack_values(shack_scheme *sc, shack_pointer args); /* (values ...) */

  shack_pointer shack_make_iterator(shack_scheme *sc, shack_pointer e); /* (make-iterator e) */
  bool shack_is_iterator(shack_pointer obj);                            /* (iterator? obj) */
  bool shack_iterator_is_at_end(shack_scheme *sc, shack_pointer obj);   /* (iterator-at-end? obj) */
  shack_pointer shack_iterate(shack_scheme *sc, shack_pointer iter);    /* (iterate iter) */

  void shack_autoload_set_names(shack_scheme *sc, const char **names, shack_int size);

  shack_pointer shack_copy(shack_scheme *sc, shack_pointer args);   /* (copy ...) */
  shack_pointer shack_fill(shack_scheme *sc, shack_pointer args);   /* (fill! ...) */
  shack_pointer shack_type_of(shack_scheme *sc, shack_pointer arg); /* (type-of arg) */

  shack_int shack_print_length(shack_scheme *sc);                                  /* value of (*shack* 'print-length) */
  shack_int shack_set_print_length(shack_scheme *sc, shack_int new_len);           /* sets (*shack* 'print-length), returns old value */
  shack_int shack_float_format_precision(shack_scheme *sc);                        /* value of (*shack* 'float-format-precision) */
  shack_int shack_set_float_format_precision(shack_scheme *sc, shack_int new_len); /* sets (*shack* 'float-format-precision), returns old value */

  /* -------------------------------------------------------------------------------- */
  /* c types/objects */

  void shack_mark(shack_pointer p);

  bool shack_is_c_object(shack_pointer p);
  shack_int shack_c_object_type(shack_pointer obj);
  void *shack_c_object_value(shack_pointer obj);
  void *shack_c_object_value_checked(shack_pointer obj, shack_int type);
  shack_pointer shack_make_c_object(shack_scheme *sc, shack_int type, void *value);
  shack_pointer shack_make_c_object_with_let(shack_scheme *sc, shack_int type, void *value, shack_pointer let);
  shack_pointer shack_c_object_let(shack_pointer obj);
  shack_pointer shack_c_object_set_let(shack_scheme *sc, shack_pointer obj, shack_pointer e);

  shack_int shack_make_c_type(shack_scheme *sc, const char *name);
  void shack_c_type_set_free(shack_scheme *sc, shack_int tag, void (*gc_free)(void *value));
  void shack_c_type_set_equal(shack_scheme *sc, shack_int tag, bool (*equal)(void *value1, void *value2));
  void shack_c_type_set_mark(shack_scheme *sc, shack_int tag, void (*mark)(void *value));
  void shack_c_type_set_ref(shack_scheme *sc, shack_int tag, shack_pointer (*ref)(shack_scheme *sc, shack_pointer args));
  void shack_c_type_set_set(shack_scheme *sc, shack_int tag, shack_pointer (*set)(shack_scheme *sc, shack_pointer args));
  void shack_c_type_set_length(shack_scheme *sc, shack_int tag, shack_pointer (*length)(shack_scheme *sc, shack_pointer args));
  void shack_c_type_set_copy(shack_scheme *sc, shack_int tag, shack_pointer (*copy)(shack_scheme *sc, shack_pointer args));
  void shack_c_type_set_fill(shack_scheme *sc, shack_int tag, shack_pointer (*fill)(shack_scheme *sc, shack_pointer args));
  void shack_c_type_set_reverse(shack_scheme *sc, shack_int tag, shack_pointer (*reverse)(shack_scheme *sc, shack_pointer args));
  void shack_c_type_set_to_list(shack_scheme *sc, shack_int tag, shack_pointer (*to_list)(shack_scheme *sc, shack_pointer args));
  void shack_c_type_set_to_string(shack_scheme *sc, shack_int tag, shack_pointer (*to_string)(shack_scheme *sc, shack_pointer args));
  void shack_c_type_set_getter(shack_scheme *sc, shack_int tag, shack_pointer getter);
  void shack_c_type_set_setter(shack_scheme *sc, shack_int tag, shack_pointer setter);
  /* if a c-object might participate in a cyclic structure, and you want to check its equality to another such object
 *   or get a readable string representing that object, you need to implement the "to_list" and "set" cases above,
 *   and make the type name a function that can recreate the object.  See the <cycle> object in shacktest.scm.
 *   For the copy function, either the first or second argument can be a c-object of the given type.
 */

  /* These functions create a new Scheme object type.  There is a simple example in shack.html.
   *
   * shack_make_c_type creates a new C-based type for Scheme:
   *   free:    the function called when an object of this type is about to be garbage collected
   *   mark:    called during the GC mark pass -- you should call shack_mark
   *            on any embedded shack_pointer associated with the object to protect if from the GC.
   *   equal:   compare two objects of this type; (equal? obj1 obj2)
   *   ref:     a function that is called whenever an object of this type
   *            occurs in the function position (at the car of a list; the rest of the list
   *            is passed to the ref function as the arguments: (obj ...))
   *   set:     a function that is called whenever an object of this type occurs as
   *            the target of a generalized set! (set! (obj ...) val)
   *   length:  the function called when the object is asked what its length is.
   *   copy:    the function called when a copy of the object is needed.
   *   fill:    the function called to fill the object with some value.
   *   reverse: similarly...
   *   to_string: object->string for an object of this type
   *   getter/setter: these help the optimizer handle applicable c-objects (see shacktest.scm for an example)
   *
   * shack_is_c_object returns true if 'p' is a c_object
   * shack_c_object_type returns the c_object's type
   * shack_c_object_value returns the value bound to that c_object (the void *value of shack_make_c_object)
   * shack_make_c_object creates a new Scheme entity of the given type with the given (uninterpreted) value
   * shack_mark marks any Scheme c_object as in-use (use this in the mark function to mark
   *    any embedded shack_pointer variables).
   */

  /* -------------------------------------------------------------------------------- */
  /* the new clm optimizer!  this time for sure! 
 *    d=double, i=integer, v=c_object, p=shack_pointer
 *    first return type, then arg types, d_vd -> returns double takes c_object and double (i.e. a standard clm generator)
 */

  /* It is possible to tell shack to call a foreign function directly, without any scheme-related
 *   overhead.  The call needs to take the form of one of the shack_*_t functions in shack.h.  For example,
 *   one way to call + is to pass it two shack_double arguments and get an shack_double back.  This is the
 *   shack_d_dd_t function (the first letter gives the return type, the rest give successive argument types).
 *   We tell shack about it via shack_set_d_dd_function.  Whenever shack's optimizer encounters + with two arguments
 *   that it (the optimizer) knows are shack_doubles, in a context where an shack_double result is expected,
 *   shack calls the shack_d_dd_t function directly without consing a list of arguments, and without
 *   wrapping up the result as a scheme cell.
 */

  shack_function shack_optimize(shack_scheme *sc, shack_pointer expr);

  typedef shack_double (*shack_float_function)(shack_scheme *sc, shack_pointer args);
  shack_float_function shack_float_optimize(shack_scheme *sc, shack_pointer expr);

  typedef shack_double (*shack_d_t)(void);
  void shack_set_d_function(shack_pointer f, shack_d_t df);
  shack_d_t shack_d_function(shack_pointer f);

  typedef shack_double (*shack_d_d_t)(shack_double x);
  void shack_set_d_d_function(shack_pointer f, shack_d_d_t df);
  shack_d_d_t shack_d_d_function(shack_pointer f);

  typedef shack_double (*shack_d_dd_t)(shack_double x1, shack_double x2);
  void shack_set_d_dd_function(shack_pointer f, shack_d_dd_t df);
  shack_d_dd_t shack_d_dd_function(shack_pointer f);

  typedef shack_double (*shack_d_ddd_t)(shack_double x1, shack_double x2, shack_double x3);
  void shack_set_d_ddd_function(shack_pointer f, shack_d_ddd_t df);
  shack_d_ddd_t shack_d_ddd_function(shack_pointer f);

  typedef shack_double (*shack_d_dddd_t)(shack_double x1, shack_double x2, shack_double x3, shack_double x4);
  void shack_set_d_dddd_function(shack_pointer f, shack_d_dddd_t df);
  shack_d_dddd_t shack_d_dddd_function(shack_pointer f);

  typedef shack_double (*shack_d_v_t)(void *v);
  void shack_set_d_v_function(shack_pointer f, shack_d_v_t df);
  shack_d_v_t shack_d_v_function(shack_pointer f);

  typedef shack_double (*shack_d_vd_t)(void *v, shack_double d);
  void shack_set_d_vd_function(shack_pointer f, shack_d_vd_t df);
  shack_d_vd_t shack_d_vd_function(shack_pointer f);

  typedef shack_double (*shack_d_vdd_t)(void *v, shack_double x1, shack_double x2);
  void shack_set_d_vdd_function(shack_pointer f, shack_d_vdd_t df);
  shack_d_vdd_t shack_d_vdd_function(shack_pointer f);

  typedef shack_double (*shack_d_vid_t)(void *v, shack_int i, shack_double d);
  void shack_set_d_vid_function(shack_pointer f, shack_d_vid_t df);
  shack_d_vid_t shack_d_vid_function(shack_pointer f);

  typedef shack_double (*shack_d_p_t)(shack_pointer p);
  void shack_set_d_p_function(shack_pointer f, shack_d_p_t df);
  shack_d_p_t shack_d_p_function(shack_pointer f);

  typedef shack_double (*shack_d_pd_t)(shack_pointer v, shack_double x);
  void shack_set_d_pd_function(shack_pointer f, shack_d_pd_t df);
  shack_d_pd_t shack_d_pd_function(shack_pointer f);

  typedef shack_double (*shack_d_7pi_t)(shack_scheme *sc, shack_pointer v, shack_int i);
  void shack_set_d_7pi_function(shack_pointer f, shack_d_7pi_t df);
  shack_d_7pi_t shack_d_7pi_function(shack_pointer f);

  typedef shack_double (*shack_d_7pid_t)(shack_scheme *sc, shack_pointer v, shack_int i, shack_double d);
  void shack_set_d_7pid_function(shack_pointer f, shack_d_7pid_t df);
  shack_d_7pid_t shack_d_7pid_function(shack_pointer f);

  typedef shack_double (*shack_d_id_t)(shack_int i, shack_double d);
  void shack_set_d_id_function(shack_pointer f, shack_d_id_t df);
  shack_d_id_t shack_d_id_function(shack_pointer f);

  typedef shack_double (*shack_d_ip_t)(shack_int i, shack_pointer p);
  void shack_set_d_ip_function(shack_pointer f, shack_d_ip_t df);
  shack_d_ip_t shack_d_ip_function(shack_pointer f);

  typedef shack_int (*shack_i_i_t)(shack_int x);
  void shack_set_i_i_function(shack_pointer f, shack_i_i_t df);
  shack_i_i_t shack_i_i_function(shack_pointer f);

  typedef shack_int (*shack_i_7d_t)(shack_scheme *sc, shack_double x);
  void shack_set_i_7d_function(shack_pointer f, shack_i_7d_t df);
  shack_i_7d_t shack_i_7d_function(shack_pointer f);

  typedef shack_int (*shack_i_ii_t)(shack_int i1, shack_int i2);
  void shack_set_i_ii_function(shack_pointer f, shack_i_ii_t df);
  shack_i_ii_t shack_i_ii_function(shack_pointer f);

  typedef shack_int (*shack_i_7p_t)(shack_scheme *sc, shack_pointer p);
  void shack_set_i_7p_function(shack_pointer f, shack_i_7p_t df);
  shack_i_7p_t shack_i_7p_function(shack_pointer f);

  typedef bool (*shack_b_p_t)(shack_pointer p);
  void shack_set_b_p_function(shack_pointer f, shack_b_p_t df);
  shack_b_p_t shack_b_p_function(shack_pointer f);

  typedef shack_pointer (*shack_p_d_t)(shack_scheme *sc, shack_double x);
  void shack_set_p_d_function(shack_pointer f, shack_p_d_t df);
  shack_p_d_t shack_p_d_function(shack_pointer f);

  /* Here is an example of using these functions; 
 * (This example comes from a HackerNews discussion):
 * plus.c:
 * --------
 * #include "shack.h"
 * 
 * shack_pointer g_plusone(shack_scheme *sc, shack_pointer args) {
 *     return (shack_make_integer(sc, shack_integer(shack_car(args)) + 1));
 * }
 * shack_int plusone(shack_int x) { return (x + 1); }
 * 
 * void plusone_init(shack_scheme *sc)
 * {
 *   shack_define_safe_function(sc, "plusone", g_plusone, 1, 0, false, "");
 *   shack_set_i_i_function(shack_name_to_value(sc, "plusone"), plusone);
 * }
 * --------
 * gcc -c plus.c -fPIC -O2 -lm
 * gcc plus.o -shared -o plus.so -ldl -lm -Wl,-export-dynamic
 * repl
 * <1> (load "plus.so" (inlet 'init_func 'plusone_init))
 * --------
 */

  /* -------------------------------------------------------------------------------- */

  /* maybe remove these? */
  shack_pointer shack_slot(shack_scheme *sc, shack_pointer symbol);
  shack_pointer shack_slot_value(shack_pointer slot);
  shack_pointer shack_slot_set_value(shack_scheme *sc, shack_pointer slot, shack_pointer value);
  shack_pointer shack_make_slot(shack_scheme *sc, shack_pointer env, shack_pointer symbol, shack_pointer value);
  void shack_slot_set_real_value(shack_scheme *sc, shack_pointer slot, shack_double value);

  /* -------------------------------------------------------------------------------- */

  /* these will be deprecated and removed eventually */
  shack_pointer shack_apply_1(shack_scheme *sc, shack_pointer args, shack_pointer (*f1)(shack_pointer a1));
  shack_pointer shack_apply_2(shack_scheme *sc, shack_pointer args, shack_pointer (*f2)(shack_pointer a1, shack_pointer a2));
  shack_pointer shack_apply_3(shack_scheme *sc, shack_pointer args, shack_pointer (*f3)(shack_pointer a1, shack_pointer a2, shack_pointer a3));
  shack_pointer shack_apply_4(shack_scheme *sc, shack_pointer args, shack_pointer (*f4)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4));
  shack_pointer shack_apply_5(shack_scheme *sc, shack_pointer args, shack_pointer (*f5)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4, shack_pointer a5));
  shack_pointer shack_apply_6(shack_scheme *sc, shack_pointer args,
                              shack_pointer (*f6)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4,
                                                  shack_pointer a5, shack_pointer a6));
  shack_pointer shack_apply_7(shack_scheme *sc, shack_pointer args,
                              shack_pointer (*f7)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4,
                                                  shack_pointer a5, shack_pointer a6, shack_pointer a7));
  shack_pointer shack_apply_8(shack_scheme *sc, shack_pointer args,
                              shack_pointer (*f8)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4,
                                                  shack_pointer a5, shack_pointer a6, shack_pointer a7, shack_pointer a8));
  shack_pointer shack_apply_9(shack_scheme *sc, shack_pointer args,
                              shack_pointer (*f9)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4,
                                                  shack_pointer a5, shack_pointer a6, shack_pointer a7, shack_pointer a8, shack_pointer a9));

  shack_pointer shack_apply_n_1(shack_scheme *sc, shack_pointer args, shack_pointer (*f1)(shack_pointer a1));
  shack_pointer shack_apply_n_2(shack_scheme *sc, shack_pointer args, shack_pointer (*f2)(shack_pointer a1, shack_pointer a2));
  shack_pointer shack_apply_n_3(shack_scheme *sc, shack_pointer args, shack_pointer (*f3)(shack_pointer a1, shack_pointer a2, shack_pointer a3));
  shack_pointer shack_apply_n_4(shack_scheme *sc, shack_pointer args, shack_pointer (*f4)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4));
  shack_pointer shack_apply_n_5(shack_scheme *sc, shack_pointer args, shack_pointer (*f5)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4, shack_pointer a5));
  shack_pointer shack_apply_n_6(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f6)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4,
                                                    shack_pointer a5, shack_pointer a6));
  shack_pointer shack_apply_n_7(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f7)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4,
                                                    shack_pointer a5, shack_pointer a6, shack_pointer a7));
  shack_pointer shack_apply_n_8(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f8)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4,
                                                    shack_pointer a5, shack_pointer a6, shack_pointer a7, shack_pointer a8));
  shack_pointer shack_apply_n_9(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f9)(shack_pointer a1, shack_pointer a2, shack_pointer a3, shack_pointer a4,
                                                    shack_pointer a5, shack_pointer a6, shack_pointer a7, shack_pointer a8, shack_pointer a9));

#if WITH_GMP
#include <gmp.h>
#include <mpfr.h>
#include <mpc.h>

  bool shack_is_bignum(shack_pointer obj);
  mpfr_t *shack_big_real(shack_pointer x);
  mpz_t *shack_big_integer(shack_pointer x);
  mpq_t *shack_big_ratio(shack_pointer x);
  mpc_t *shack_big_complex(shack_pointer x);
  shack_pointer shack_make_big_integer(shack_scheme *sc, mpz_t *val);
  shack_pointer shack_make_big_ratio(shack_scheme *sc, mpq_t *val);
  shack_pointer shack_make_big_real(shack_scheme *sc, mpfr_t *val);
  shack_pointer shack_make_big_complex(shack_scheme *sc, mpc_t *val);
#endif

/* -------------------------------------------------------------------------------- */
#if (!DISABLE_DEPRECATED)
  typedef shack_int shack_Int;
  typedef shack_double shack_Double;

#define shack_is_object shack_is_c_object
#define shack_object_type shack_c_object_type
#define shack_object_value shack_c_object_value
#define shack_make_object shack_make_c_object
#define shack_mark_object shack_mark
#define shack_UNSPECIFIED(Sc) shack_unspecified(Sc)
#define shack_NIL(Sc) shack_nil(Sc)
#define shack_new_type(Name, Print, GC_Free, Equal, Mark, Ref, Set) shack_new_type_1(shack, Name, Print, GC_Free, Equal, Mark, Ref, Set)
#endif

#ifdef __cplusplus
}
#endif

#endif //__SHACK_H__
