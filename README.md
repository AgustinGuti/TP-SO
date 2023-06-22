Para la compilación, se debe realizar el comando

``` {frame="single"}
$ make all
```

Para ejecutarlo hay diferentes opciones: si no se quiere ver la salida
por la consola en tiempo real, se puede ejecutar solamente:

``` {frame="single"}
$ ./aplicacion.out <archivos>
```

y ver el resultado en el archivo *results.txt*.\
Para ver la salida en tiempo real hay 2 opciones, usar un pipe entre el
proceso aplicación y el proceso vista, de la siguiente forma:

``` {frame="single"}
$ ./aplicacion.out <archivos> | ./vista.out
```

Por último, se puede ejecutar

``` {frame="single"}
$ ./aplicacion.out <archivos> &
```

y luego, antes de los 2 segundos, ejecutar, copiando el valor del PID de
aplicación que se imprime por consola.

``` {frame="single"}
$ ./vista.out <PID aplicacion>
```
