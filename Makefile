# Smart Exam Lab Management System - build
# Linux/dev:   make            -> bin/exam_server, bin/exam_client, tests
# Windows exe: make windows    -> bin/exam_server.exe, bin/exam_client.exe (MinGW cross)
# Native Windows (MinGW installed): use scripts/build_windows.bat instead.
CC      ?= gcc
WINCC   ?= x86_64-w64-mingw32-gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Iinclude -Ithird_party/sqlite -DSQLITE_THREADSAFE=1 -Wno-implicit-fallthrough
CORE     = src/common.c src/config.c src/protocol.c src/database.c src/monitor.c \
           src/allocation.c src/alerts.c src/reports.c third_party/sqlite/sqlite3.c
NET_SRV  = src/server.c src/client.c

all: linux tests

linux: bin/exam_server bin/exam_client

bin/exam_server: $(CORE) $(NET_SRV) src/main_server.c
	mkdir -p bin data reports
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lm

bin/exam_client: $(CORE) $(NET_SRV) src/main_client.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lm

windows: bin/exam_server.exe bin/exam_client.exe

bin/exam_server.exe: $(CORE) $(NET_SRV) src/main_server.c
	mkdir -p bin data reports
	$(WINCC) $(CFLAGS) -static-libgcc -o $@ $^ -lws2_32 -lm

bin/exam_client.exe: $(CORE) $(NET_SRV) src/main_client.c
	mkdir -p bin
	$(WINCC) $(CFLAGS) -static-libgcc -o $@ $^ -lws2_32 -lm

tests: bin/test_health bin/test_protocol bin/test_database bin/test_allocation

bin/test_health: tests/test_health.c src/common.c src/config.c src/monitor.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ -lm

bin/test_protocol: tests/test_protocol.c src/common.c src/protocol.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

bin/test_database: tests/test_database.c $(CORE)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lm

bin/test_allocation: tests/test_allocation.c $(CORE)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lm

test: tests
	./bin/test_health && ./bin/test_protocol && ./bin/test_database && ./bin/test_allocation

clean:
	rm -rf bin data/*.db data/*.db-wal data/*.db-shm

.PHONY: all linux windows tests test clean
