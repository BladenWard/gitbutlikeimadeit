all: gblimi

gblimi: main.c
	gcc -o gblimi main.c -L/opt/homebrew/opt/openssl/lib -I/opt/homebrew/opt/openssl/include -lssl -lcrypto -lz

.PHONY: clean
clean:
	rm gblimi
