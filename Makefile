# Protocoale de comunicatii:
# Laborator 8: Multiplexare
# Makefile

CFLAGS = -Wall -g

# Portul pe care asculta serverul (de completat)
PORT = 7542

# Adresa IP a serverului (de completat)
IP_SERVER = 127.0.0.1

all: server subscriber

# Compileaza server.cpp
server: server.cpp

# Compileaza subscriber.c
subscriber: subscriber.c

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza subscriberul ADM de test
# Rularea normala pentru un client este ./subscriber <nume> <adresa> <server port>
run_subscriber_adm:
	./subscriber adm ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber
