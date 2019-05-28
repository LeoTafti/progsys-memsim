# Makefile for CS-207(a) course - spring 2019
# Author: Jean-Cédric Chappelier
# Date: 05 Nov 2018

CC = gcc

### LDLIBS += -lm

CFLAGS = -std=c11 -Wall -Wpedantic -g

# a bit more checks
CFLAGS += -Wextra -Wfloat-equal -Wshadow                         \
-Wpointer-arith -Wbad-function-cast -Wcast-align -Wwrite-strings \
-Wconversion -Wunreachable-code

CPPFLAGS += -DCS207_STAFF

## ---- WEEK  4 --------------------------------------
TARGETS := 
CHECK_TARGETS := test-addr
OBJS = addr_mng.o error.o

addr_mng.o: addr_mng.c error.h addr_mng.h addr.h util.h

test-addr.o: test-addr.c tests.h error.h util.h addr.h addr_mng.h
test-addr: test-addr.o $(OBJS)

## ---- WEEK  5 --------------------------------------
OBJS += commands.o
TARGETS += test-commands 
CHECK_TARGETS += unit-test-commands

commands.o: commands.c commands.h mem_access.h addr.h addr_mng.h error.h util.h

test-commands.o: test-commands.c error.h commands.h mem_access.h addr.h
test-commands: test-commands.o $(OBJS)

unit-test-commands.o: unit-test-commands.c error.h commands.h mem_access.h addr.h
unit-test-commands: unit-test-commands.o $(OBJS)

## ---- WEEK  6 --------------------------------------
OBJS += page_walk.o memory.o
TARGETS += test-memory test-page_walk

page_walk.o: page_walk.c page_walk.h addr.h

test-memory.o: test-memory.c addr.h memory.h page_walk.h
test-memory: test-memory.o $(OBJS)
test-page_walk.o: test-page_walk.c addr.h memory.h page_walk.h
test-page_walk: test-page_walk.o $(OBJS)

## ---- WEEK  8 --------------------------------------
OBJS += list.o
CHECK_TARGETS += test-lists unit-test-tlb
TARGETS += test-tlb_simple

test-lists.o: test-lists.c tests.h error.h list.h
test-lists: test-lists.o $(OBJS)

unit-test-tlb.o: unit-test-tlb.c tests.h error.h util.h addr_mng.h addr.h \
 tlb_mng.h tlb.h list.h
unit-test-tlb: unit-test-tlb.o tlb_mng.o addr_mng.o list.o error.o

test-tlb_simple.o: test-tlb_simple.c tlb.h addr.h tlb_mng.h list.h
test-tlb_simple: test-tlb_simple.o tlb_mng.o $(OBJS)

## ---- WEEK  9 --------------------------------------
TARGETS += test-tlb_hrchy
CHECK_TARGETS += unit-test-tlb-hrchy

unit-test-tlb-hrchy.o: unit-test-tlb-hrchy.c 
unit-test-tlb-hrchy: unit-test-tlb-hrchy.o tlb_hrchy_mng.o addr_mng.o error.o

## ---- WEEK  9 --------------------------------------
test-tlb_hrchy.o:: test-tlb_hrchy.c error.h util.h addr_mng.h addr.h \
 commands.h mem_access.h memory.h tlb_hrchy.h tlb_hrchy_mng.h
test-tlb_hrchy: test-tlb_hrchy.o tlb_hrchy_mng.o $(OBJS)

test-free-list.o: test-free-list.c tests.h error.h list.h

test-free-list: test-free-list.o list.o

## ---- Staff stuff ------------------------------

## --------------------------------------------------
all:: $(TARGETS)

## --------------------------------------------------
clean::
	-@/bin/rm -f $(TARGETS)

## --------------------------------------------------
# target to do all checks before releasing a new version by staff
release: new check style static-check clean

## --------------------------------------------------
# target to make documentation
doc: Doxyfile
	doxygen Doxyfile

## --------------------------------------------------
feedback-tests: CFLAGS += -DFEEDBACK
feedback-tests: test-addr.o test-lists.o unit-test-tlb.o unit-test-tlb-hrchy.o \
unit-test-cache.o

## ======================================================================

clean::
	-@/bin/rm -f *.o *~ $(CHECK_TARGETS)

new: clean all

static-check:
	scan-build -analyze-headers --status-bugs -maxloop 64 make CC=clang new

style:
	astyle -n -o -A8 *.[ch]

# all those libs are required on Debian, adapt to your box
$(CHECK_TARGETS): LDLIBS += -lcheck -lm -lrt -pthread -lsubunit

check:: $(CHECK_TARGETS)
	$(foreach target,$(CHECK_TARGETS),./$(target);)

# target to run tests
check:: all
	@if ls tests/*.*.sh 1> /dev/null 2>&1; then \
	  for file in tests/*.*.sh; do [ -x $$file ] || echo "Launching $$file"; ./$$file || exit 1; done; \
	fi

IMAGE=arashpz/feedback:latest
feedback:
	@docker pull $(IMAGE)
	@docker run -it --rm -v ${PWD}:/home/tester/done $(IMAGE)

SUBMIT_SCRIPT=../provided/submit.sh
submit1: $(SUBMIT_SCRIPT)
	@$(SUBMIT_SCRIPT) 1

submit2: $(SUBMIT_SCRIPT)
	@$(SUBMIT_SCRIPT) 2

submit:
	@printf 'what "make submit"??\nIt'\''s either "make submit1" or "make submit2"...\n'
