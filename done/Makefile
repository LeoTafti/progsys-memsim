## =====================================================================
## partial Makefile provided to students
##

CFLAGS = -std=c11 -Wall -Wpedantic -g

# a bit more checks if you'd like to (uncomment
# CFLAGS += -Wextra -Wfloat-equal -Wshadow                         \
# -Wpointer-arith -Wbad-function-cast -Wcast-align -Wwrite-strings \
# -Wconversion -Wunreachable-code

# uncomment if you want to add DEBUG flag
CPPFLAGS += -DDEBUG

# ----------------------------------------------------------------------

# Paul's machine
LDLIBS += -lcheck -lm -lrt -pthread -lsubunit
# Leo's machine
# LDLIBS += -lcheck -lm

all:: memory.o error.o addr_mng.o commands.o page_walk.o list.o tlb_mng.o \
 test-addr.o test-commands.o test-memory.o test-list.o test-tlb_simple.o \
 test-addr test-commands test-memory test-list test-tlb_simple tlb_hrchy_mng.o \
 test-tlb_hrchy

# dependencies ---------------------------------------------------------

memory.o: memory.c memory.h addr.h page_walk.h addr_mng.h util.h error.h
error.o: error.c

addr_mng.o: addr_mng.c addr.h addr_mng.h error.h
commands.o: commands.c commands.h mem_access.h addr.h addr_mng.h error.h
page_walk.o: page_walk.c addr.h addr_mng.h error.h
list.o: list.c list.h
tlb_mng.o: tlb_mng.c tlb.h addr.h tlb_mng.h list.h addr_mng.h page_walk.h \
 error.h
tlb_hrchy_mng.o: tlb_hrchy_mng.c tlb_hrchy_mng.h tlb_hrchy.h addr.h\
 mem_access.h error.h page_walk.c
cache_mng.o: cache_mng.c cache_mng.h cache.h lru.h error.h

test-addr.o: test-addr.c tests.h error.h util.h addr.h addr_mng.h
test-commands.o: test-commands.c error.h commands.h mem_access.h addr.h
test-memory.o: test-memory.c error.h memory.h addr.h page_walk.h util.h \
 addr_mng.h
test-list.o: test-list.c list.h
test-tlb_simple.o: test-tlb_simple.c error.h util.h addr_mng.h addr.h \
 commands.h mem_access.h memory.h list.h tlb.h tlb_mng.h
test-tlb_hrchy.o: test-tlb_hrchy.c error.h util.h addr_mng.h addr.h \
  commands.h mem_access.h memory.h tlb_hrchy.h tlb_hrchy_mng.h
test-cache.o: test-cache.c cache_mng.o error.h

# exe ------------------------------------------------------------------
test-addr: test-addr.o addr_mng.o
test-commands: test-commands.o commands.o addr_mng.o error.o
test-memory: test-memory.o memory.o addr_mng.o page_walk.o error.o
test-list: test-list.o list.o error.o
test-tlb_simple: test-tlb_simple.o list.o error.o addr_mng.o page_walk.o commands.o \
 memory.o tlb_mng.o
test-tlb_hrchy: test-tlb_hrchy.o error.o addr_mng.o commands.o memory.o \
 tlb_hrchy_mng.o page_walk.o
test-cache: test-cache.o error.o addr_mng.o commands.o memory.o cache_mng.o \
 page_walk.o tlb_hrchy_mng.o


# test-runner ----------------------------------------------------------
test: test-addr test-commands test-memory test-list test-tlb_simple test-tlb_hrchy test-cache
	@echo " +++++++ TESTING ADDR +++++++"
	./test-addr
	@echo " +++++++ TESTING COMMANDS +++++++"
	./test-commands tests/files/commands01.txt
	./test-commands tests/files/commands02.txt
	@echo " +++++++ TESTING MEM +++++++"
	./tests/06.basic.sh
	@echo " +++++++ TESTING LIST +++++++"
	./test-list
	@echo "++++++++TESTING TLB++++++++"
	./tests/08.basic.sh
	@echo "++++++++TESTING TLB HRCHY++++++++"
	./test-tlb_hrchy tests/files/commands02.txt tests/files/memory-dump-01.mem resultat.txt
	./tests/09.basic.sh
	@echo "++++++++TESTING CACHE HRCHY++++++++"
	./test-cache dump tests/files/memory-dump-01.mem tests/files/commands01.txt resultat.txt
	# ./tests/11.basic.sh
	@echo " +++++++ DONE +++++++"

# ----------------------------------------------------------------------
# This part is to make your life easier. See handouts how to make use of it.

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

IMAGE=arashpz/feedback:week09
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
