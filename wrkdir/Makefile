all: gblimi

gblimi: main.c blob.o hash-object.o index.o tree.o uint-util.o
	gcc -o gblimi $^ -L/opt/homebrew/opt/openssl/lib -I/opt/homebrew/opt/openssl/include -lssl -lcrypto -lz 

blob.o: blob.c blob.h
	gcc -c blob.c

hash-object.o: hash-object.c hash-object.h
	gcc -c hash-object.c

index.o: index.c index.h
	gcc -c index.c

tree.o: tree.c tree.h
	gcc -c tree.c

uint-util.o: uint-util.c uint-util.h
	gcc -c uint-util.c

.PHONY: clean
clean:
	rm gblimi
