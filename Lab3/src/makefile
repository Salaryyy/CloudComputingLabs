kvstore2pcsystem: conf.o participant coordinator 
	g++ conf.o kvstore2pcsystem.cpp -o kvstore2pcsystem
coordinator: coordinator.o tools.o conf.o participant client
	g++ coordinator.o tools.o conf.o -o coordinator -lpthread
participant: participant.o tools.o conf.o
	g++ participant.o tools.o conf.o -o participant -lpthread
client: client.o tools.o
	g++ client.o tools.o -o client
coordinator.o: coordinator.cpp
	g++ -c coordinator.cpp -o coordinator.o
participant.o: participant.cpp
	g++ -c participant.cpp -o participant.o
client.o: client.cpp
	g++ -c client.cpp -o client.o
conf.o: conf.cpp
	g++ -c conf.cpp -o conf.o
tools.o: tools.cpp
	g++ -c tools.cpp -o tools.o
clean: 
	rm *.o participant coordinator kvstore2pcsystem client