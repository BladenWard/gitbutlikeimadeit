all: gblimi

gblimi: main.c blob.o hash-object.o
	gcc -o gblimi main.c blob.o hash-object.o -L/opt/homebrew/opt/openssl/lib -I/opt/homebrew/opt/openssl/include -lssl -lcrypto -lz 

blob.o: blob.c blob.h
	gcc -c blob.c

hash-object.o: hash-object.c hash-object.h
	gcc -c hash-object.c

.PHONY: clean
clean:
	rm gblimi
