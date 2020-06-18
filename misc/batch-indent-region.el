;;
;;           b a t c h - i n d e n t - r e g i o n . e l
;;
;; Copyright (C) 2005 Christopher Sean Morrison
;;
;; Redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions
;; are met:
;;
;; 1. Redistributions of source code must retain the above copyright
;; notice, this list of conditions and the following disclaimer.
;;
;; 2. Redistributions in binary form must reproduce the above copyright
;; notice, this list of conditions and the following disclaimer in the
;; documentation and/or other materials provided with the distribution.
;;
;; 3. The name of the author may not be used to endorse or promote
;; products derived from this software without specific prior written
;; permission.
;;
;; THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
;; IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
;; WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
;; ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
;; DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
;; GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
;; INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
;; IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
;; OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
;; IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;
;;;;;;
;;
;; batch-indent-region.el provides automatic emacs source indentation.
;;
;; Using the emacs batch mode, this script will perform source code
;; indentation per the formatting rules specified by the editing mode.
;; Emacs local variable blocks may be used override default formatting
;; styles, as shown below.  Here is an example usage:
;;
;; emacs -batch -l batch-indent-region.el -f batch-indent-region pi.c
;;
;; Author: Christopher Sean Morrison <morrison@brlcad.org>
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun batch-indent-region ()
    "Run `batch-indent-region' on the files specified.  Use this from the
command line, with `-batch' only; it won't work in an interactive
Emacs. For example, invoke:
  emacs -batch -l batch-indent-region.el -f batch-indent-region *.c"

    (if (not noninteractive)
	(error "`batch-indent-region' is to be used only with -batch"))

    (while command-line-args-left
	;; don't align backslash line continuations
	(setq c-auto-align-backslashes nil)
	;; don't indent goto labels
	(setq c-label-minimum-indentation 0)

	(setq file (car command-line-args-left))
	(print file)
	(find-file file)

	;; indent switch() { case: sections
	(c-set-offset 'case-label '+)
	;; indent switch() { lines after case:
	(c-set-offset 'statement-case-open '+)
	;; don't indent C++ function decls that span lines
	(c-set-offset 'statement-cont 0)
	;; don't indent function declarations that span lines (probably a bug)
	(c-set-offset 'func-decl-cont 0)
	;; align braces to their parent statement
	(c-set-offset 'substatement-open 0)
	;; don't indent namespace {} blocks
	(c-set-offset 'innamespace '-)
	;; don't indent extern "C" {} blocks
	(c-set-offset 'inextern-lang 0)
	;; don't treat inline functions differently
	(c-set-offset 'inline-open 0)
	;; line up multiline function args
	(c-set-offset 'arglist-cont '(c-lineup-arglist-operators 0))
	;; line up multiline function args with previous arg
	(c-set-offset 'arglist-cont-nonempty '(c-lineup-arglist-operators c-lineup-arglist))
	;; line up multilin function paren with open paren
	(c-set-offset 'arglist-close '(c-lineup-arglist-close-under-paren))
	;; line up C++ template args
	(c-set-offset 'template-args-cont '+)

	;; DO IT
	(indent-region (point-min) (point-max) nil)

	;; only tabify from the beginning of line, not internal spaces
	(setq tabify-regexp "^\t* [ \t]+")
	;; replace leading tabs
	(tabify (point-min) (point-max))

	(save-buffer)
	(setq command-line-args-left (cdr command-line-args-left))))

;; Local Variables:
;; mode: Lisp
;; tab-width: 8
;; lisp-indent-offset: 4
;; indent-tabs-mode: t
;; End:
;; ex: shiftwidth=4 tabstop=8
