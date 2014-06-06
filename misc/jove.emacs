;;
;; Jonathan Paynes .emacs
;;
;; Makes GNUEmacs feel more like JOVE.
;;
;; 2013-Mar-21 - Removed code that wasn't loading in GNU Emacs 23.1.1 - if
;;               functionality was lost that actually did emulate jove behavior
;;               it will have to be redone for more modern Emacs versions.
;;               Mostly what was removed was some Windows NT specific config
;;               code java related abbreviations, and font lock setup. CY

(setq adaptive-fill-mode t)
(setq paragraph-start paragraph-separate)
(setq completion-ignored-extensions (cons ".class" completion-ignored-extensions))
(setq next-screen-context-lines 1)
(setq mouse-scroll-delay .05)
(setq next-line-add-newlines nil)
(setq hilit-quietly t)
(setq inhibit-startup-message t)
(setq require-final-newline t)
(setq completion-auto-help nil)
(setq sentence-end (concat sentence-end "\\|[;:]-)[ .!\"\t]*"))
(setq shell-pushd-dunique t)
(setq shell-pushd-tohome nil)
(setq comint-input-ring-size 128)
(setq comint-eol-on-send t)

(global-set-key [M-backspace] 'backward-kill-word)
(global-set-key "\r" 'newline-and-indent)
(global-set-key "\C-xl" 'ispell-complete-word)
(global-set-key "\C-xf" 'font-lock-fontify-buffer)
(global-set-key "\C-r" 'isearch-backward-regexp)
(global-set-key "\C-s" 'isearch-forward-regexp)
(global-set-key "\C-h" 'delete-backward-char)
(global-set-key "\C-z" 'my-scroll-forward)
(global-set-key "\C-\\" 'advertised-undo)
(global-set-key "\C-x\C-a" 'find-alternate-file)
(define-key esc-map "[" 'backward-paragraph)
(define-key esc-map "]" 'forward-paragraph)
(define-key esc-map "r" 'replace-regexp)
(define-key esc-map "g" 'goto-line)
(define-key ctl-x-map "t" 'find-tag)
(define-key ctl-x-map "\C-c" 'current-error)
(define-key ctl-x-map "\C-n" 'next-error)
(define-key ctl-x-map "\C-p" 'previous-error)
(define-key ctl-x-map "\C-z" 'save-buffers-kill-emacs)
(define-key esc-map "z" 'my-scroll-backward)
(define-key esc-map "Z" 'my-scroll-backward)
(define-key esc-map "w" 'copy-region-as-kill)
(define-key esc-map "W" 'copy-region-as-kill)
(define-key ctl-x-map "d" 'delete-window)
(define-key ctl-x-map "D" 'delete-window)
(define-key ctl-x-map "g" 'enlarge-window)
(define-key ctl-x-map "G" 'enlarge-window)
(define-key ctl-x-map "s" 'shrink-window)
(define-key ctl-x-map "S" 'shrink-window)
(define-key ctl-x-map "4t" 'find-tag-other-window)
(define-key ctl-x-map "4T" 'find-tag-other-window)
(define-key ctl-x-map "r" 'vc-toggle-read-only)
(define-key esc-map "j" 'fill-paragraph)
(define-key esc-map "J" 'fill-paragraph)
(define-key esc-map "q" 'query-replace-regexp)
(define-key esc-map "Q" 'query-replace-regexp)
(define-key ctl-x-map "\C-v" 'revert-buffer)
(define-key ctl-x-map "\C-i" 'insert-file)
(define-key esc-map "." 'end-of-window)
(define-key esc-map "," 'beginning-of-window)
(define-key ctl-x-map "2" 'my-split-window)
(define-key ctl-x-map "\C-m" 'write-modified-buffers)
(define-key global-map "\C-x=" 'buffer-info)

(setq default-mode-line-format '("    (Emacs) -%*- [%b]" global-mode-string " (" mode-name minor-mode-alist ") %l %p " ))

(defun current-line-number ()
  "Returns the current line number, such that goto-line <n> would
go to the current line."
  (save-excursion
    (let ((line 1)
	  (limit (point)))
      (goto-char 1)
      (while (search-forward "\n" limit t 40)
	(setq line (+ line 40)))
      (while (search-forward "\n" limit t 1)
	(setq line (+ line 1)))
      line)))

(defun buffer-info ()
  "Display information about current buffer."
  (interactive)
  (let* ((this-line (current-line-number))
	 (total-lines (+ this-line (count-lines (point) (buffer-size))))
	 (filename (or (buffer-file-name) (buffer-name))))
    (message (format "\"%s\" column %d, line %s/%s, char %d/%d (%d pct)."
		     (file-name-nondirectory filename)
		     (current-column)
		     this-line (1- total-lines)
		     (1- (point)) (buffer-size)
		     (/ (* 100.0 (1- (point))) (buffer-size))))))


(defun current-error ()
  "Goto current error."
  (interactive)
  (next-error 0))

;Missing functions
;
(defun write-modified-buffers (arg)
  (interactive "P")
  (save-some-buffers (not arg)))

(defun end-of-window ()
  "Move point to the end of the window."
  (interactive)
  (goto-char (- (window-end) 1)))

(defun beginning-of-window ()
  "Move point to the beginning of the window."
  (interactive)
  (goto-char (window-start)))

;Make shell mode split the window.
;
(defun my-shell ()
  (interactive)
  (if (= (count-windows) 1)
      (progn (split-window)))
  (select-window (next-window))
  (shell))

(defun my-split-window ()
  (interactive)
  (split-window)
  (select-window (next-window)))

(define-key esc-map "=" 'my-shell)

(defun my-scroll-forward (arg)
  "Scrolls forward by 1 or ARG lines."
  (interactive "p")
  (scroll-up (if (= arg 0) 1 arg)))

(defun my-scroll-backward (arg)
  "Scrolls forward by 1 or ARG lines."
  (interactive "p")
  (scroll-down (if (= arg 0) 1 arg)))

(defun my-next-window ()
  "moves to the window above the current one"
   (interactive)
   (other-window 1))

(defun my-previous-window ()
  "moves to the window above the current one"
   (interactive)
   (other-window -1))

(define-key ctl-x-map "n" 'my-next-window)
(define-key ctl-x-map "p" 'my-previous-window)

(defun cdf ()
  "switches to *shell* and cd's to the current buffer's file dir."
  (interactive)
  (let ((shell-window (get-buffer-window "*shell*"))
	(fn (buffer-file-name)))
    (if shell-window
	(select-window shell-window)
      (my-shell))
    (switch-to-buffer "*shell*")
    (insert "cd " fn)
    (search-backward "/")
    (kill-line)
    (comint-send-input)))

(setq default-major-mode 'text-mode)
(setq-default truncate-lines nil)

;;(setq compilation-error-regexp-alist
;;  '(
;;    ;; UNIX utilities, compiler, Javac, grep, etc
;;    ("\n\
;;\\([a-zA-Z]?:?[^:( \t\n]+\\)[:(][ \t]*\\([0-9]+\\)\\([) \t]\\|\
;;:\\([^0-9\n]\\|\\([0-9]+:\\)\\)\\)" 1 2 5)
;;
;;    ;; Microsoft C/C++, symantec
;;    ("\n\\(\\([a-zA-Z]:\\)?[^:( \t\n-]+\\)[:(][ \t]*\\([0-9]+\\)[:) \t]" 1 3)))

;;replace occurrences of a with b in path
(defun replace-char (path a b)
  (progn
    (while (string-match a path)
      (setq path (replace-match b t t path)))
    path))

;;Some shell stuff

(define-key ctl-x-map "!" 'shell-command)

(defun nuke-shell-buffer ()
  "clears the whole buffer without confirmation"
  (interactive)
  (delete-region (point-min) (point-max)))

(setq shell-mode-hook
    '((lambda ()
	(setq shell-prompt-regexp "[a-z.]*[%$#] \\|(dbx) ")
	(setq comint-input-ring-size 64)
	(setq shell-completion-fignore '("~" "#" "%" ".class" ".o"))
;	(define-key shell-mode-map "\C-m" 'comint-send-input)
;	(define-key shell-mode-map "\C-w" 'backward-kill-word)
	(define-key shell-mode-map "\C-\M-a" 'comint-bol);
	(define-key shell-mode-map "\C-\M-u" 'comint-kill-input)
	(define-key shell-mode-map "\C-\M-p" 'comint-previous-input)
	(define-key shell-mode-map "\C-\M-n" 'comint-next-input)
	(define-key shell-mode-map "\C-\M-r" 'comint-previous-matching-input)
	(define-key shell-mode-map "\M-a" 'comint-bol);
	(define-key shell-mode-map "\C-c\C-u" 'comint-kill-input)
	(define-key shell-mode-map "\C-c\C-p" 'comint-previous-input)
	(define-key shell-mode-map "\C-c\C-n" 'comint-next-input)
	(define-key shell-mode-map "\C-c\C-r" 'comint-previous-matching-input)
	(define-key shell-mode-map "\C-x\C-d" 'nuke-shell-buffer))))

(defun get-defaulted-arg-value (arg default)
  (if (consp arg)
      (car arg)
    (if (null arg)
	default
      arg)))

(defun shift-region-left (arg)
  (interactive "P")
  (let ((m (mark)) (p (point)) tmp)
    (if (< p m) (progn (setq tmp p) (setq p m) (setq m tmp)))
    (indent-rigidly m p (- (get-defaulted-arg-value arg 4)))))

(defun shift-region-right (arg)
  (interactive "P")
  (let ((m (mark)) (p (point)) tmp)
    (if (< p m) (progn (setq tmp p) (setq p m) (setq m tmp)))
    (indent-rigidly m p (get-defaulted-arg-value arg 4))))

(defun filter-region (&optional command)
  "Filter region through shell command."
  (interactive)
  (let* ((command (or command (read-input "Filter command: "))))
    (shell-command-on-region (point) (mark) command t)))

(defun replace-in-region-regexp (&optional old new)
  "Replace REGEXP with TO-STRING in region."
  (interactive)
  (let* ((old (or old (read-input "Replace regexp: ")))
	 (new (or new (read-input (concat "Replace regexp: " old " with: ")))))
    (narrow-to-region (point) (mark))
    (beginning-of-buffer)
    (replace-regexp old new)
    (widen)))

(define-key ctl-x-map ">" 'shift-region-right)
(define-key ctl-x-map "<" 'shift-region-left)

(setq compile-window-height 20)

(if (and nil (not (null window-system)))
    (progn
      (setq c-font-lock-keywords c-font-lock-keywords-2)
      (setq c++-font-lock-keywords c++-font-lock-keywords-2)))

(defun turn-on-font-lock ()
  (font-lock-mode 1))

(defun c-and-c++-hook ()
  (local-set-key "/" 'self-insert-command)
  (local-set-key "{" 'self-insert-command)
  (local-set-key "\M-j" 'c-fill-paragraph)
  (local-set-key "\M-J" 'c-fill-paragraph)
  (local-set-key "\M-q" 'query-replace-regexp)
  (local-set-key "\M-Q" 'query-replace-regexp)
  (local-set-key "\M-i" 'c-indent-command)
  (local-set-key "\C-c\C-e" 'compile-again)
  (local-set-key "\M-e" 'forward-sentence)
  (local-set-key "\M-a" 'backward-sentence)
  (setq c-tab-always-indent nil)
  (setq c-hanging-comment-ender-p t)
  (c-set-offset 'case-label 2)
  (c-set-offset 'statement-case-intro 2)
  (setq c-basic-offset 4)
  (modify-syntax-entry ?# "."))

;;Version 19.30 specific
(add-hook 'c-mode-common-hook 'c-and-c++-hook)
;;(add-hook 'c-mode-common-hook 'turn-on-font-lock)
;;(add-hook 'c-mode-common-hook 'turn-on-font-lock)

(put 'erase-buffer 'disabled nil)
(put 'narrow-to-region 'disabled nil)
