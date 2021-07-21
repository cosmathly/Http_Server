object_file=Reactor.o Thread_Pool.o Log.o Keep_Alive.o
http_server: $(object_file)
	g++ -std=c++11 -o http_server $^ -w -pthread
%.o: %.cpp
	g++ -c -o $@ $< -std=c++11 -w -pthread
.PHONY: clean
clean:
	rm *.o http_server