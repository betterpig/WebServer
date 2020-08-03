server : main.cc connection_pool.cc http_conn.cc log.cc
	g++ main.cc connection_pool.cc http_conn.cc log.cc -lpthread -lmysqlclient -static-libgcc -lrt -o server

.PHONY : clean
clean :
	rm server main.o connection_pool.o http_conn.o log.o