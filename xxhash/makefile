### This is the normal makefile to build tests

OBJ = test/test_main.o
CPPVERFLAG = -std=c++17
EXTRAARGS = -mavx2 -march=native
WARNINGS = -Wall -Wpedantic -Wextra -Werror
CC ?= g++

%.o: %.cpp 
	$(CC) -c -o $@ $< -I./include $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS)
	
testmake: $(OBJ)
	$(CC) -o test_scalar_debug $^ -I./include -O0 -g -DXXH_VECTOR=0 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	$(CC) -o test_scalar_release $^ -I./include -O2 -g -DXXH_VECTOR=0 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	$(CC) -o test_sse2_debug $^ -I./include -O0 -g -DXXH_VECTOR=1 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	$(CC) -o test_sse2_release $^ -I./include -O2 -g -DXXH_VECTOR=1 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	$(CC) -o test_avx2_debug $^ -I./include -O0 -g -DXXH_VECTOR=2 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	$(CC) -o test_avx2_release $^ -I./include -O2 -g -DXXH_VECTOR=2 $(CPPVERFLAG) $(EXTRAARGS) $(WARNINGS) 
	
clean:
	rm -f ./test/test_* 