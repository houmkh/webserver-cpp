
all:main.o http_conn.o conn_timer.o client
	g++ main.o http_conn.o  conn_timer.o  -o webserver -pthread 

client:
	g++ client.cpp -o client
clean: 
	rm -f *.o




