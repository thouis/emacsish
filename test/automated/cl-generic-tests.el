;;; cl-generic-tests.el --- Tests for cl-generic.el functionality  -*- lexical-binding: t; -*-

;; Copyright (C) 2015  Stefan Monnier

;; Author: Stefan Monnier <monnier@iro.umontreal.ca>

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

;;; Commentary:

;;; Code:

(require 'ert)
(require 'cl-lib)

(cl-defgeneric cl--generic-1 (x y))
(cl-defgeneric (setf cl--generic-1) (v y z) "My generic doc.")

(ert-deftest cl-generic-test-0 ()
  (cl-defgeneric cl--generic-1 (x y))
  (cl-defmethod cl--generic-1 ((x t) y) (cons x y))
  (should (equal (cl--generic-1 'a 'b) '(a . b))))

(ert-deftest cl-generic-test-1-eql ()
  (cl-defgeneric cl--generic-1 (x y))
  (cl-defmethod cl--generic-1 ((x t) y) (cons x y))
  (cl-defmethod cl--generic-1 ((_x (eql 4)) _y)
    (cons "quatre" (cl-call-next-method)))
  (cl-defmethod cl--generic-1 ((_x (eql 5)) _y)
    (cons "cinq" (cl-call-next-method)))
  (cl-defmethod cl--generic-1 ((_x (eql 6)) y)
    (cons "six" (cl-call-next-method 'a y)))
  (should (equal (cl--generic-1 'a nil) '(a)))
  (should (equal (cl--generic-1 4 nil) '("quatre" 4)))
  (should (equal (cl--generic-1 5 nil) '("cinq" 5)))
  (should (equal (cl--generic-1 6 nil) '("six" a))))

(cl-defstruct cl-generic-struct-parent a b)
(cl-defstruct (cl-generic-struct-child1 (:include cl-generic-struct-parent)) c)
(cl-defstruct (cl-generic-struct-child11 (:include cl-generic-struct-child1)) d)
(cl-defstruct (cl-generic-struct-child2 (:include cl-generic-struct-parent)) e)

(ert-deftest cl-generic-test-2-struct ()
  (cl-defgeneric cl--generic-1 (x y) "My doc.")
  (cl-defmethod cl--generic-1 ((x t) y) "Doc 1." (cons x y))
  (cl-defmethod cl--generic-1 ((_x cl-generic-struct-parent) y)
    "Doc 2." (cons "parent" (cl-call-next-method 'a y)))
  (cl-defmethod cl--generic-1 ((_x cl-generic-struct-child1) _y)
    (cons "child1" (cl-call-next-method)))
  (cl-defmethod cl--generic-1 :around ((_x t) _y)
    (cons "around" (cl-call-next-method)))
  (cl-defmethod cl--generic-1 :around ((_x cl-generic-struct-child11) _y)
    (cons "child11" (cl-call-next-method)))
  (cl-defmethod cl--generic-1 ((_x cl-generic-struct-child2) _y)
    (cons "child2" (cl-call-next-method)))
  (should (equal (cl--generic-1 (make-cl-generic-struct-child1) nil)
                 '("around" "child1" "parent" a)))
  (should (equal (cl--generic-1 (make-cl-generic-struct-child2) nil)
                 '("around""child2" "parent" a)))
  (should (equal (cl--generic-1 (make-cl-generic-struct-child11) nil)
                 '("child11" "around""child1" "parent" a))))

(ert-deftest cl-generic-test-3-setf ()
  (cl-defmethod (setf cl--generic-1) (v (y t) z) (list v y z))
  (cl-defmethod (setf cl--generic-1) (v (_y (eql 4)) z) (list v "four" z))
  (should (equal (setf (cl--generic-1 'a 'b) 'v) '(v a b)))
  (should (equal (setf (cl--generic-1 4 'b) 'v) '(v "four" b)))
  (let ((x ()))
    (should (equal (setf (cl--generic-1 (progn (push 1 x) 'a)
                                        (progn (push 2 x) 'b))
                         (progn (push 3 x) 'v))
                   '(v a b)))
    (should (equal x '(3 2 1)))))

(ert-deftest cl-generic-test-4-overlapping-tagcodes ()
  (cl-defgeneric cl--generic-1 (x y) "My doc.")
  (cl-defmethod cl--generic-1 ((y t) z) (list y z))
  (cl-defmethod cl--generic-1 ((_y (eql 4)) _z)
                (cons "four" (cl-call-next-method)))
  (cl-defmethod cl--generic-1 ((_y integer) _z)
                (cons "integer" (cl-call-next-method)))
  (cl-defmethod cl--generic-1 ((_y number) _z)
                (cons "number" (cl-call-next-method)))
  (should (equal (cl--generic-1 'a 'b) '(a b)))
  (should (equal (cl--generic-1 1 'b) '("integer" "number" 1 b)))
  (should (equal (cl--generic-1 4 'b) '("four" "integer" "number" 4 b))))

(ert-deftest cl-generic-test-5-alias ()
  (cl-defgeneric cl--generic-1 (x y) "My doc.")
  (defalias 'cl--generic-2 #'cl--generic-1)
  (cl-defmethod cl--generic-1 ((y t) z) (list y z))
  (cl-defmethod cl--generic-2 ((_y (eql 4)) _z)
                (cons "four" (cl-call-next-method)))
  (should (equal (cl--generic-1 4 'b) '("four" 4 b))))

(ert-deftest cl-generic-test-6-multiple-dispatch ()
  (cl-defgeneric cl--generic-1 (x y) "My doc.")
  (cl-defmethod cl--generic-1 (x y) (list x y))
  (cl-defmethod cl--generic-1 (_x (_y integer))
    (cons "y-int" (cl-call-next-method)))
  (cl-defmethod cl--generic-1 ((_x integer) _y)
    (cons "x-int" (cl-call-next-method)))
  (cl-defmethod cl--generic-1 ((_x integer) (_y integer))
    (cons "x&y-int" (cl-call-next-method)))
  (should (equal (cl--generic-1 1 2) '("x&y-int" "x-int" "y-int" 1 2))))

(ert-deftest cl-generic-test-7-apo ()
  (cl-defgeneric cl--generic-1 (x y)
    (:documentation "My doc.") (:argument-precedence-order y x))
  (cl-defmethod cl--generic-1 (x y) (list x y))
  (cl-defmethod cl--generic-1 (_x (_y integer))
    (cons "y-int" (cl-call-next-method)))
  (cl-defmethod cl--generic-1 ((_x integer) _y)
    (cons "x-int" (cl-call-next-method)))
  (cl-defmethod cl--generic-1 ((_x integer) (_y integer))
    (cons "x&y-int" (cl-call-next-method)))
  (should (equal (cl--generic-1 1 2) '("x&y-int" "y-int" "x-int" 1 2))))

(provide 'cl-generic-tests)
;;; cl-generic-tests.el ends here