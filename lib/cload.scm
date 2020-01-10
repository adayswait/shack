(provide 'cload.scm)

;;; --------------------------------------------------------------------------------
;;; automatically link a C function into shack (there are a bunch of examples below)
;;;     (c-define '(double j0 (double)) "m" "math.h")
;;; means link the name m:j0 to the math library function j0 passing a double arg and getting a double result (reals in shack)
;;;
;;; (c-define c-info prefix headers cflags ldflags)
;;;    prefix is some arbitrary prefix (it can be "") that you want prepended to various names.
;;;    headers is a list of headers (as strings) that the c-info relies on, (("math.h") for example).
;;;    cflags are any special C compiler flags that are needed ("-I." in particular).
;;;    ldflags is the similar case for the loader.
;;;    c-info is a list that describes the C entities that you want to tie into shack.
;;;       it can be either one list describing one entity, or a list of such lists.
;;;       Each description has the form: (return-type entity-name-in-C (argument-type...))
;;;       where each entry is a symbol, and C names are used throughout.  So, in the j0
;;;       example above, (double j0 (double)) says we want access to j0, it returns
;;;       a C double, and takes one argument, also a C double.  shack tries to figure out 
;;;       what the corresponding shack type is, but in tricky cases, you should tell it
;;;       by replacing the bare type name with a list: (C-type underlying-C-type).  For example,
;;;       the function set_graph_style takes an (enum) argument of type graph_style_t.
;;;       This is actually an int, so we use (graph_style_t int) as the type:
;;;         (void set_graph_style ((graph_style_t int)))
;;;       If the C entity is a constant, then the descriptor list has just two entries,
;;;       the C-type and the entity name: (int F_OK) for example. The entity name can also be a list 
;;;       (an enum listing for example).
;;;       If the C type has a space ("struct tm*" for example), use (symbol "struct tm*") 
;;;       to construct the corresponding symbol.
;;;    The entity is placed in the current shack environment under the name (string-append prefix ":" name)
;;;    where the ":" is omitted if the prefix is null.  So in the j0 example, we get in shack the function m:j0.
;;;
;;; some examples:
;;;
;;;  (c-define '((double j0 (double)) 
;;;              (double j1 (double)) 
;;;              (double erf (double)) 
;;;              (double erfc (double))
;;;              (double lgamma (double)))
;;;             "m" "math.h")
;;; 
;;;
;;; (c-define '(char* getenv (char*)))
;;; (c-define '(int setenv (char* char* int)))
;;; (define get-environment-variable (let () (c-define '(char* getenv (char*))) getenv))
;;;
;;; (define file-exists? (let () (c-define '((int F_OK) (int access (char* int))) "" "unistd.h") (lambda (arg) (= (access arg F_OK) 0))))
;;; (define delete-file (let () (c-define '(int unlink (char*)) "" "unistd.h") (lambda (file) (= (unlink file) 0)))) ; 0=success, -1=failure
;;;
;;;
;;;   
;;;
;;;  (c-define '(char* getcwd (char* size_t)) "" "unistd.h")
;;;    :(let ((str (make-string 32))) (getcwd str 32) str)
;;;    "/home/bil/cl\x00                   "
;;;    so it works in a sense -- there is a memory leak here
;;; 
;;;
;;; (c-define (list '(void* calloc (size_t size_t))
;;;		    '(void* malloc (size_t))
;;;		    '(void free (void*))
;;;		    '(void* realloc(void* size_t))
;;;		    '(void time (time_t*)) ; ignore returned value
;;;		    (list (symbol "struct tm*") 'localtime '(time_t*))
;;;                 (list 'size_t 'strftime (list 'char* 'size_t 'char* (symbol "struct tm*"))))
;;;          "" "time.h")
;;;   > (let ((p (calloc 1 8)) (str (make-string 32))) (time p) (strftime str 32 "%a %d-%b-%Y %H:%M %Z" (localtime p)) (free p) str)
;;;   "Sat 11-Aug-2012 08:55 PDT\x00      "
;;;
;;;
;;; (c-define '((int closedir (DIR*))
;;; 	        (DIR* opendir (char*))
;;; 		(in-C "static char *read_dir(DIR *p) \
;;;                    {                             \
;;;                      struct dirent *dirp;         \
;;;                      dirp = readdir(p);           \
;;;                      if (!dirp) return(NULL);     \
;;;                      else return(dirp->d_name);   \
;;;                     }")
;;; 	        (char* read_dir (DIR*)))
;;;   "" '("sys/types.h" "dirent.h"))
;;; 
;;;
;;; (define (memory-usage)
;;;   (with-let *libc*
;;;     (let ((v (rusage.make))) 
;;;       (getrusage RUSAGE_SELF v)
;;;       (let ((mem (rusage.ru_maxrss v))) 
;;;         (free v) 
;;;         (* 1024 mem)))))
;;; --------------------------------------------------------------------------------

(define *cload-cflags* (if (provided? 'clang) "-fPIC" ""))
(define *cload-ldflags* "")
(if (not (defined? '*cload-directory*))
    (define *cload-directory* ""))

(define *cload-c-compiler* (if (provided? 'gcc) "gcc" (if (provided? 'clang) "clang" "cc")))


(define-macro (defvar name value) 
  `(if (not (defined? ',name)) 
       (define ,name ,value)))

(defvar c-define-output-file-counter 0)   ; ugly, but I can't find a way around this (dlopen/dlsym stupidity)


;;; to place the new function in the caller's current environment, we need to pass the environment in explicitly:
(define-macro (c-define . args) 
  (cons 'c-define-1 (cons '(curlet) args)))


(define* (c-define-1 cur-env function-info (prefix "") (headers ()) (cflags "") (ldflags "") output-name)
  ;; write a C shared library module that links in the functions in function-info
  ;;    function info is either a list: (return-type c-name arg-type) or a list thereof
  ;;    the new functions are placed in cur-env

  (define handlers (list '(integer shack_is_integer shack_integer shack_make_integer shack_int)
			 '(boolean shack_is_boolean shack_boolean shack_make_boolean bool)
			 '(real shack_is_real shack_number_to_real shack_make_real shack_double)

			 ;; '(complex shack_is_complex #f shack_make_complex shack_Complex)
			 ;; the typedef is around line 6116 in shack.c, but we also need shack_complex which requires the shack_Complex type
			 ;; xen.h uses (shack_real_part(a) + shack_imag_part(a) * _Complex_I) instead since c++ won't let use define shack_Complex in shack.h

			 '(string shack_is_string shack_string shack_make_string char*)
			 (list 'character 'shack_is_character 'shack_character 'shack_make_character (symbol "unsigned char"))
			 '(c_pointer shack_is_c_pointer shack_c_pointer shack_make_c_pointer_with_type void*)
			 '(shack_pointer #f #f #f shack_pointer)
			 ))

  (define (C-type->shack-type type)

    (if (pair? type)                             ; in case the type name does not make its C type obvious: (graph_style_t int)
	(C-type->shack-type (cadr type))
	(let ((type-name (symbol->string type)))
	  (cond ((string-position "**" type-name)     ; any complicated C pointer is uninterpreted
		 'c_pointer)

		((string=? "shack_pointer" type-name)
		 'shack_pointer)
		
		((string-position "char*" type-name)  ; but not char** (caught above)
		 'string)

		((or (string-position "*" type-name)  ; float* etc
		     (string-position "pointer" type-name))
		 'c_pointer)

		((assoc type-name '(("char" . character) 
				    ("bool" . boolean)) string-position)
		 => cdr)
		
		;; ((string-position "complex" type-name)
		;;  'complex)                              ; double complex or complex double (mus_edot_product in clm.c uses the latter)

		((or (string-position "float" type-name) 
		     (string-position "double" type-name)) 
		 'real)

		((or (string-position "int" type-name) 
		     (string-position "long" type-name) ; assuming not "long double" here so we need to look for it first (above)
		     (string-position "short" type-name) 
		     (string-position "size" type-name)
		     (string-position "byte" type-name)) 
		 'integer)

		(#t #t)))))

  (define (find-handler type choice)
    (cond ((assq (C-type->shack-type type) handlers) => choice) (else #t)))

  (define (C->shack-cast type)
    (find-handler type (lambda (p) (list-ref p 4))))
    
  (define (C->shack type)
    (find-handler type cadddr))
    
  (define (shack->C type)
    (find-handler type caddr))

  (define (checker type)
    (find-handler type cadr))

  (define (signature->pl type)
    (case type
      ((integer?)   #\i)
      ((boolean?)   #\b)
      ((real?)      #\r)
      ((float?)     #\d)
      ((char?)      #\c)
      ((string?)    #\s)
      ((c-pointer?) #\x)
      (else         #\t)))

  (set! c-define-output-file-counter (+ c-define-output-file-counter 1))

  (let ((file-name (string-append *cload-directory* 
				  (if (and (> (length *cload-directory*) 0)
					   (not (char=? (string-ref *cload-directory* (- (length *cload-directory*) 1)) #\/)))
				      "/" "")
				  (or output-name (format #f "temp-shack-output-~D" c-define-output-file-counter)))))
    (let ((c-file-name (string-append file-name ".c"))
	  (o-file-name (string-append file-name ".o"))
	  (so-file-name (string-append file-name ".so"))
	  (init-name (if (string? output-name)
			 (string-append output-name "_init")
			 (string-append "init_" (number->string c-define-output-file-counter))))
	  (functions ())
	  (constants ())
	  (macros ())     ; these are protected by #ifdef ... #endif
	  (inits ())      ; C code (a string in shack) inserted in the library initialization function
	  (p #f)
	  (int-funcs ())  ; functions guaranteed to return int
	  (double-funcs ())  ; functions returning double
	  (sig-symbols (list (cons 'integer? 0) (cons 'boolean? 0) (cons 'real?  0) (cons 'float? 0) 
			     (cons 'char? 0) (cons 'string? 0) (cons 'c-pointer? 0) (cons 't 0)))
	  (signatures (make-hash-table)))
      
      (define make-signature 
	(let ((cload->signature 
	       (lambda* (type rtn)
		 (case (C-type->shack-type type)
		   ((real)      (if rtn 'float? 'real?))
		   ((integer)   'integer?)
		   ((string)    'string?)
		   ((boolean)   'boolean?)
		   ((character) 'char?)
		   ((c_pointer) 'c-pointer?)
		   (else #t)))))
	  (lambda (rtn args)
	    (let ((sig (list (cload->signature rtn #t)))
		  (cyclic #f))
	      (for-each
	       (lambda (arg)
		 (set! sig (cons (cload->signature arg) sig)))
	       args)
	      (let ((len (length sig)))
		(set! sig (do ((sig sig (cdr sig)))
			      ((not (and (pair? sig) 
					 (pair? (cdr sig))
					 (eq? (car sig) (cadr sig))))
			       sig)))
		(set! cyclic (not (= len (length sig)))))
	      (set! sig (cons cyclic (reverse sig)))      ; need to include cyclic in key else trailing same-type args are dropped from the signature
	      (unless (signatures sig)                    ; it's not in our collection yet
		(let ((pl (make-string (+ (if cyclic 3 2) (length sig))))
		      (loc (if cyclic 4 3)))
		  (set! (pl 0) #\p) 
		  (if cyclic
		      (begin (set! (pl 1) #\c) (set! (pl 2) #\l) (set! (pl 3) #\_))
		      (begin (set! (pl 1) #\l) (set! (pl 2) #\_)))
		  (for-each 
		   (lambda (typer)
		     (set! (pl loc) (signature->pl typer))
		     (let ((count (or (assq typer sig-symbols)
				      (assq 't sig-symbols))))
		       (set-cdr! count (+ (cdr count) 1)))
		     (set! loc (+ loc 1)))
		   (cdr sig))
		  (set! (signatures sig) pl)))
	      sig))))
      
      (define (initialize-c-file)
	;; C header stuff
	(set! p (open-output-file c-file-name))
	(format p "#include <stdlib.h>~%")
	(format p "#include <stdio.h>~%")
	(format p "#include <string.h>~%")
	(if (string? headers)
	    (format p "#include <~A>~%" headers)
	    (for-each
	     (lambda (header)
	       (format p "#include <~A>~%" header))
	     headers))
	(format p "#include \"shack.h\"~%~%"))
      
      (define collides?
	(let ((all-names ()))
	  (lambda (name)
	    (if (memq name all-names) 
		(format *stderr* "~A twice?~%" name)
		(set! all-names (cons name all-names)))
	    name)))
      
      (define* (add-one-function return-type name arg-types doc)
	;; (format *stderr* "~A ~A ~A~%" return-type name arg-types): double j0 (double) for example
	;; C function -> scheme
	(let ((func-name (symbol->string (collides? name))))
	  (let ((num-args (length arg-types))
		(base-name (string-append (if (> (length prefix) 0) prefix "shack_dl") "_" func-name)) ; not "g" -- collides with glib
		(scheme-name (string-append prefix (if (> (length prefix) 0) ":" "") func-name)))
	  
	    (if (and (= num-args 1) 
		     (eq? (car arg-types) 'void))
		(set! num-args 0))
	    (format p "~%/* -------- ~A -------- */~%" func-name)
	    (format p "static shack_pointer ~A(shack_scheme *sc, shack_pointer ~A)~%" base-name (if (= num-args 1) 'arg 'args))
	    (format p "{~%")
	    
	    ;; get the Scheme args, check their types, assign to local C variables
	    (when (positive? num-args)
	      (if (not (= num-args 1)) (format p "  shack_pointer arg;~%"))
	      (do ((i 0 (+ i 1))
		   (type arg-types (cdr type)))
		  ((= i num-args))
		(format p "  ~A ~A_~D;~%" ((if (pair? (car type)) caar car) type) base-name i))
	      (if (not (= num-args 1)) (format p "  arg = args;~%"))
	      (do ((i 0 (+ i 1))
		   (type arg-types (cdr type)))
		  ((= i num-args))
		
		(let* ((nominal-type ((if (pair? (car type)) caar car) type))  ; double in the example
		       (true-type    ((if (pair? (car type)) cadar car) type))
		       (shack-type      (C-type->shack-type true-type)))                    ; real
		  ;(format *stderr* "~A(~D): ~A ~A ~A~%" func-name i nominal-type true-type shack-type)
		  (if (eq? true-type 'shack_pointer)
		      (format p "    ~A_~D = shack_car(arg);~%" base-name i)
		      (if (eq? shack-type 'c_pointer)
			  (begin
			    (format p "  if (shack_is_c_pointer_of_type(shack_car(arg), shack_make_symbol(sc, ~S)))~%" (symbol->string nominal-type))
			    (format p "    ~A_~D = (~A)shack_c_pointer(shack_car(arg));~%" base-name i nominal-type)
			    (format p "  else return(shack_wrong_type_arg_error(sc, ~S, ~D, shack_car(arg), ~S));~%"
				func-name (if (= num-args 1) 0 (+ i 1)) (symbol->string nominal-type)))
			  (begin
			    (format p "  if (~A(shack_car(arg)))~%" (checker true-type))
			    (format p "    ~A_~D = (~A)~A(~Ashack_car(arg));~%"
				    base-name i
				    nominal-type
				    (shack->C true-type)                               ; shack_number_to_real which requires 
				    (if (memq shack-type '(boolean real))              ;   the extra sc arg
					"sc, " ""))
			    (format p "  else return(shack_wrong_type_arg_error(sc, ~S, ~D, shack_car(arg), ~S));~%"
				    func-name 
				    (if (= num-args 1) 0 (+ i 1))
				    (if (symbol? shack-type) 
					(symbol->string shack-type) 
					(error 'bad-arg (format #f "in ~S, ~S is not a symbol~%" name shack-type)))))))
		  (if (< i (- num-args 1))
		      (format p "  arg = shack_cdr(arg);~%")))))
	    
	    ;; return C value to Scheme
	    (if (pair? return-type) 
		(set! return-type (cadr return-type)))
	    (let ((return-translator (C->shack return-type)))
	      ;(format *stderr* "return ~A ~A~%" return-type return-translator)
	      (format p "  ")
	      (if (not (eq? return-translator #t))
		  (format p "return("))
	      (if (symbol? return-translator)
		  (format p "~A(sc, (~A)" return-translator (C->shack-cast return-type)))
	      (format p "~A(" func-name)
	      (do ((i 0 (+ i 1)))
		  ((>= i (- num-args 1)))
		(format p "~A_~D, " base-name i))
	      (if (positive? num-args)
		  (format p "~A_~D" base-name (- num-args 1)))
	      (format p ")")

	      (if (eq? return-translator 'shack_make_c_pointer_with_type)
		  (format p ", shack_make_symbol(sc, ~S), shack_f(sc))" (symbol->string return-type))
		  (if (symbol? return-translator)
		      (format p ")")))
	      (format p (if (not (eq? return-translator #t))
			    ");~%"
			    ";~%  return(shack_unspecified(sc));~%"))
	      (format p "}~%"))
	    
	    ;; add optimizer connection
	    (define (sig-every? f sequence)
	      (do ((arg sequence (cdr arg)))
		  ((not (and (pair? arg)
			     (f (car arg))))
		   (null? arg))))

	    (when (and (eq? return-type 'double)
		       (< num-args 5)
		       (sig-every? (lambda (p) (eq? p 'double)) arg-types))
	      (let ((local-name #f))
		(case num-args
		  ((0)
		   (set! local-name "_d")
		   (format p "static shack_double ~A~A(void) {return(~A());}~%" func-name local-name func-name))
		  ((1)
		   (set! local-name "_d_d")
		   (format p "static shack_double ~A~A(shack_double x) {return(~A(x));}~%" func-name local-name func-name))
		  ((2)
		   (set! local-name "_d_dd")
		   (format p "static shack_double ~A~A(shack_double x1, shack_double x2) {return(~A(x1, x2));}~%" func-name local-name func-name))
		  ((3)
		   (set! local-name "_d_ddd")
		   (format p "static shack_double ~A~A(shack_double x1, shack_double x2, shack_double x3) {return(~A(x1, x2, x3));}~%" func-name local-name func-name))
		  ((4)
		   (set! local-name "_d_dddd")
		   (format p "static shack_double ~A~A(shack_double x1, shack_double x2, shack_double x3, shack_double x4) {return(~A(x1, x2, x3, x4));}~%" func-name local-name func-name)))
		(set! double-funcs (cons (list func-name scheme-name local-name) double-funcs))))
	    
	    (when (and (eq? return-type 'int)        ; int (f int|double|void)
		       (or ;(= num-args 0)
			   (and (= num-args 1)
				(memq (car arg-types) '(int double)))
			   (and (= num-args 2)
				(eq? (car arg-types) 'int)
				(eq? (cadr arg-types) 'int))))
	      (let ((local-name #f))
		(case (car arg-types)
		  ((void)
		   (set! local-name "_i")
		   (format p "static shack_int ~A~A(void) {return(~A());}~%" func-name local-name func-name))
		  ((double)
		   (set! local-name "_i_7d")
		   (format p "static shack_int ~A~A(shack_scheme *sc, shack_double x) {return(~A(x));}~%" func-name local-name func-name))
		  ((int)
		   (if (= num-args 1)
		       (begin
			 (set! local-name "_i_i")
			 (format p "static shack_int ~A~A(shack_int i1) {return(~A(i1));}~%" func-name local-name (if (string=? func-name "abs") "llabs" func-name)))
		       (begin
			 (set! local-name "_i_ii")
			 (format p "static shack_int ~A~A(shack_int i1, shack_int i2) {return(~A(i1, i2));}~%" func-name local-name func-name)))))
		(set! int-funcs (cons (list func-name scheme-name local-name) int-funcs))))
	    
	    (format p "~%")
	    (set! functions (cons (list scheme-name base-name 
					(if (and (string? doc)
						 (> (length doc) 0))
					    doc
					    (format #f "~A ~A~A" return-type func-name arg-types))
					num-args 0 
					(make-signature return-type arg-types))
				  functions)))))
      
      (define (end-c-file)
	;; now the init function
	;;   the new scheme variables and functions are placed in the current environment
	
	(format p "void ~A(shack_scheme *sc);~%" init-name)
	(format p "void ~A(shack_scheme *sc)~%" init-name)
	(format p "{~%")
	(format p "  shack_pointer cur_env;~%")
	(format p "  shack_pointer ")
	(let ((pls (hash-table-entries signatures))
	      (loc 1))
	  (for-each
	   (lambda (s)
	     (format p "~A~A~A" (cdr s) (if (< loc pls) (values "," " ") (values ";" #\newline)))
	     (set! loc (+ loc 1)))
	   signatures))
	
	(let ((syms ())
	      (names ()))
	  (for-each
	   (lambda (q)
	     (when (positive? (cdr q))
	       (set! syms (cons (car q) syms))
	       (set! names (cons (signature->pl (car q)) names))))
	   sig-symbols)
	  (when (pair? syms)
	    (format p "  {~%    shack_pointer ~{~C~^, ~};~%" names)
	    (for-each
	     (lambda (name sym)
	       (format p (if (eq? sym 't)
			     "    t = shack_t(sc);~%"
			     (values "    ~C = shack_make_symbol(sc, ~S);~%" name (symbol->string sym)))))
	     names syms)))
	(format p "~%")
	(for-each
	 (lambda (sig)
	   (let ((siglen (length (cdar sig)))
		 (cyclic (char=? ((cdr sig) 1) #\c)))
	     (format p (if cyclic 
			   (values "    ~A = shack_make_circular_signature(sc, ~D, ~D" (cdr sig) (- siglen 1) siglen)
			   (values "    ~A = shack_make_signature(sc, ~D" (cdr sig) siglen)))
	     (format p "~{~^, ~C~}" (substring (cdr sig) (if cyclic 4 3)))
	     (format p ");~%")))
	 signatures)
	(format p "  }~%~%")
	(format p "  cur_env = shack_curlet(sc);~%") ; changed from shack_outlet(shack_curlet) 20-Aug-17
	
	;; send out any special initialization code
	(for-each
	 (lambda (init-str)
	   (format p "  ~A~%" init-str))
	 (reverse inits))
	
	;; "constants" -- actually variables in shack because we want them to be local to the current environment (this comment is obsolete)
	(if (pair? constants)
	    (begin
	      (format p "~%")
	      (for-each
	       (lambda (c)
		 (let* ((type (c 0))
			(c-name (c 1))
			(scheme-name (string-append prefix (if (> (length prefix) 0) ":" "") c-name))
			(trans (C->shack type)))
		   (if (eq? trans 'shack_make_c_pointer_with_type)
		       (format p "  shack_define(sc, cur_env, shack_make_symbol(sc, ~S), ~A(sc, (~A)~A, shack_make_symbol(sc, ~S), shack_f(sc)));~%" 
			       scheme-name
			       trans
			       (C->shack-cast type)
			       c-name
			       (if (eq? type 'c-pointer) "void*" (symbol->string type)))
		       (format p "  shack_define(sc, cur_env, shack_make_symbol(sc, ~S), ~A(sc, (~A)~A));~%" 
			       scheme-name
			       trans
			       (C->shack-cast type)
			       c-name))))
	       constants)))
	
	;; C macros -- need #ifdef name #endif wrapper
	(if (pair? macros)
	    (begin
	      (format p "~%")
	      (for-each
	       (lambda (c)
		 (let* ((type (c 0))
			(c-name (c 1))
			(scheme-name (string-append prefix (if (> (length prefix) 0) ":" "") c-name))
			(trans (C->shack type)))
		   (format p "#ifdef ~A~%" c-name)
		   (if (eq? trans 'shack_make_c_pointer_with_type)
		       (format p "  shack_define(sc, cur_env, shack_make_symbol(sc, ~S), shack_make_c_pointer_with_type(sc, (~A)~A, shack_make_symbol(sc, \"~S\"), shack_f(sc)));~%" 
			       scheme-name
			       (C->shack-cast type)
			       c-name
			       type)
		       (format p "  shack_define(sc, cur_env, shack_make_symbol(sc, ~S), ~A(sc, (~A)~A));~%" 
			       scheme-name
			       trans
			       (C->shack-cast type)
			       c-name))
		   (format p "#endif~%")))
	       macros)))
	
	;; functions
	(for-each
	 (lambda (f) 
	   (let ((scheme-name (f 0))
		 (base-name   (f 1))
		 (help        (f 2))
		 (num-args    (f 3))
		 (opt-args    (if (> (length f) 4) (f 4) 0))
		 (sig         (and (> (length f) 5) (f 5))))
	     (format p "~%  shack_define(sc, cur_env,~%            shack_make_symbol(sc, ~S),~%" scheme-name)
	     (format p "            shack_make_typed_function(sc, ~S, ~A, ~D, ~D, false, ~S, ~A));~%"
		     scheme-name
		     base-name
		     num-args
		     opt-args
		     help
		     (if (pair? sig) (signatures sig) 'NULL))))
	 functions)
	
	;; optimizer connection
	(when (pair? double-funcs)
	  (format p "~%  /* double optimizer connections */~%")
	  (for-each
	   (lambda (f)
	     (format p "  shack_set~A_function(shack_name_to_value(sc, ~S), ~A~A);~%" (caddr f) (cadr f) (car f) (caddr f)))
	   double-funcs))
	
	(when (pair? int-funcs)
	  (format p "~%  /* int optimizer connections */~%")
	  (for-each
	   (lambda (f)
	     (format p "  shack_set~A_function(shack_name_to_value(sc, ~S), ~A~A);~%" (caddr f) (cadr f) (car f) (caddr f)))
	   int-funcs))
	
	(format p "}~%")
	(close-output-port p)
	
	;; now we have the module .c file -- make it into a shared object, load it, delete the temp files
	
	(cond ((provided? 'osx)
	       ;; I assume the caller is also compiled with these flags?
	       (system (format #f "~A -c ~A -o ~A ~A ~A" 
			       *cload-c-compiler* c-file-name o-file-name *cload-cflags* cflags))
	       (system (format #f "~A ~A -o ~A -dynamic -bundle -undefined suppress -flat_namespace ~A ~A" 
			       *cload-c-compiler* o-file-name so-file-name *cload-ldflags* ldflags)))
	      
	      ((provided? 'freebsd)
	       (system (format #f "cc -fPIC -c ~A -o ~A ~A ~A" 
			       c-file-name o-file-name *cload-cflags* cflags))
	       (system (format #f "cc ~A -shared -o ~A ~A ~A" 
			       o-file-name so-file-name *cload-ldflags* ldflags)))
	      
	      ((provided? 'openbsd)
	       (system (format #f "~A -fPIC -ftrampolines -c ~A -o ~A ~A ~A" 
			       *cload-c-compiler* c-file-name o-file-name *cload-cflags* cflags))
	       (system (format #f "~A ~A -shared -o ~A ~A ~A" 
			       *cload-c-compiler* o-file-name so-file-name *cload-ldflags* ldflags)))
	      
	      ((provided? 'sunpro_c) ; just guessing here...
	       (system (format #f "cc -c ~A -o ~A ~A ~A" 
			       c-file-name o-file-name *cload-cflags* cflags))
	       (system (format #f "cc ~A -G -o ~A ~A ~A" 
			       o-file-name so-file-name *cload-ldflags* ldflags)))
	      
	      (else
	       (system (format #f "~A -fPIC -c ~A -o ~A ~A ~A" 
			       *cload-c-compiler* c-file-name o-file-name *cload-cflags* cflags))
	       (system (format #f "~A ~A -shared -o ~A ~A ~A" 
			       *cload-c-compiler* o-file-name so-file-name *cload-ldflags* ldflags)))))
      
      (define handle-declaration 
	(let ()
	  (define (add-one-constant type name)
	    ;; C constant -> scheme
	    (let ((c-type (if (pair? type) (cadr type) type)))
	      (if (symbol? name)
		  (set! constants (cons (list c-type (symbol->string (collides? name))) constants))
		  (for-each 
		   (lambda (c)
		     (set! constants (cons (list c-type (symbol->string (collides? c))) constants)))
		   name))))
	  
	  (define (add-one-macro type name)
	    ;; C macro (with definition check) -> scheme
	    (let ((c-type (if (pair? type) (cadr type) type)))
	      (if (symbol? name)
		  (set! macros (cons (list c-type (symbol->string (collides? name))) macros))
		  (for-each 
		   (lambda (c)
		     (set! macros (cons (list c-type (symbol->string (collides? c))) macros)))
		   name))))
	  
	  (define (check-doc func-data)
	    (let ((doc (caddr func-data)))
	      (if (and (string? doc)
		       (> (length doc) 0))
		  func-data
		  (append (list (car func-data) (cadr func-data) (car func-data)) (cdddr func-data)))))
	  
	  (lambda (func)
	    ;; functions
	    (if (>= (length func) 3)
		(apply add-one-function func)
		(case (car func)
		  ((in-C)       (format p "~A~%" (cadr func)))
		  ((C-init)     (set! inits (cons (cadr func) inits)))
		  ((C-macro)    (apply add-one-macro (cadr func)))
		  ((C-function) (collides? (caadr func)) (set! functions (cons (check-doc (cadr func)) functions)))
		  (else         (apply add-one-constant func)))))))
      
      
      ;; c-define-1 (called in c-define macro above)
      (unless (and output-name
		   (file-exists? c-file-name)
		   (file-exists? so-file-name)
		   (provided? 'system-extras)
		   (>= (file-mtime so-file-name) (file-mtime c-file-name))
		   (not (and (file-exists? (port-filename))
			     (< (file-mtime so-file-name) (file-mtime (port-filename))))))
	(format *stderr* "writing ~A~%" c-file-name)
	;; write a new C file and compile it
	(initialize-c-file)
	
	(if (and (pair? (cdr function-info))
		 (symbol? (cadr function-info)))
	    (handle-declaration function-info)
	    (for-each handle-declaration function-info))
	
	(end-c-file)
	(delete-file o-file-name))
      
      ;; load the object file, clean up
      (varlet cur-env 'init_func (string->symbol init-name))
      (format *stderr* "loading ~A~%" so-file-name)
      (load so-file-name cur-env))))


#|
(let ((cd (symbol "complex double"))
      (cd* (symbol "complex double *")))
  (c-define (list cd 'mus_edot_product (list cd cd* 'int))))

;complex double mus_edot_product(complex double freq, complex double *data, mus_long_t size)
|#
