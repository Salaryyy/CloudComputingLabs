coordinator: coordinator.o tools.o conf.o participant
	g++ coordinator.o tools.o conf.o -o coordinator -lpthread
participant: participant.o tools.o conf.o
	g++ participant.o tools.o conf.o -o participant -lpthread
coordinator.o: coordinator.cpp 
	g++ -c coordinator.cpp -o coordinator.o
participant.o: participant.cpp
	g++ -c participant.cpp -o participant.o
conf.o: conf.cpp
	g++ -c conf.cpp -o conf.o
tools.o: tools.cpp
	g++ -c tools.cpp -o tools.o
clean: 
	rm *.o participant coordinator