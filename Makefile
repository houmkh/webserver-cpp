
all:main.o http_conn.o client
	g++ main.o http_conn.o  -o webserver -pthread 

client:
	g++ client.cpp -o client
clean: 
	rm -f *.o




