PROJECT = 	socket99
OPTIMIZE = 	-O3
WARN = 		-Wall -Wextra -pedantic

# These are necessary because the library depends on
# both C99 _and_ POSIX (for the BSD sockets API).
CDEFS += 	-D_POSIX_C_SOURCE=1 -D_C99_SOURCE

CFLAGS += 	-std=c99 -g ${WARN} ${CDEFS} ${OPTIMIZE}
#LDFLAGS +=

all: test_${PROJECT}
all: lib${PROJECT}.a

OBJS= socket99.o

TEST_OBJS=

lib${PROJECT}.a: ${OBJS}
	ar -rcs lib${PROJECT}.a ${OBJS}

test_${PROJECT}: test_${PROJECT}.c ${OBJS} ${TEST_OBJS}
	${CC} -o $@ test_${PROJECT}.c ${OBJS} ${TEST_OBJS} ${CFLAGS} ${LDFLAGS}

test: ./test_${PROJECT}
	./test_all

clean:
	rm -f test_${PROJECT} lib${PROJECT}.a *.o *.core

socket99.o: socket99.h
test_socket99.o: socket99.o

# Installation
PREFIX ?=	/usr/local
INSTALL ?=	install
RM ?=		rm

install:
	${INSTALL} -d ${PREFIX}/lib ${PREFIX}/include
	${INSTALL} -c lib${PROJECT}.a ${PREFIX}/lib
	${INSTALL} -c ${PROJECT}.h ${PREFIX}/include

uninstall:
	${RM} -f ${PREFIX}/lib/lib${PROJECT}.a
	${RM} -f ${PREFIX}/include/${PROJECT}.h
