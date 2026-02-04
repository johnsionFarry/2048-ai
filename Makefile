CFLAGS += -std=c99 -Wextra
CXXFLAGS += -std=c++11 -Wextra
PREFIX ?= /usr

.PHONY: all clean test

all: 2048

2048: main.c Game2048.cpp Game2048.h
	$(CXX) $(CXXFLAGS) main.c Game2048.cpp -o 2048 -lpthread

test:
	./2048 test

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -p 2048 $(DESTDIR)$(PREFIX)/bin/2048

clean:
	rm -f 2048