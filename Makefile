all: aplicacion esclavo vista

aplicacion: aplicacion.c 
		gcc -Wall -lpthread -lrt -lm  $< shm_buffer.c -o $@.out

esclavo: esclavo.c
		gcc -Wall $< -o $@.out

vista: vista.c 
		gcc -Wall -lpthread -lrt -lm $< shm_buffer.c -o $@.out 

clean: 
	rm -f *.out

.PHONY: all clean