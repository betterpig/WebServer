server : main.cc connection_pool.cc http_conn.cc log.cc server.cc
		g++ main.cc connection_pool.cc http_conn.cc log.cc server.cc -lpthread -L/usr/lib64/mysql -lmysqlclient -g -O0 -static-libgcc -lrt -o server

.PHONY : clean
clean :
	rm server main.o connection_pool.o http_conn.o log.o