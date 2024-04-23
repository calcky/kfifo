all: inttype-example bytestream-example record-example

inttype-example:
	gcc -o inttype-example inttype-example.c kfifo.c

bytestream-example:
	gcc -o bytestream-example bytestream-example.c kfifo.c

record-example:
	gcc -o record-example record-example.c kfifo.c	

clean:
	rm -f nttype-example bytestream-example record-example