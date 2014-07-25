PROJECT = socket99
OPTIMIZE = -O3
WARN = -Wall -Wextra -pedantic

# This is necessary because the library depends on
# both C99 _and_ POSIX (for the BSD sockets API).
CDEFS += -D_POSIX_C_SOURCE=1

CFLAGS += -std=c99 -g ${WARN} ${CDEFS} ${OPTIMIZE}
#LDFLAGS +=

all: test_${PROJECT}
#all: ${PROJECT}
all: lib${PROJECT}.a

OBJS= socket99.o

TEST_OBJS=

${PROJECT}: main.c ${OBJS}
	${CC} -o $@ main.c ${OBJS} ${LDFLAGS}

lib${PROJECT}.a: ${OBJS}
	ar -rcs lib${PROJECT}.a ${OBJS}

test_${PROJECT}: test_${PROJECT}.c ${OBJS} ${TEST_OBJS}
	${CC} -o $@ test_${PROJECT}.c ${OBJS} ${TEST_OBJS} ${CFLAGS} ${LDFLAGS}

test: ./test_${PROJECT}
	test_all

clean:
	rm -f ${PROJECT} test_${PROJECT} *.o *.a *.core

socket99.o: socket99.h
test_socket99.o: socket99.o
