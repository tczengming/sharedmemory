CXX=g++
CCFLAG=-I../

%.o : %.cpp
	$(CXX) $(CCFLAG) -c $< -o $@

example.out:example.o ../shared_memory.o
	$(CXX) $(CCFLAG) -lpthread -lrt -o $@ $?
