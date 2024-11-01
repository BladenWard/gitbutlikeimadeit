all: gblimi

gblimi: main.c blob.o hash-object.o index.o uint-util.o
	gcc -o gblimi $^ -L/opt/homebrew/opt/openssl/lib -I/opt/homebrew/opt/openssl/include -lssl -lcrypto -lz 

blob.o: blob.c blob.h
	gcc -c blob.c

hash-object.o: hash-object.c hash-object.h
	gcc -c hash-object.c

index.o: index.c index.h
	gcc -c index.c

uint-util.o: uint-util.c uint-util.h
	gcc -c uint-util.c

.PHONY: clean
clean:
	rm gblimi
