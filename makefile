server: main.o myqueue.o
	g++ main.o myqueue.o -o server -lpthread
main.o: main.cpp
	g++ -c main.cpp myqueue.hpp
myqueue.o: myqueue.cpp myqueue.hpp
	g++ -c myqueue.cpp
clean:
	rm -f *.o