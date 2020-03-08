### This is the normal makefile to build tests

SRC = test/test_main.cpp
CPPVERFLAG = -std=c++17
EXTRAARGS = -mavx2 -march=native
WARNINGS = -Wall -Wpedantic -Wextra -Werror
CC ?= g++
	
testmake: $(SRC)
	$(CC) -o test_scalar_debug $(SRC) -I./include -O0 -g -DXXH_VECTOR=0 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	$(CC) -o test_scalar_release $(SRC) -I./include -O2 -g -DXXH_VECTOR=0 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	$(CC) -o test_sse2_debug $(SRC) -I./include -O0 -g -DXXH_VECTOR=1 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	$(CC) -o test_sse2_release $(SRC) -I./include -O2 -g -DXXH_VECTOR=1 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	$(CC) -o test_avx2_debug $(SRC) -I./include -O0 -g -DXXH_VECTOR=2 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	$(CC) -o test_avx2_release $(SRC) -I./include -O2 -g -DXXH_VECTOR=2 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	
clean:
	rm -f ./test/test_* 