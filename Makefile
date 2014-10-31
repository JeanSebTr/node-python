DEBUG=node-python*
PYTHONPATH=./test/support

test:
	$(MAKE) DEBUG= test-debug
	
test-debug:
	DEBUG=$(DEBUG) PYTHONPATH=$(PYTHONPATH) ./node_modules/.bin/mocha -R spec

.PHONY: test test-debug
