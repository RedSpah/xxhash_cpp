OBJ = test_main.o

%.o: %.cpp 
	$(CXX) -c -o $@ $< $(CPPVERFLAG) $(EXTRAARGS) $(CMDARGS)

xxhashmake: $(OBJ)
	$(CXX) -o $(OUTNAME) $^ -I. -Wall -Wextra $(CPPVERFLAG) $(EXTRAARGS) $(CMDARGS) $(LIBS)