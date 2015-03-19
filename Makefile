all: base core svrkit example

base:
	(cd base; blade build)

core:
	(cd core; blade build)

svrkit:
	(cd svrkit; blade build)

example:
	(cd example; blade build)

.PHONY: all base core svrkit example
