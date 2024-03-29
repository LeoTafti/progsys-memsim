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

# correcteur all doesnt't have any .o

all:: memory.o error.o addr_mng.o commands.o page_walk.o test-addr.o \
test-commands.o test-memory.o test-addr test-commands test-memory

# dependencies ---------------------------------------------------------

memory.o: memory.c memory.h addr.h page_walk.h addr_mng.h util.h error.h
error.o: error.c

addr_mng.o: addr_mng.c addr.h addr_mng.h error.h
commands.o: commands.c commands.h mem_access.h addr.h addr_mng.h error.h
page_walk.o: page_walk.c addr.h addr_mng.h error.h

test-addr.o: test-addr.c tests.h error.h util.h addr.h addr_mng.h
test-commands.o: test-commands.c error.h commands.h mem_access.h addr.h
test-memory.o: test-memory.c error.h memory.h addr.h page_walk.h util.h \
 addr_mng.h


# exe ------------------------------------------------------------------
test-addr: test-addr.o addr_mng.o
test-commands: test-commands.o commands.o addr_mng.o error.o
test-memory: test-memory.o memory.o addr_mng.o page_walk.o error.o

# test-runner ----------------------------------------------------------
test: test-addr test-commands test-memory
	@echo " +++++++ TESTING ADDR +++++++"
	./test-addr
	@echo " +++++++ TESTING COMMANDS +++++++"
	./test-commands tests/files/commands01.txt
	./test-commands tests/files/commands02.txt
	@echo " +++++++ TESTING MEM +++++++"
	./tests/06.basic.sh
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
