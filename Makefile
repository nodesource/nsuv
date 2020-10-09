TOPLEVEL ?= $(dir $(lastword $(MAKEFILE_LIST)))
CPPLINT ?= $(TOPLEVEL)/tools/cpplint.py
PYTHON ?= python

CXXFLAGS = -Wall -Wextra -O0 -g
LDFLAGS = -luv -lrt -lpthread -lnsl -ldl

GCC_CXXFLAGS = -DMESSAGE='"Compiled with GCC"'
CLANG_CXXFLAGS = -fstandalone-debug -DMESSAGE='"Compiled with Clang"'
UNKNOWN_CXXFLAGS = -DMESSAGE='"Compiled with an unknown compiler"'

ifneq '' '$(findstring clang++,$(CXX))'
  CXXFLAGS += $(CLANG_CXXFLAGS)
else ifneq '' '$(findstring g++,$(CXX))'
  CXXFLAGS += $(GCC_CXXFLAGS)
else
  CXXFLAGS += $(UNKNOWN_CXXFLAGS)
endif

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
	$(CXX) ${CXXFLAGS} -std=c++1y -o out/run_tests test/test*.cc ${LDFLAGS}

.PHONY: clean lint lint-test test
