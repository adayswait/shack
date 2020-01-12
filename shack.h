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

#if defined(__linux__) || defined(__APPLE__)
#ifdef __cplusplus
#define HAVE_COMPLEX_NUMBERS 1
#define HAVE_COMPLEX_TRIG 0
#else
#define HAVE_COMPLEX_NUMBERS 1
#define HAVE_COMPLEX_TRIG 1
#endif
#elif defined(_WIN32)
#define HAVE_COMPLEX_NUMBERS 0
#define HAVE_COMPLEX_TRIG 0
#define MUS_CONFIG_H_LOADED 1
#else
#define HAVE_COMPLEX_NUMBERS 0
#define HAVE_COMPLEX_TRIG 0
#endif

/*
 * Your config file goes here, or just replace that #include line with the defines you need.
 * The compile-time switches involve booleans, complex numbers, and multiprecision arithmetic.
 * Currently we assume we have setjmp.h (used by the error handlers).
 *
 * Complex number support which is problematic in C++, Solaris, and netBSD
 *   is on the HAVE_COMPLEX_NUMBERS switch. In OSX or Linux, if you're not using C++,
 *
 *   #define HAVE_COMPLEX_NUMBERS 1
 *   #define HAVE_COMPLEX_TRIG 1
 *
 *   In C++ I use:
 *
 *   #define HAVE_COMPLEX_NUMBERS 1
 *   #define HAVE_COMPLEX_TRIG 0
 *
 *   In windows, both are 0.
 *
 *   Some systems (FreeBSD) have complex.h, but some random subset of the trig funcs, so
 *   HAVE_COMPLEX_NUMBERS means we can find
 *      cimag creal cabs csqrt carg conj
 *   and HAVE_COMPLEX_TRIG means we have
 *      cacos cacosh casin casinh catan catanh ccos ccosh cexp clog cpow csin csinh ctan ctanh
 *
 * When HAVE_COMPLEX_NUMBERS is 0, the complex functions are stubs that simply return their
 *   argument -- this will be very confusing for the shack user because, for example, (sqrt -2)
 *   will return something bogus (it will not signal an error).
 *
 * so the incoming (non-shack-specific) compile-time switches are
 *     HAVE_COMPLEX_NUMBERS, HAVE_COMPLEX_TRIG, SIZEOF_VOID_P
 * if SIZEOF_VOID_P is not defined, we look for __SIZEOF_POINTER__ instead
 *   the default is to assume that we're running on a 64-bit machine.
 *
 * To get multiprecision arithmetic, set WITH_GMP to 1.
 *   You'll also need libgmp, libmpfr, and libmpc (version 0.8.0 or later)
 *   In highly numerical contexts, the gmp version of shack is about 50(!) times slower than the non-gmp version.
 *
 * and we use these predefined macros: __cplusplus, _MSC_VER, __GNUC__, __clang__, __ANDROID__
 *
 * if WITH_SYSTEM_EXTRAS is 1 (default is 1 unless _MSC_VER), various OS and file related functions are included.
 * in openBSD I think you need to include -ftrampolines in CFLAGS.
 * if you want this file to compile into a stand-alone interpreter, define WITH_MAIN
 *
 * -O3 produces segfaults, and is often slower than -O2 (at least according to callgrind)
 * -march=native seems to improve tree-vectorization which is important in Snd
 * -ffast-math makes a mess of NaNs, and does not appear to be faster
 * for timing tests, I use: -O2 -march=native -fomit-frame-pointer -funroll-loops
 *   some say -funroll-loops has no effect, but it is consistently faster (according to callgrind) in shack's timing tests
 * according to callgrind, clang is normally about 10% slower than gcc, and vectorization either doesn't work or is much worse than gcc's
 *   also g++ appears to be slightly slower than gcc
 */

#if (defined(__GNUC__) || defined(__clang__)) /* shack uses PRId64 so (for example) g++ 4.4 is too old */
#define WITH_GCC 1
#else
#define WITH_GCC 0
#endif

/* ---------------- initial sizes ---------------- */

#ifndef INITIAL_HEAP_SIZE
#define INITIAL_HEAP_SIZE 128000
#endif
/* the heap grows as needed, this is its initial size.
 * If the initial heap is small, shack can run in about 2.5 Mbytes of memory. There are (many) cases where a bigger heap is faster.
 * The heap size must be a multiple of 32.  Each object takes 48 bytes.
 */

#ifndef SYMBOL_TABLE_SIZE
#define SYMBOL_TABLE_SIZE 32749
#endif
/* names are hashed into the symbol table (a vector) and collisions are chained as lists. */

#ifndef INITIAL_STACK_SIZE
#define INITIAL_STACK_SIZE 512
#endif
/* the stack grows as needed, each frame takes 4 entries, this is its initial size.
 * this needs to be big enough to handle the eval_c_string's at startup (ca 100)
 * In shacktest.scm, the maximum stack size is ca 440.  In snd-test.scm, it's ca 200.
 * This number matters only because call/cc copies the stack, which requires filling
 * the unused portion of the new stack, which requires memcpy of #<unspecified>'s.
 */

#ifndef INITIAL_PROTECTED_OBJECTS_SIZE
#define INITIAL_PROTECTED_OBJECTS_SIZE 16
#endif
/* a vector of objects that are (semi-permanently) protected from the GC, grows as needed */

#ifndef GC_TEMPS_SIZE
#define GC_TEMPS_SIZE 256
#endif
/* the number of recent objects that are temporarily gc-protected; 8 works for shacktest and snd-test.
 * For the FFI, this sets the lag between a call on shack_cons and the first moment when its result
 * might be vulnerable to the GC.
 */

/* ---------------- scheme choices ---------------- */

#ifndef WITH_GMP
#define WITH_GMP 0
/* this includes multiprecision arithmetic for all numeric types and functions, using gmp, mpfr, and mpc
   * WITH_GMP adds the following functions: bignum and bignum?, and (*shack* 'bignum-precision)
   * using gmp with precision=128 is about 50 times slower than using C doubles and int64_t.
   */
#endif

#define DEFAULT_BIGNUM_PRECISION 128

#ifndef WITH_PURE_SHACK
#define WITH_PURE_SHACK 0
#endif
#if WITH_PURE_SHACK
#define WITH_EXTRA_EXPONENT_MARKERS 0
#define WITH_IMMUTABLE_UNQUOTE 1
/* also omitted: *-ci* functions, char-ready?, cond-expand, multiple-values-bind|set!, call-with-values
      *   and a lot more (inexact/exact, integer-length,  etc) -- see shack.html.
      */
#endif

#ifndef WITH_EXTRA_EXPONENT_MARKERS
#define WITH_EXTRA_EXPONENT_MARKERS 0
#endif
/* if 1, shack recognizes "d", "f", "l", and "s" as exponent markers, in addition to "e" (also "D", "F", "L", "S") */

#ifndef WITH_SYSTEM_EXTRAS
#define WITH_SYSTEM_EXTRAS (!_MSC_VER)
/* this adds several functions that access file info, directories, times, etc
   *    this may be replaced by the cload business below
   */
#endif

#ifndef WITH_IMMUTABLE_UNQUOTE
#define WITH_IMMUTABLE_UNQUOTE 0
/* this removes the name "unquote" */
#endif

#ifndef WITH_C_LOADER
#if WITH_GCC
#define WITH_C_LOADER 1
/* (load file.so [e]) looks for (e 'init_func) and if found, calls it
   *   as the shared object init function.  If WITH_SYSTEM_EXTRAS is 0, the caller
   *   needs to supply system and delete-file so that cload.scm works.
   */
#else
#define WITH_C_LOADER 0
#endif
#endif

#ifndef WITH_HISTORY
#define WITH_HISTORY 0
/* this includes a circular buffer of previous evaluations for debugging, ((owlet) 'error-history) and (*shack* 'history-size) */
#endif

#ifndef DEFAULT_HISTORY_SIZE
#define DEFAULT_HISTORY_SIZE 8
/* this is the default length of the eval history buffer */
#endif
#if WITH_HISTORY
#define MAX_HISTORY_SIZE 1048576
#endif

#define DEFAULT_PRINT_LENGTH 32 /* (*shack* 'print-length) */

#ifndef WITH_PROFILE
#define WITH_PROFILE 0
/* this includes profiling data collection accessible from scheme via the hash-table (*shack* 'profile-info) */
#endif

/* in case mus-config.h forgets these */
#ifdef _MSC_VER
#ifndef HAVE_COMPLEX_NUMBERS
#define HAVE_COMPLEX_NUMBERS 0
#endif
#ifndef HAVE_COMPLEX_TRIG
#define HAVE_COMPLEX_TRIG 0
#endif
#else
#ifndef HAVE_COMPLEX_NUMBERS
#define HAVE_COMPLEX_NUMBERS 1
#endif
#if __cplusplus
#ifndef HAVE_COMPLEX_TRIG
#define HAVE_COMPLEX_TRIG 0
#endif
#else
#ifndef HAVE_COMPLEX_TRIG
#define HAVE_COMPLEX_TRIG 1
#endif
#endif
#endif

#ifndef WITH_MULTITHREAD_CHECKS
#define WITH_MULTITHREAD_CHECKS 0
/* debugging aid if using shack in a multithreaded program
   -- this code courtesy of Kjetil Matheussen */
#endif

#ifndef SHACK_DEBUGGING
#define SHACK_DEBUGGING 0
#endif

#undef DEBUGGING
#define DEBUGGING typo !

#define SHOW_EVAL_OPS 0

#ifndef OP_NAMES
#define OP_NAMES SHOW_EVAL_OPS
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
/* for qsort_r, grumble... */
#endif

#ifndef _MSC_VER
#include <unistd.h>
#include <sys/param.h>
#include <strings.h>
#include <errno.h>
#include <locale.h>
#else
/* in Snd these are in mus-config.h */
#ifndef MUS_CONFIG_H_LOADED
#define snprintf _snprintf
#if _MSC_VER > 1200
#define _CRT_SECURE_NO_DEPRECATE 1
#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#endif
#endif
#include <io.h>
#ifndef F_OK
#define F_OK 0 /* Check for file existence */
#endif
#ifndef X_OK
#define X_OK 1 /* Check for execute permission. */
#endif
#ifndef W_OK
#define W_OK 2 /* Check for write permission */
#endif
#ifndef R_OK
#define R_OK 4 /* Check for read permission */
#endif
#pragma warning(disable : 4244) /* conversion might cause loss of data warning */
#endif

#ifndef WITH_VECTORIZE
#if (defined(__GNUC__) && __GNUC__ >= 5)
#define WITH_VECTORIZE 1
#else
#define WITH_VECTORIZE 0
#endif
#endif

#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <setjmp.h>

#ifdef _MSC_VER
#define MS_WINDOWS 1
#else
#define MS_WINDOWS 0
#endif

#if (!MS_WINDOWS)
#include <pthread.h>
#endif

#if __cplusplus
#include <cmath>
#else
#include <math.h>
#endif

#if HAVE_COMPLEX_NUMBERS
#if __cplusplus
#include <complex>
#else
#include <complex.h>
#ifndef __SUNPRO_C
#if defined(__sun) && defined(__SVR4)
#undef _Complex_I
#define _Complex_I 1.0fi
#endif
#endif
#endif

#ifndef CMPLX
#if (!(defined(__cplusplus))) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)) && !defined(__INTEL_COMPILER)
#define CMPLX(x, y) __builtin_complex((double)(x), (double)(y))
#else
#define CMPLX(r, i) ((r) + ((i)*_Complex_I))
#endif
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /*shack_scheme is our interpreter*/
    typedef struct shack_scheme shack_scheme;

    /*shack_pointer is a scheme object of any (scheme) type*/
    typedef struct shack_cell *shack_pointer;

    /*creates the interpreter.*/
    shack_scheme *shack_init(void);

    /* that is, obj = func(shack, args) -- args is a list of arguments */
    typedef shack_pointer (*shack_function)(shack_scheme *sc, shack_pointer args);

    /*the followings are the Scheme constants; they don't change in value during
     *a run, so they can be safely assigned to C global variables if desired. */
    /* #f */
    shack_pointer shack_f(shack_scheme *sc);

    /* #t */
    shack_pointer shack_t(shack_scheme *sc);

    /* () */
    shack_pointer shack_nil(shack_scheme *sc);

    /* #<undefined> */
    shack_pointer shack_undefined(shack_scheme *sc);

    /* #<unspecified> */
    shack_pointer shack_unspecified(shack_scheme *sc);

    /* returns true if val is #<unspecified> */
    bool shack_is_unspecified(shack_scheme *sc, shack_pointer val);

    /* #<eof> */
    shack_pointer shack_eof_object(shack_scheme *sc);

    /* null? */
    bool shack_is_null(shack_scheme *sc, shack_pointer p);

    /* does 'arg' look like an shack object? */
    bool shack_is_valid(shack_scheme *sc, shack_pointer arg);
    /* (c-pointer? arg) */
    bool shack_is_c_pointer(shack_pointer arg);
    bool shack_is_c_pointer_of_type(shack_pointer arg, shack_pointer type);
    void *shack_c_pointer(shack_pointer p);
    shack_pointer shack_c_pointer_type(shack_pointer p);

    /* these are for passing uninterpreted C pointers through Scheme */
    shack_pointer shack_make_c_pointer(shack_scheme *sc, void *ptr);
    shack_pointer shack_make_c_pointer_with_type(shack_scheme *sc,
                                                 void *ptr,
                                                 shack_pointer type,
                                                 shack_pointer info);

    /* (eval-string str) */
    shack_pointer shack_eval_c_string(shack_scheme *sc, const char *str);
    shack_pointer shack_eval_c_string_with_environment(shack_scheme *sc,
                                                       const char *str,
                                                       shack_pointer e);
    shack_pointer shack_object_to_string(shack_scheme *sc,
                                         shack_pointer arg,
                                         bool use_write);

    /* (object->string obj) */
    /*same as object->string but returns a C char* directly*/
    /*the returned value should be freed by the caller */
    char *shack_object_to_c_string(shack_scheme *sc, shack_pointer obj);

    /* (load file) */
    shack_pointer shack_load(shack_scheme *sc, const char *file);
    shack_pointer shack_load_with_environment(shack_scheme *sc,
                                              const char *filename,
                                              shack_pointer e);

    /* *load-path* */
    shack_pointer shack_load_path(shack_scheme *sc);

    /* (set! *load-path* (cons dir *load-path*)) */
    shack_pointer shack_add_to_load_path(shack_scheme *sc, const char *dir);
    /* (autoload symbol file-or-function) */
    shack_pointer shack_autoload(shack_scheme *sc, shack_pointer symbol,
                                 shack_pointer file_or_function);

    /**the load path is a list of directories to search if load can't find the
     * file passed as its argument.
     * shack_load and shack_load_with_environment can load shared object files
     * as well as scheme code.
     * The scheme (load "somelib.so" (inlet 'init_func 'somelib_init)) is
     * equivalent to 
     * shack_load_with_environment(shack, "somelib.so", shack_inlet(shack,
     *     shack_list(shack, 2, shack_make_symbol(shack, "init_func"),
     *     shack_make_symbol(shack, "somelib_init"))))
     * shack_load_with_environment returns NULL if it can't load the file.
     */

    /* this tries to break out of the current evaluation,
       leaving everything else intact */
    void shack_quit(shack_scheme *sc);

    void (*shack_begin_hook(shack_scheme *sc))(shack_scheme *sc, bool *val);

    /* call "hook" at the start of any block; use NULL to cancel.
     * shack_begin_hook returns the current begin_hook function or NULL.*/
    void shack_set_begin_hook(shack_scheme *sc,
                              void (*hook)(shack_scheme *sc, bool *val));

    /* (eval code e) -- e is the optional environment */
    shack_pointer shack_eval(shack_scheme *sc, shack_pointer code,
                             shack_pointer e);

    /* add feature (as a symbol) to the *features* list */
    void shack_provide(shack_scheme *sc, const char *feature);

    /* (provided? feature) */
    bool shack_is_provided(shack_scheme *sc, const char *feature);
    void shack_repl(shack_scheme *sc);

    shack_pointer shack_error(shack_scheme *sc, shack_pointer type,
                              shack_pointer info);
    shack_pointer shack_wrong_type_arg_error(shack_scheme *sc,
                                             const char *caller,
                                             shack_int arg_n,
                                             shack_pointer arg,
                                             const char *descr);
    /* set arg_n to 0 to indicate that caller takes only one argument,
       so the argument number need not be reported */
    shack_pointer shack_out_of_range_error(shack_scheme *sc, const char *caller,
                                           shack_int arg_n, shack_pointer arg,
                                           const char *descr);
    shack_pointer shack_wrong_number_of_args_error(shack_scheme *sc,
                                                   const char *caller,
                                                   shack_pointer args);

    /* these are equivalent to (error ...) in Scheme
     * the first argument to shack_error is a symbol that can be caught
     * (via (catch tag ...))
     * the rest of the arguments are passed to the error handler (if in catch)
     * or printed out (in the default case).  If the first element of the list
     * of args ("info") is a string, the default error handler treats it as
     * a format control string, and passes it to format with the rest of the
     * info list as the format function arguments.
     *
     * shack_wrong_type_arg_error is equivalent to shack_error with a type of
     * 'wrong-type-arg and similarly shack_out_of_range_error with type 
     * 'out-of-range.
     *
     * catch in Scheme is taken from Guile:
     * 
     *(catch tag thunk handler)
     *
     *evaluates 'thunk'.  If an error occurs, and the type matches 
     *'tag'(or if 'tag' is #t), the handler is called, passing it the 
     *arguments (including the type) passed to the error function. 
     *If no handler is found, the default error handler is called,
     *normally printing the error arguments to current-error-port.
     */

    shack_pointer shack_stacktrace(shack_scheme *sc);
    /* the current (circular backwards) history buffer */
    shack_pointer shack_history(shack_scheme *sc);
    /* add entry to the history buffer */
    shack_pointer shack_add_to_history(shack_scheme *sc, shack_pointer entry);
    bool shack_history_enabled(shack_scheme *sc);
    bool shack_set_history_enabled(shack_scheme *sc, bool enabled);

    /* (gc on) */
    shack_pointer shack_gc_on(shack_scheme *sc, bool on);
    /* (set! (*shack* 'gc-stats) on) */
    void shack_set_gc_stats(shack_scheme *sc, bool on);

    shack_int shack_gc_protect(shack_scheme *sc, shack_pointer x);
    void shack_gc_unprotect_at(shack_scheme *sc, shack_int loc);
    shack_pointer shack_gc_protected_at(shack_scheme *sc, shack_int loc);
    shack_pointer shack_gc_protect_via_stack(shack_scheme *sc, shack_pointer x);
    shack_pointer shack_gc_unprotect_via_stack(shack_scheme *sc,
                                               shack_pointer x);
    shack_pointer shack_gc_protect_via_location(shack_scheme *sc,
                                                shack_pointer x,
                                                shack_int loc);
    shack_pointer shack_gc_unprotect_via_location(shack_scheme *sc,
                                                  shack_int loc);

    /**any shack_pointer object held in C (as a local variable for example)
     * needs to be protected from garbage collection if there is any chance
     * the GC may run without an existing Scheme-level reference to that object.
     * shack_gc_protect places the object in a vector that the GC always checks,
     * returning the object's location in that table.
     * shack_gc_unprotect_at unprotects the object (removes it from the vector)
     * using the location passed to it. shack_gc_protected_at returns the object
     * at the given location.
     * 
     * You can turn the GC on and off via shack_gc_on.
     * 
     * There is a built-in lag between the creation of a new object and its
     * first possible GC(the lag time is set indirectly by GC_TEMPS_SIZE in
     * shack.c), so you don't need to worry about very short term temps such as
     * the arguments to shack_cons in:
     * shack_cons(shack, shack_make_real(shack, 3.14),
     *            shack_cons(shack, shack_make_integer(shack, 123),
     *                shack_nil(shack)));
     */

    /* (eq? a b) */
    bool shack_is_eq(shack_pointer a, shack_pointer b);
    /* (eqv? a b) */
    bool shack_is_eqv(shack_pointer a, shack_pointer b);
    /* (equal? a b) */
    bool shack_is_equal(shack_scheme *sc, shack_pointer a, shack_pointer b);
    /* (equivalent? x y) */
    bool shack_is_equivalent(shack_scheme *sc, shack_pointer x, shack_pointer y);

    /* (boolean? x) */
    bool shack_is_boolean(shack_pointer x);

    /* Scheme boolean -> C bool */
    bool shack_boolean(shack_scheme *sc, shack_pointer x);

    /* C bool -> Scheme boolean */
    shack_pointer shack_make_boolean(shack_scheme *sc, bool x);

    /* for each Scheme type (boolean, integer, string, etc), there are three
     * functions: shack_<type>(...), shack_make_<type>(...),
     * and shack_is_<type>(...):
     *  
     * shack_boolean(shack, obj) returns the C bool corresponding to the value
     * of 'obj' (#f -> false)
     * shack_make_boolean(shack, false|true) returns the shack boolean
     * corresponding to the C bool argument (false -> #f)
     * shack_is_boolean(shack, obj) returns true if 'obj' has a boolean
     * value (#f or #t).
     */

    /* (pair? p) */
    bool shack_is_pair(shack_pointer p);

    /* (cons a b) */
    shack_pointer shack_cons(shack_scheme *sc, shack_pointer a, shack_pointer b);

    shack_pointer shack_car(shack_pointer p); /* (car p) */
    shack_pointer shack_cdr(shack_pointer p); /* (cdr p) */

    /* (set-car! p q) */
    shack_pointer shack_set_car(shack_pointer p, shack_pointer q);
    /* (set-cdr! p q) */
    shack_pointer shack_set_cdr(shack_pointer p, shack_pointer q);

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

    /* (list? p) -> (or (pair? p) (null? p)) */
    bool shack_is_list(shack_scheme *sc, shack_pointer p);
    /* (proper-list? p) */
    bool shack_is_proper_list(shack_scheme *sc, shack_pointer p);
    /* (length a) */
    shack_int shack_list_length(shack_scheme *sc, shack_pointer a);
    /* (list ...) */
    shack_pointer shack_list(shack_scheme *sc, shack_int num_values, ...);
    /* (list ...) arglist should be NULL terminated
       (more error checks than shack_list) */
    shack_pointer shack_list_nl(shack_scheme *sc, shack_int num_values, ...);
    /* (reverse a) */
    shack_pointer shack_reverse(shack_scheme *sc, shack_pointer a);

    /* (append a b) */
    shack_pointer shack_append(shack_scheme *sc, shack_pointer a,
                               shack_pointer b);
    /* (list-ref lst num) */
    shack_pointer shack_list_ref(shack_scheme *sc, shack_pointer lst,
                                 shack_int num);

    /* (list-set! lst num val) */
    shack_pointer shack_list_set(shack_scheme *sc, shack_pointer lst,
                                 shack_int num, shack_pointer val);

    /* (assoc obj lst) */
    shack_pointer shack_assoc(shack_scheme *sc, shack_pointer obj,
                              shack_pointer lst);

    /* (assq obj lst) */
    shack_pointer shack_assq(shack_scheme *sc, shack_pointer obj,
                             shack_pointer x);

    /* (member obj lst) */
    shack_pointer shack_member(shack_scheme *sc, shack_pointer obj,
                               shack_pointer lst);
    /* (memq obj lst) */
    shack_pointer shack_memq(shack_scheme *sc, shack_pointer obj,
                             shack_pointer x);
    /* (tree-memq sym tree) */
    bool shack_tree_memq(shack_scheme *sc, shack_pointer sym,
                         shack_pointer tree);

    /* (string? p) */
    bool shack_is_string(shack_pointer p);
    /* Scheme string -> C string (do not free the string) */
    const char *shack_string(shack_pointer p);
    /* C string -> Scheme string (str is copied) */
    shack_pointer shack_make_string(shack_scheme *sc, const char *str);
    /* same as shack_make_string, but provides strlen */
    shack_pointer shack_make_string_with_length(shack_scheme *sc,
                                                const char *str, shack_int len);
    shack_pointer shack_make_string_wrapper(shack_scheme *sc, const char *str);
    /* make a string that will never be GC'd */
    shack_pointer shack_make_permanent_string(shack_scheme *sc,
                                              const char *str);

    /* (string-length str) */
    shack_int shack_string_length(shack_pointer str);

    /* (character? p) */
    bool shack_is_character(shack_pointer p);
    /* Scheme character -> unsigned C char */
    uint8_t shack_character(shack_pointer p);
    /* unsigned C char -> Scheme character */
    shack_pointer shack_make_character(shack_scheme *sc, uint8_t c);

    /* (number? p) */
    bool shack_is_number(shack_pointer p);
    /* (integer? p) */
    bool shack_is_integer(shack_pointer p);
    /* Scheme integer -> C integer (shack_int) */
    shack_int shack_integer(shack_pointer p);
    /* C shack_int -> Scheme integer */
    shack_pointer shack_make_integer(shack_scheme *sc, shack_int num);

    /* (real? p) */
    bool shack_is_real(shack_pointer p);
    /* Scheme real -> C double */
    shack_double shack_real(shack_pointer p);
    /* C double -> Scheme real */
    shack_pointer shack_make_real(shack_scheme *sc, shack_double num);
    shack_pointer shack_make_mutable_real(shack_scheme *sc, shack_double n);
    /* x can be any kind of number */
    shack_double shack_number_to_real(shack_scheme *sc, shack_pointer x);
    shack_double shack_number_to_real_with_caller(shack_scheme *sc,
                                                  shack_pointer x,
                                                  const char *caller);
    shack_int shack_number_to_integer(shack_scheme *sc, shack_pointer x);
    shack_int shack_number_to_integer_with_caller(shack_scheme *sc,
                                                  shack_pointer x,
                                                  const char *caller);

    /* (rational? arg) -- integer or ratio */
    bool shack_is_rational(shack_pointer arg);
    /* true if arg is a ratio, not an integer */
    bool shack_is_ratio(shack_pointer arg);
    /* returns the Scheme object a/b */
    shack_pointer shack_make_ratio(shack_scheme *sc, shack_int a, shack_int b);
    /* (rationalize x error) */
    shack_pointer shack_rationalize(shack_scheme *sc, shack_double x,
                                    shack_double error);
    /* (numerator x) */
    shack_int shack_numerator(shack_pointer x);
    /* (denominator x) */
    shack_int shack_denominator(shack_pointer x);
    /* (random x) */
    shack_double shack_random(shack_scheme *sc, shack_pointer state);
    /* (random-state seed) */
    shack_pointer shack_random_state(shack_scheme *sc, shack_pointer seed);
    /* (random-state->list r) */
    shack_pointer shack_random_state_to_list(shack_scheme *sc,
                                             shack_pointer args);
    void shack_set_default_random_state(shack_scheme *sc, shack_int seed,
                                        shack_int carry);

    /* (complex? arg) */
    bool shack_is_complex(shack_pointer arg);
    /* returns the Scheme object a+bi */
    shack_pointer shack_make_complex(shack_scheme *sc, shack_double a,
                                     shack_double b);
    /* (real-part z) */
    shack_double shack_real_part(shack_pointer z);
    /* (imag-part z) */
    shack_double shack_imag_part(shack_pointer z);
    /* (number->string obj radix) */
    char *shack_number_to_string(shack_scheme *sc, shack_pointer obj,
                                 shack_int radix);

    /* (vector? p) */
    bool shack_is_vector(shack_pointer p);
    /* (vector-length vec) */
    shack_int shack_vector_length(shack_pointer vec);
    /* number of dimensions in vect */
    shack_int shack_vector_rank(shack_pointer vect);
    shack_int shack_vector_dimension(shack_pointer vec, shack_int dim);
    /* a pointer to the array of shack_pointers */
    shack_pointer *shack_vector_elements(shack_pointer vec);
    shack_int *shack_int_vector_elements(shack_pointer vec);
    shack_double *shack_float_vector_elements(shack_pointer vec);
    /* (float-vector? p) */
    bool shack_is_float_vector(shack_pointer p);
    /* (int-vector? p) */
    bool shack_is_int_vector(shack_pointer p);

    /* (vector-ref vec index) */
    shack_pointer shack_vector_ref(shack_scheme *sc, shack_pointer vec,
                                   shack_int index);
    /* (vector-set! vec index a) */
    shack_pointer shack_vector_set(shack_scheme *sc, shack_pointer vec,
                                   shack_int index, shack_pointer a);
    /* multidimensional vector-ref */
    shack_pointer shack_vector_ref_n(shack_scheme *sc, shack_pointer vector,
                                     shack_int indices, ...);
    /* multidimensional vector-set! */
    shack_pointer shack_vector_set_n(shack_scheme *sc,
                                     shack_pointer vector,
                                     shack_pointer value,
                                     shack_int indices, ...);
    /* vector dimensions */
    shack_int shack_vector_dimensions(shack_pointer vec, shack_int *dims,
                                      shack_int dims_size);
    shack_int shack_vector_offsets(shack_pointer vec, shack_int *offs,
                                   shack_int offs_size);

    /* (make-vector len) */
    shack_pointer shack_make_vector(shack_scheme *sc, shack_int len);
    shack_pointer shack_make_int_vector(shack_scheme *sc, shack_int len,
                                        shack_int dims, shack_int *dim_info);
    shack_pointer shack_make_float_vector(shack_scheme *sc, shack_int len,
                                          shack_int dims, shack_int *dim_info);
    shack_pointer shack_make_float_vector_wrapper(shack_scheme *sc,
                                                  shack_int len,
                                                  shack_double *data,
                                                  shack_int dims,
                                                  shack_int *dim_info,
                                                  bool free_data);

    /* (make-vector len fill) */
    shack_pointer shack_make_and_fill_vector(shack_scheme *sc, shack_int len,
                                             shack_pointer fill);

    /* (vector-fill! vec obj) */
    void shack_vector_fill(shack_scheme *sc, shack_pointer vec,
                           shack_pointer obj);
    shack_pointer shack_vector_copy(shack_scheme *sc, shack_pointer old_vect);

    /* (vector->list vec) */
    /*
     *  (vect i) is the same as (vector-ref vect i)
     *  (set! (vect i) x) is the same as (vector-set! vect i x)
     *  (vect i j k) accesses the 3-dimensional vect
     *  (set! (vect i j k) x) sets that element (vector-ref and vector-set!
     *      can also be used)
     *  (make-vector (list 2 3 4)) returns a 3-dimensional vector with the
     *      given dimension sizes
     *  (make-vector '(2 3) 1.0) returns a 2-dim vector with all elements
     *      set to 1.0
     */
    shack_pointer shack_vector_to_list(shack_scheme *sc, shack_pointer vect);

    /* (hash-table? p) */
    bool shack_is_hash_table(shack_pointer p);
    /* (make-hash-table size) */
    shack_pointer shack_make_hash_table(shack_scheme *sc, shack_int size);

    /* (hash-table-ref table key) */
    shack_pointer shack_hash_table_ref(shack_scheme *sc, shack_pointer table,
                                       shack_pointer key);

    /* (hash-table-set! table key value) */
    shack_pointer shack_hash_table_set(shack_scheme *sc, shack_pointer table,
                                       shack_pointer key, shack_pointer value);

    /* (hook-functions hook) */
    shack_pointer shack_hook_functions(shack_scheme *sc, shack_pointer hook);
    /* (set! (hook-functions hook) ...) */
    shack_pointer shack_hook_set_functions(shack_scheme *sc, shack_pointer hook,
                                           shack_pointer functions);

    /* (input-port? p) */
    bool shack_is_input_port(shack_scheme *sc, shack_pointer p);
    /* (output-port? p) */
    bool shack_is_output_port(shack_scheme *sc, shack_pointer p);
    /* (port-filename p) */
    const char *shack_port_filename(shack_scheme *sc, shack_pointer x);
    /* (port-line-number p) */
    shack_int shack_port_line_number(shack_scheme *sc, shack_pointer p);

    /* (current-input-port) */
    shack_pointer shack_current_input_port(shack_scheme *sc);
    /* (set-current-input-port) */
    shack_pointer shack_set_current_input_port(shack_scheme *sc,
                                               shack_pointer p);
    /* (current-output-port) */
    shack_pointer shack_current_output_port(shack_scheme *sc);
    /* (set-current-output-port) */
    shack_pointer shack_set_current_output_port(shack_scheme *sc,
                                                shack_pointer p);
    /* (current-error-port) */
    shack_pointer shack_current_error_port(shack_scheme *sc);
    /* (set-current-error-port port) */
    shack_pointer shack_set_current_error_port(shack_scheme *sc,
                                               shack_pointer port);
    /* (close-input-port p) */
    void shack_close_input_port(shack_scheme *sc, shack_pointer p);
    /* (close-output-port p) */
    void shack_close_output_port(shack_scheme *sc, shack_pointer p);

    /* (open-input-file name mode) */
    shack_pointer shack_open_input_file(shack_scheme *sc, const char *name,
                                        const char *mode);

    /* (open-output-file name mode) */
    /* mode here is an optional C style flag, "a" for "alter", 
       etc ("r" is the input default, "w" is the output default) */
    shack_pointer shack_open_output_file(shack_scheme *sc, const char *name,
                                         const char *mode);

    /* (open-input-string str) */
    shack_pointer shack_open_input_string(shack_scheme *sc,
                                          const char *input_string);

    /* (open-output-string) */
    shack_pointer shack_open_output_string(shack_scheme *sc);
    /* (get-output-string port) -- current contents of output string */
    /*    don't free the string */
    const char *shack_get_output_string(shack_scheme *sc,
                                        shack_pointer out_port);

    /* (flush-output-port port) */
    void shack_flush_output_port(shack_scheme *sc, shack_pointer p);

    typedef enum
    {
        SHACK_READ,
        SHACK_READ_CHAR,
        SHACK_READ_LINE,
        SHACK_READ_BYTE,
        SHACK_PEEK_CHAR,
        SHACK_IS_CHAR_READY
    } shack_read_t;
    shack_pointer shack_open_output_function(shack_scheme *sc,
                                             void (*function)(shack_scheme *sc,
                                                              uint8_t c,
                                                              shack_pointer port));
    shack_pointer shack_open_input_function(shack_scheme *sc,
                                            shack_pointer (*function)(shack_scheme *sc,
                                                                      shack_read_t read_choice,
                                                                      shack_pointer port));

    /* (read-char port) */
    shack_pointer shack_read_char(shack_scheme *sc, shack_pointer port);
    /* (peek-char port) */
    shack_pointer shack_peek_char(shack_scheme *sc, shack_pointer port);
    /* (read port) */
    shack_pointer shack_read(shack_scheme *sc, shack_pointer port);
    /* (newline port) */
    void shack_newline(shack_scheme *sc, shack_pointer port);
    /* (write-char c port) */
    shack_pointer shack_write_char(shack_scheme *sc, shack_pointer c,
                                   shack_pointer port);
    /* (write obj port) */
    shack_pointer shack_write(shack_scheme *sc, shack_pointer obj,
                              shack_pointer port);
    /* (display obj port) */
    shack_pointer shack_display(shack_scheme *sc, shack_pointer obj,
                                shack_pointer port);
    /* (format ... */
    const char *shack_format(shack_scheme *sc, shack_pointer args);

    /* (syntax? p) */
    bool shack_is_syntax(shack_pointer p);
    /* (symbol? p) */
    bool shack_is_symbol(shack_pointer p);
    /* (symbol->string p) -- don't free the string */
    const char *shack_symbol_name(shack_pointer p);
    /* (string->symbol name) */
    shack_pointer shack_make_symbol(shack_scheme *sc, const char *name);
    /* (gensym prefix) */
    shack_pointer shack_gensym(shack_scheme *sc, const char *prefix);

    /* (keyword? obj) */
    bool shack_is_keyword(shack_pointer obj);
    /* (string->keyword key) */
    shack_pointer shack_make_keyword(shack_scheme *sc, const char *key);
    /* (keyword->symbol key) */
    shack_pointer shack_keyword_to_symbol(shack_scheme *sc, shack_pointer key);

    shack_pointer shack_rootlet(shack_scheme *sc); /* (rootlet) */
    shack_pointer shack_shadow_rootlet(shack_scheme *sc);
    shack_pointer shack_set_shadow_rootlet(shack_scheme *sc, shack_pointer let);
    /* (curlet) */
    shack_pointer shack_curlet(shack_scheme *sc);
    /* returns previous curlet */
    shack_pointer shack_set_curlet(shack_scheme *sc, shack_pointer e);
    /* (outlet e) */
    shack_pointer shack_outlet(shack_scheme *sc, shack_pointer e);
    /* (sublet e ...) */
    shack_pointer shack_sublet(shack_scheme *sc, shack_pointer env,
                               shack_pointer bindings);
    /* (inlet ...) */
    shack_pointer shack_inlet(shack_scheme *sc, shack_pointer bindings);
    /* (varlet env symbol value) */
    shack_pointer shack_varlet(shack_scheme *sc, shack_pointer env,
                               shack_pointer symbol, shack_pointer value);
    /* (let->list env) */
    shack_pointer shack_let_to_list(shack_scheme *sc, shack_pointer env);
    /* )let? e) */
    bool shack_is_let(shack_pointer e);
    /* (let-ref e sym) */
    shack_pointer shack_let_ref(shack_scheme *sc, shack_pointer env,
                                shack_pointer sym);
    /* (let-set! e sym val) */
    shack_pointer shack_let_set(shack_scheme *sc, shack_pointer env,
                                shack_pointer sym, shack_pointer val);
    /* (openlet e) */
    shack_pointer shack_openlet(shack_scheme *sc, shack_pointer e);
    /* (openlet? e) */
    bool shack_is_openlet(shack_pointer e);
    shack_pointer shack_method(shack_scheme *sc, shack_pointer obj,
                               shack_pointer method);

    /* these access the current environment and symbol table, providing
     *   a symbol's current binding (shack_name_to_value takes the symbol name
     *   as a char*, shack_symbol_value takes the symbol itself, 
     *   shack_symbol_set_value changes the current binding, and 
     *   shack_symbol_local_value uses the environment passed
     *   as its third argument).
     *
     * To iterate over the complete symbol table,use shack_for_each_symbol_name,
     *   and shack_for_each_symbol.  Both call 'symbol_func' on each symbol, 
     *   passing it the symbol or symbol name, and the uninterpreted 'data' 
     *   pointer. the current binding. The for-each loop stops if the
     *   symbol_func returns true, or at the end of the table.
     */
    /*name's value in the current env (after turning name into a symbol) */
    shack_pointer shack_name_to_value(shack_scheme *sc, const char *name);
    shack_pointer shack_symbol_table_find_name(shack_scheme *sc,
                                               const char *name);
    shack_pointer shack_symbol_value(shack_scheme *sc, shack_pointer sym);
    shack_pointer shack_symbol_set_value(shack_scheme *sc, shack_pointer sym,
                                         shack_pointer val);
    shack_pointer shack_symbol_local_value(shack_scheme *sc, shack_pointer sym,
                                           shack_pointer local_env);
    bool shack_for_each_symbol_name(shack_scheme *sc,
                                    bool (*symbol_func)(const char *symbol_name,
                                                        void *data),
                                    void *data);
    bool shack_for_each_symbol(shack_scheme *sc,
                               bool (*symbol_func)(const char *symbol_name,
                                                   void *data),
                               void *data);

    /* These functions add a symbol and its binding to either the top-leve
     * environment or the 'env' passed as the second argument to shack_define.
     *
     * shack_define_variable(sc, "*features*", sc->NIL);
     *
     * in shack.c is equivalent to the top level form
     *
     *    (define *features* ())
     *
     * shack_define_variable is simply shack_define with string->symbol and the
     *     global environment.
     * shack_define_constant is shack_define but makes its "definee" immutable.
     * shack_define is equivalent to define in Scheme.
     */
    shack_pointer shack_dynamic_wind(shack_scheme *sc, shack_pointer init,
                                     shack_pointer body, shack_pointer finish);

    bool shack_is_immutable(shack_pointer p);
    shack_pointer shack_immutable(shack_pointer p);

    void shack_define(shack_scheme *sc, shack_pointer env, shack_pointer symbol,
                      shack_pointer value);
    bool shack_is_defined(shack_scheme *sc, const char *name);
    shack_pointer shack_define_variable(shack_scheme *sc, const char *name,
                                        shack_pointer value);
    shack_pointer shack_define_variable_with_documentation(shack_scheme *sc,
                                                           const char *name,
                                                           shack_pointer value,
                                                           const char *help);
    shack_pointer shack_define_constant(shack_scheme *sc,
                                        const char *name,
                                        shack_pointer value);
    shack_pointer shack_define_constant_with_documentation(shack_scheme *sc,
                                                           const char *name,
                                                           shack_pointer value,
                                                           const char *help);

    /** shack_make_function creates a Scheme function object from the
     *  shack_function 'fnc'.
     *  Its name (for shack_describe_object) is 'name', it requires 
     *  'required_args' arguments, can accept 'optional_args' other arguments, 
     *  and if 'rest_arg' is true, it accepts a "rest" argument (a list of all 
     *  the trailing arguments).  The function's documentation is 'doc'.
     *
     * shack_define_function is the same as shack_make_function, but it also
     * adds 'name' (as a symbol) to the global (top-level) environment, with the
     * function as its value.
     * For example, the Scheme function 'car' is essentially:
     *
     *     shack_pointer g_car(shack_scheme *sc, shack_pointer args)
     *       {return(shack_car(sc, shack_car(sc, args)));}
     *
     *   then bound to the name "car":
     *
     *     shack_define_function(sc, "car", g_car, 1, 0, false, "(car obj)");
     * 
     *   one required arg, no optional arg, no "rest" arg
     *
     * shack_is_function returns true if its argument is a function defined in
     * this manner.
     * shack_apply_function applies the function (the result of
     * shack_make_function) to the arguments.
     *
     * shack_define_macro defines a Scheme macro; its arguments are not
     * evaluated (unlike a function),  but its returned value (assumed to be
     * some sort of Scheme expression) is evaluated.
     *
     * Use the "unsafe" definer if the function might call the evaluator itself
     * in some way (shack_apply_function for example), or messes with shack's
     * stack.
     */

    /** In shack, (define* (name . args) body) or
     *  (define name (lambda* args body)) define a function that takes
     *  optional (keyword) named arguments.
     *  The "args" is a list that can contain either names (normal arguments),
     *  or lists of the form (name default-value), in any order.  When called,
     *  the names are bound to their default values (or #f), then the function's
     *  current arglist is scanned.  Any name that occurs as a keyword (":name")
     *  precedes that argument's new value.  Otherwise, as values occur, they
     *  are plugged into the environment based on their position in the arglist
     *  (as normal for a function).  So,
     *
     *   (define* (hi a (b 32) (c "hi")) (list a b c))
     *     (hi 1) -> '(1 32 "hi")
     *     (hi :b 2 :a 3) -> '(3 2 "hi")
     *     (hi 3 2 1) -> '(3 2 1)
     *
     *   :rest causes its argument to be bound to the rest of the arguments
     *        at that point.
     *
     * The C connection to this takes the function name, the C function to call,
     * the argument list as written in Scheme, and the documentation string. 
     * shack makes sure the arguments are ordered correctly and have the 
     * specified defaults before calling the C function.
     * shack_define_function_star(sc, "a-func", a_func, "arg1 (arg2 32)",
     *                            "an example of C define*");
     * Now (a-func :arg1 2) calls the C function a_func(2, 32).
     * See the example program in shack.html.
     *
     * In shack Scheme, define* can be used just for its optional arguments
     * feature, but that is included in shack_define_function. 
     * shack_define_function_star implements keyword arguments
     * for C-level functions (as well as optional/rest arguments).
     */
    bool shack_is_function(shack_pointer p);
    /* (procedure? x) */
    bool shack_is_procedure(shack_pointer x);
    /* (macro? x) */
    bool shack_is_macro(shack_scheme *sc, shack_pointer x);
    shack_pointer shack_closure_body(shack_scheme *sc, shack_pointer p);
    shack_pointer shack_closure_let(shack_scheme *sc, shack_pointer p);
    shack_pointer shack_closure_args(shack_scheme *sc, shack_pointer p);
    /* (funclet x) */
    shack_pointer shack_funclet(shack_scheme *sc, shack_pointer p);
    /* (aritable? x args) */
    bool shack_is_aritable(shack_scheme *sc, shack_pointer x, shack_int args);
    /* (arity x) */
    shack_pointer shack_arity(shack_scheme *sc, shack_pointer x);
    /* (help obj) */
    const char *shack_help(shack_scheme *sc, shack_pointer obj);
    /* call/cc... (see example below) */
    shack_pointer shack_make_continuation(shack_scheme *sc);

    /* (documentation x) if any (don't free the string) */
    const char *shack_documentation(shack_scheme *sc, shack_pointer p);
    const char *shack_set_documentation(shack_scheme *sc, shack_pointer p,
                                        const char *new_doc);
    /* (setter obj) */
    shack_pointer shack_setter(shack_scheme *sc, shack_pointer obj);
    /* (set! (setter p) setter) */
    shack_pointer shack_set_setter(shack_scheme *sc, shack_pointer p,
                                   shack_pointer setter);
    /* (signature obj) */
    shack_pointer shack_signature(shack_scheme *sc, shack_pointer func);
    /* procedure-signature data */
    shack_pointer shack_make_signature(shack_scheme *sc, shack_int len, ...);
    shack_pointer shack_make_circular_signature(shack_scheme *sc,
                                                shack_int cycle_point,
                                                shack_int len, ...);

    shack_pointer shack_make_function(shack_scheme *sc, const char *name,
                                      shack_function fnc,
                                      shack_int required_args,
                                      shack_int optional_args, bool rest_arg,
                                      const char *doc);
    shack_pointer shack_make_safe_function(shack_scheme *sc, const char *name,
                                           shack_function fnc,
                                           shack_int required_args,
                                           shack_int optional_args,
                                           bool rest_arg,
                                           const char *doc);
    shack_pointer shack_make_typed_function(shack_scheme *sc,
                                            const char *name,
                                            shack_function f,
                                            shack_int required_args,
                                            shack_int optional_args,
                                            bool rest_arg,
                                            const char *doc,
                                            shack_pointer signature);

    shack_pointer shack_define_function(shack_scheme *sc,
                                        const char *name,
                                        shack_function fnc,
                                        shack_int required_args,
                                        shack_int optional_args,
                                        bool rest_arg,
                                        const char *doc);
    shack_pointer shack_define_safe_function(shack_scheme *sc,
                                             const char *name,
                                             shack_function fnc,
                                             shack_int required_args,
                                             shack_int optional_args,
                                             bool rest_arg,
                                             const char *doc);
    shack_pointer shack_define_typed_function(shack_scheme *sc,
                                              const char *name,
                                              shack_function fnc,
                                              shack_int required_args,
                                              shack_int optional_args,
                                              bool rest_arg,
                                              const char *doc,
                                              shack_pointer signature);
    shack_pointer shack_define_unsafe_typed_function(shack_scheme *sc,
                                                     const char *name,
                                                     shack_function fnc,
                                                     shack_int required_args,
                                                     shack_int optional_args,
                                                     bool rest_arg,
                                                     const char *doc,
                                                     shack_pointer signature);

    shack_pointer shack_make_function_star(shack_scheme *sc, const char *name,
                                           shack_function fnc,
                                           const char *arglist,
                                           const char *doc);
    shack_pointer shack_make_safe_function_star(shack_scheme *sc,
                                                const char *name,
                                                shack_function fnc,
                                                const char *arglist,
                                                const char *doc);
    void shack_define_function_star(shack_scheme *sc, const char *name,
                                    shack_function fnc, const char *arglist,
                                    const char *doc);
    void shack_define_safe_function_star(shack_scheme *sc,
                                         const char *name,
                                         shack_function fnc,
                                         const char *arglist,
                                         const char *doc);
    void shack_define_typed_function_star(shack_scheme *sc,
                                          const char *name,
                                          shack_function fnc,
                                          const char *arglist,
                                          const char *doc,
                                          shack_pointer signature);
    shack_pointer shack_define_macro(shack_scheme *sc, const char *name,
                                     shack_function fnc,
                                     shack_int required_args,
                                     shack_int optional_args, bool rest_arg,
                                     const char *doc);

    /** shack_call takes a Scheme function (e.g. g_car above), and applies it to
     * 'args' (a list of arguments) returning the result.
     *  shack_integer(shack_call(shack, g_car, shack_cons(shack, 
     *                shack_make_integer(shack, 123), shack_nil(shack))));
     *  
     *  returns 123.
     *
     * shack_call_with_location passes some information to the error handler.
     * shack_call makes sure some sort of catch exists if an error occurs during
     * the call, but shack_apply_function does not -- it assumes the catch has
     * been set up already.
     * shack_call_with_catch wraps an explicit catch around a function call
     * ("body" above); shack_call_with_catch(sc, tag, body, err) is equivalent
     * to (catch tag body err).
     */
    shack_pointer shack_apply_function(shack_scheme *sc, shack_pointer fnc,
                                       shack_pointer args);
    shack_pointer shack_apply_function_star(shack_scheme *sc, shack_pointer fnc,
                                            shack_pointer args);

    shack_pointer shack_call(shack_scheme *sc, shack_pointer func,
                             shack_pointer args);
    shack_pointer shack_call_with_location(shack_scheme *sc, shack_pointer func,
                                           shack_pointer args,
                                           const char *caller,
                                           const char *file,
                                           shack_int line);
    shack_pointer shack_call_with_catch(shack_scheme *sc, shack_pointer tag,
                                        shack_pointer body,
                                        shack_pointer error_handler);

    bool shack_is_dilambda(shack_pointer obj);
    shack_pointer shack_dilambda(shack_scheme *sc,
                                 const char *name,
                                 shack_pointer (*getter)(shack_scheme *sc,
                                                         shack_pointer args),
                                 shack_int get_req_args, shack_int get_opt_args,
                                 shack_pointer (*setter)(shack_scheme *sc,
                                                         shack_pointer args),
                                 shack_int set_req_args, shack_int set_opt_args,
                                 const char *documentation);
    shack_pointer shack_typed_dilambda(shack_scheme *sc,
                                       const char *name,
                                       shack_pointer (*getter)(shack_scheme *sc,
                                                               shack_pointer args),
                                       shack_int get_req_args,
                                       shack_int get_opt_args,
                                       shack_pointer (*setter)(shack_scheme *sc,
                                                               shack_pointer args),
                                       shack_int set_req_args,
                                       shack_int set_opt_args,
                                       const char *documentation,
                                       shack_pointer get_sig,
                                       shack_pointer set_sig);

    /* (values ...) */
    shack_pointer shack_values(shack_scheme *sc, shack_pointer args);
    /* (make-iterator e) */
    shack_pointer shack_make_iterator(shack_scheme *sc, shack_pointer e);
    /* (iterator? obj) */
    bool shack_is_iterator(shack_pointer obj);
    /* (iterator-at-end? obj) */
    bool shack_iterator_is_at_end(shack_scheme *sc, shack_pointer obj);
    /* (iterate iter) */
    shack_pointer shack_iterate(shack_scheme *sc, shack_pointer iter);

    void shack_autoload_set_names(shack_scheme *sc, const char **names,
                                  shack_int size);

    /* (copy ...) */
    shack_pointer shack_copy(shack_scheme *sc, shack_pointer args);
    /* (fill! ...) */
    shack_pointer shack_fill(shack_scheme *sc, shack_pointer args);
    /* (type-of arg) */
    shack_pointer shack_type_of(shack_scheme *sc, shack_pointer arg);

    /* value of (*shack* 'print-length) */
    shack_int shack_print_length(shack_scheme *sc);
    /* sets (*shack* 'print-length), returns old value */
    shack_int shack_set_print_length(shack_scheme *sc, shack_int new_len);
    /* value of (*shack* 'float-format-precision) */
    shack_int shack_float_format_precision(shack_scheme *sc);
    /* sets (*shack* 'float-format-precision), returns old value */
    shack_int shack_set_float_format_precision(shack_scheme *sc,
                                               shack_int new_len);

    /* --------------------------------------------------------------- */
    /* c types/objects */

    void shack_mark(shack_pointer p);

    bool shack_is_c_object(shack_pointer p);
    shack_int shack_c_object_type(shack_pointer obj);
    void *shack_c_object_value(shack_pointer obj);
    void *shack_c_object_value_checked(shack_pointer obj, shack_int type);
    shack_pointer shack_make_c_object(shack_scheme *sc, shack_int type,
                                      void *value);
    shack_pointer shack_make_c_object_with_let(shack_scheme *sc, shack_int type,
                                               void *value, shack_pointer let);
    shack_pointer shack_c_object_let(shack_pointer obj);
    shack_pointer shack_c_object_set_let(shack_scheme *sc, shack_pointer obj,
                                         shack_pointer e);

    shack_int shack_make_c_type(shack_scheme *sc, const char *name);
    void shack_c_type_set_free(shack_scheme *sc, shack_int tag,
                               void (*gc_free)(void *value));
    void shack_c_type_set_equal(shack_scheme *sc, shack_int tag,
                                bool (*equal)(void *value1,
                                              void *value2));
    void shack_c_type_set_mark(shack_scheme *sc, shack_int tag,
                               void (*mark)(void *value));
    void shack_c_type_set_ref(shack_scheme *sc, shack_int tag,
                              shack_pointer (*ref)(shack_scheme *sc,
                                                   shack_pointer args));
    void shack_c_type_set_set(shack_scheme *sc, shack_int tag,
                              shack_pointer (*set)(shack_scheme *sc,
                                                   shack_pointer args));
    void shack_c_type_set_length(shack_scheme *sc, shack_int tag,
                                 shack_pointer (*length)(shack_scheme *sc,
                                                         shack_pointer args));
    void shack_c_type_set_copy(shack_scheme *sc, shack_int tag,
                               shack_pointer (*copy)(shack_scheme *sc,
                                                     shack_pointer args));
    void shack_c_type_set_fill(shack_scheme *sc, shack_int tag,
                               shack_pointer (*fill)(shack_scheme *sc,
                                                     shack_pointer args));
    void shack_c_type_set_reverse(shack_scheme *sc, shack_int tag,
                                  shack_pointer (*reverse)(shack_scheme *sc,
                                                           shack_pointer args));
    void shack_c_type_set_to_list(shack_scheme *sc, shack_int tag,
                                  shack_pointer (*to_list)(shack_scheme *sc,
                                                           shack_pointer args));
    void shack_c_type_set_to_string(shack_scheme *sc, shack_int tag,
                                    shack_pointer (*to_string)(shack_scheme *sc,
                                                               shack_pointer args));
    void shack_c_type_set_getter(shack_scheme *sc, shack_int tag,
                                 shack_pointer getter);
    void shack_c_type_set_setter(shack_scheme *sc, shack_int tag,
                                 shack_pointer setter);
    /** if a c-object might participate in a cyclic structure, and you want to
     *  check its equality to another such object or get a readable string
     *  representing that object, you need to implement the "to_list" and "set"
     *  cases above, and make the type name a function that can recreate the
     *  object.  See the <cycle> object in shacktest.scm. For the copy function,
     *  either the first or second argument can be a c-object of the given type.
     */

    /** These functions create a new Scheme object type.
     *  There is a simple example in shack.html.
     *
     * shack_make_c_type creates a new C-based type for Scheme:
     * free:    the function called when an object of this type is about to be
     *          garbage collected
     * mark:    called during the GC mark pass -- you should call shack_mark
     *          on any embedded shack_pointer associated with the object to 
     *          protect if from the GC.
     * equal:   compare two objects of this type; (equal? obj1 obj2)
     * ref:     a function that is called whenever an object of this type
     *          occurs in the function position (at the car of a list; the 
     *          rest of the list is passed to the ref function as the
     *          arguments: (obj ...))
     * set:     a function that is called whenever an object of this type
     *          occurs as the target of a generalized set! (set! (obj ...) val)
     * length:  the function called when the object is asked what its length is.
     * copy:    the function called when a copy of the object is needed.
     * fill:    the function called to fill the object with some value.
     * reverse  : similarly...
     * to_string: object->string for an object of this type
     * getter/setter: these help the optimizer handle applicable c-objects 
     *                (see shacktest.scm for an example)
     * shack_is_c_object returns true if 'p' is a c_object
     * shack_c_object_type returns the c_object's type
     * shack_c_object_value returns the value bound to that c_object
     *                             (the void *value of shack_make_c_object)
     * shack_make_c_object creates a new Scheme entity of the given type with 
     *     the given (uninterpreted) value
     * shack_mark marks any Scheme c_object as in-use (use this in the mark
     * function to mark any embedded shack_pointer variables).
     */

    /* ---------------------------------------------------------------------- */
    /** the new clm optimizer!  this time for sure!
     * d=double, i=integer, v=c_object, p=shack_pointer
     * first return type, then arg types, d_vd -> returns double takes c_object
     * and double (i.e. a standard clm generator)
     */

    /**It is possible to tell shack to call a foreign function directly,
     * without any scheme-related overhead.
     * The call needs to take the form of one of the shack_*_t functions
     * in shack.h.
     * For example, one way to call + is to pass it two shack_double arguments
     * and get an shack_double back.  This is the shack_d_dd_t function (the
     * first letter gives the return type, the rest give successive argument
     * types). We tell shack about it via shack_set_d_dd_function.
     * Whenever shack's optimizer encounters + with two arguments that it (the
     * optimizer) knows are shack_doubles, in a context where an shack_double
     * result is expected, shack calls the shack_d_dd_t function directly
     * without consing a list of arguments, and without wrapping up the result
     * as a scheme cell.
     */

    shack_function shack_optimize(shack_scheme *sc, shack_pointer expr);

    typedef shack_double (*shack_float_function)(shack_scheme *sc,
                                                 shack_pointer args);
    shack_float_function shack_float_optimize(shack_scheme *sc,
                                              shack_pointer expr);

    typedef shack_double (*shack_d_t)(void);
    void shack_set_d_function(shack_pointer f, shack_d_t df);
    shack_d_t shack_d_function(shack_pointer f);

    typedef shack_double (*shack_d_d_t)(shack_double x);
    void shack_set_d_d_function(shack_pointer f, shack_d_d_t df);
    shack_d_d_t shack_d_d_function(shack_pointer f);

    typedef shack_double (*shack_d_dd_t)(shack_double x1, shack_double x2);
    void shack_set_d_dd_function(shack_pointer f, shack_d_dd_t df);
    shack_d_dd_t shack_d_dd_function(shack_pointer f);

    typedef shack_double (*shack_d_ddd_t)(shack_double x1, shack_double x2,
                                          shack_double x3);
    void shack_set_d_ddd_function(shack_pointer f, shack_d_ddd_t df);
    shack_d_ddd_t shack_d_ddd_function(shack_pointer f);

    typedef shack_double (*shack_d_dddd_t)(shack_double x1, shack_double x2,
                                           shack_double x3, shack_double x4);
    void shack_set_d_dddd_function(shack_pointer f, shack_d_dddd_t df);
    shack_d_dddd_t shack_d_dddd_function(shack_pointer f);

    typedef shack_double (*shack_d_v_t)(void *v);
    void shack_set_d_v_function(shack_pointer f, shack_d_v_t df);
    shack_d_v_t shack_d_v_function(shack_pointer f);

    typedef shack_double (*shack_d_vd_t)(void *v, shack_double d);
    void shack_set_d_vd_function(shack_pointer f, shack_d_vd_t df);
    shack_d_vd_t shack_d_vd_function(shack_pointer f);

    typedef shack_double (*shack_d_vdd_t)(void *v, shack_double x1,
                                          shack_double x2);
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

    typedef shack_double (*shack_d_7pi_t)(shack_scheme *sc, shack_pointer v,
                                          shack_int i);
    void shack_set_d_7pi_function(shack_pointer f, shack_d_7pi_t df);
    shack_d_7pi_t shack_d_7pi_function(shack_pointer f);

    typedef shack_double (*shack_d_7pid_t)(shack_scheme *sc, shack_pointer v,
                                           shack_int i, shack_double d);
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

    /** Here is an example of using these functions;
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

    /* ---------------------------------------------------------------------- */

    /* maybe remove these? */
    shack_pointer shack_slot(shack_scheme *sc, shack_pointer symbol);
    shack_pointer shack_slot_value(shack_pointer slot);
    shack_pointer shack_slot_set_value(shack_scheme *sc, shack_pointer slot,
                                       shack_pointer value);
    shack_pointer shack_make_slot(shack_scheme *sc, shack_pointer env,
                                  shack_pointer symbol, shack_pointer value);
    void shack_slot_set_real_value(shack_scheme *sc, shack_pointer slot,
                                   shack_double value);

    /* ---------------------------------------------------------------------- */

    /* these will be deprecated and removed eventually */
    shack_pointer shack_apply_1(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f1)(shack_pointer a1));
    shack_pointer shack_apply_2(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f2)(shack_pointer a1,
                                                    shack_pointer a2));
    shack_pointer shack_apply_3(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f3)(shack_pointer a1,
                                                    shack_pointer a2,
                                                    shack_pointer a3));
    shack_pointer shack_apply_4(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f4)(shack_pointer a1,
                                                    shack_pointer a2,
                                                    shack_pointer a3,
                                                    shack_pointer a4));
    shack_pointer shack_apply_5(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f5)(shack_pointer a1,
                                                    shack_pointer a2,
                                                    shack_pointer a3,
                                                    shack_pointer a4,
                                                    shack_pointer a5));
    shack_pointer shack_apply_6(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f6)(shack_pointer a1,
                                                    shack_pointer a2,
                                                    shack_pointer a3,
                                                    shack_pointer a4,
                                                    shack_pointer a5,
                                                    shack_pointer a6));
    shack_pointer shack_apply_7(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f7)(shack_pointer a1,
                                                    shack_pointer a2,
                                                    shack_pointer a3,
                                                    shack_pointer a4,
                                                    shack_pointer a5,
                                                    shack_pointer a6,
                                                    shack_pointer a7));
    shack_pointer shack_apply_8(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f8)(shack_pointer a1,
                                                    shack_pointer a2,
                                                    shack_pointer a3,
                                                    shack_pointer a4,
                                                    shack_pointer a5,
                                                    shack_pointer a6,
                                                    shack_pointer a7,
                                                    shack_pointer a8));
    shack_pointer shack_apply_9(shack_scheme *sc, shack_pointer args,
                                shack_pointer (*f9)(shack_pointer a1,
                                                    shack_pointer a2,
                                                    shack_pointer a3,
                                                    shack_pointer a4,
                                                    shack_pointer a5,
                                                    shack_pointer a6,
                                                    shack_pointer a7,
                                                    shack_pointer a8,
                                                    shack_pointer a9));

    shack_pointer shack_apply_n_1(shack_scheme *sc, shack_pointer args,
                                  shack_pointer (*f1)(shack_pointer a1));
    shack_pointer shack_apply_n_2(shack_scheme *sc, shack_pointer args,
                                  shack_pointer (*f2)(shack_pointer a1,
                                                      shack_pointer a2));
    shack_pointer shack_apply_n_3(shack_scheme *sc, shack_pointer args,
                                  shack_pointer (*f3)(shack_pointer a1,
                                                      shack_pointer a2,
                                                      shack_pointer a3));
    shack_pointer shack_apply_n_4(shack_scheme *sc, shack_pointer args,
                                  shack_pointer (*f4)(shack_pointer a1,
                                                      shack_pointer a2,
                                                      shack_pointer a3,
                                                      shack_pointer a4));
    shack_pointer shack_apply_n_5(shack_scheme *sc, shack_pointer args,
                                  shack_pointer (*f5)(shack_pointer a1,
                                                      shack_pointer a2,
                                                      shack_pointer a3,
                                                      shack_pointer a4,
                                                      shack_pointer a5));
    shack_pointer shack_apply_n_6(shack_scheme *sc, shack_pointer args,
                                  shack_pointer (*f6)(shack_pointer a1,
                                                      shack_pointer a2,
                                                      shack_pointer a3,
                                                      shack_pointer a4,
                                                      shack_pointer a5,
                                                      shack_pointer a6));
    shack_pointer shack_apply_n_7(shack_scheme *sc, shack_pointer args,
                                  shack_pointer (*f7)(shack_pointer a1,
                                                      shack_pointer a2,
                                                      shack_pointer a3,
                                                      shack_pointer a4,
                                                      shack_pointer a5,
                                                      shack_pointer a6,
                                                      shack_pointer a7));
    shack_pointer shack_apply_n_8(shack_scheme *sc, shack_pointer args,
                                  shack_pointer (*f8)(shack_pointer a1,
                                                      shack_pointer a2,
                                                      shack_pointer a3,
                                                      shack_pointer a4,
                                                      shack_pointer a5,
                                                      shack_pointer a6,
                                                      shack_pointer a7,
                                                      shack_pointer a8));
    shack_pointer shack_apply_n_9(shack_scheme *sc, shack_pointer args,
                                  shack_pointer (*f9)(shack_pointer a1,
                                                      shack_pointer a2,
                                                      shack_pointer a3,
                                                      shack_pointer a4,
                                                      shack_pointer a5,
                                                      shack_pointer a6,
                                                      shack_pointer a7,
                                                      shack_pointer a8,
                                                      shack_pointer a9));

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
#define shack_new_type(Name, Print, GC_Free, Equal, Mark, Ref, Set) \
    shack_new_type_1(shack, Name, Print, GC_Free, Equal, Mark, Ref, Set)
#endif

#ifdef __cplusplus
}
#endif

#endif //__SHACK_H__
