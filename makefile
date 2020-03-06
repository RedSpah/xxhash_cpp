OBJ = test_main.o

%.o: %.cpp 
	$(COMPILER) -c -o $@ $< $(CPPVERFLAG) $(EXTRAARGS) $(CMDARGS)

xxhashmake: $(OBJ)
	$(COMPILER) -o $(OUTNAME) $^ -I. -Wall -Wextra $(CPPVERFLAG) $(EXTRAARGS) $(CMDARGS) $(LIBS)