all: aplicacion esclavo vista vista_2

aplicacion: aplicacion.c 
		gcc -Wall -lpthread -lrt -lm  $< shm_buffer.c -o $@.out

esclavo: esclavo.c
		gcc -Wall $< -o $@.out

vista: vista.c 
		gcc -Wall -lpthread -lrt -lm $< shm_buffer.c -o $@.out 

vista_2: vista_2.c 
		gcc -Wall -lpthread -lrt -lm $< -o $@.out 

clean: 
	rm -f *.out

.PHONY: all clean