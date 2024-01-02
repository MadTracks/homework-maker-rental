target:	hw4_free

hw4_free: hw4_free.c
		gcc hw4_free.c -o hw4 -lpthread -lrt -Wall
clean:
		rm hw4_free