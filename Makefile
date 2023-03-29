all: aplicacion esclavo

aplicacion: aplicacion.c
		gcc -Wall $< -o $@.out

esclavo: esclavo.c
		gcc -Wall $< -o $@.out

clean: 
	rm -f *.out

.PHONY: all clean