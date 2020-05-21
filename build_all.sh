#/usr/bin/env bash
(cd base; blade build $*)
(cd core; blade build $*)
(cd svrkit; blade build $*)
(cd example; blade build $*)

