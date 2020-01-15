TOPLEVEL ?= $(dir $(lastword $(MAKEFILE_LIST)))
CPPLINT ?= $(TOPLEVEL)/tools/cpplint.py
PYTHON ?= python

LINT_SOURCES = \
	include/nsuv.h \
	include/nsuv-inl.h

clean:
	@rm -f $(TOPLEVEL)/out/run_tests

lint:
	@cd $(TOPLEVEL) && $(PYTHON) $(CPPLINT) --filter=-legal/copyright,-build/header_guard \
		$(LINT_SOURCES)

lint-test:
	@cd $(TOPLEVEL) && $(PYTHON) $(CPPLINT) \
		--filter=-legal/copyright,-readability/check test/*.cc test/*.h

test:
	$(CXX) -Wall -Wextra -O0 -g -fstandalone-debug -luv -lrt -lpthread \
		-lnsl -ldl -o out/run_tests test/test*.cc

.PHONY: clean lint lint-test test
