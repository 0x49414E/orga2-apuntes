# Parte 1 - Respuestas Teoricas

1 - ¿A qué nos referimos con modo real y con modo protegido en un procesador Intel? ¿Qué particularidades tiene cada modo?

Son los diferentes modos de operación que tienen los procesadores de Intel.

- El *Modo Real* provee el entorno de programacion de un Intel 8086 (16 bits); con algunas excepciones, como por ejemplo la posibilidad de cambiar a *Modo Protegido*.

- Por otro lado, el *Modo Protegido* es el modo nativo del procesador. Cuenta con retrocompatibilidad con programas anteriores, flexibilidad, alto rendimiento y un set de funciones que van más alla de lo que se puede lograr en el *Modo Real*.

2 - Comenten en su equipo, ¿Por qué debemos hacer el pasaje de modo real a modo protegido? ¿No podríamos simplemente tener un sistema operativo en modo real? ¿Qué desventajas tendría?

Como poder hacer un sistema operativo en *modo real* se podría, pero estariamos limitados al modo de 16 bits, no tendriamos acceso a todas las funcionalidades del procesador, lo estariamos limitando a imitar al 8086...

3 - ¿Qué es la GDT? ¿Cómo es el formato de un descriptor de segmento, bit a bit? Expliquen brevemente para qué sirven los campos *Limit*, *Base*, *G*, *P*, *DPL*, *S*.

La *GDT* es, como indica el acronimo, la **tabla global de descriptores de segmentos**.Siendo un descriptor de segmentos una estructura de datos almacenada en una tabla (ya sea **GLOBAL** o **LOCAL**) que provee al procesador con el tamaño y ubicacion de un segmento, asi como acceso de control (Tipo, Nivel de privilegio, etc...) e infarmacion de estado.

Un descriptor esta formado por 8 Bytes de informacion separada en distintas partes, cada una con una funcion especifica:

![](img/Cuadro_SegmentDescriptor.jpg)

- Segment Limit: Especifica el tamaño del segmento. Esta formado por 2 segmentos (0b - 15b ... 16b - 19b) que representan un valor de 20 bits. Este sera interpretado de diferente forma dependiendo del valor de la flag de **Granularidad**.

- Base Address: Indica la posicion del byte 0 del segmento dentro de los 4 GB the espacio de direccion linear (El "linear address space" es la memoria? Seguramente no :P). Esta formado por 3 campos que en conjunto equivalen a un valor de 32 bits.

- G Flag (Granularidad): Determina la escala del campo del *Limite de Segmento*. Si la flag esta libre, el Segment Limit se interpreta en unidades de byte. Caso contrario, se lo interpreta en unidades de 4KBytes.

- P Flag (Segment-Present): Indica si un segmento esta presente en memoria (Flag seteada) o no (Flag limpio).

- DPL (Descriptor Privilege Level): Especifica el nivel de privilegio del segment. Este puede ir de 0 a 3, siendo 0 el nivel más privilegiado. Se utiliza para controlar el acceso al segmento.

- S Flag (Descriptor Type): Especifica si un descriptor de segmento es para un **Segmento de sistema** (Flag limpia) o para un **Segmento de codigo o data** (Flag seteada)

4 - ¿Qué combinación de bits tendríamos que usar si queremos especificar un segmento para ejecución y lectura de código?

Si queremos indicar que un segmento es para lectura y ejecucion de codigo; los bits 11, 10, 9 y 8 de la segunda DW se verian de la siguiente forma:

1 _ 1 _

El bit 10 y 8 no afectan en este caso.

5 - Tenemos que direccionar 817 MiB, si pasamos esto a Bytes nos da 856,686,592 bytes. <br>
Para poder indicar este valor como un limite valido en 20bits, tendriamos que leer el limite en unidades de 4KB.

Teniendo esto en cuenta, los Descriptores quedarian como se indica en este [<u>*Excel*</u>](https://docs.google.com/spreadsheets/d/14kKnZ_73GDb6eFquMOuAXC4VqxL-Hm3Z7v-mD-uNP-Q/edit?usp=sharing)

6 - ¿Qué creen que contiene la variable `extern gdt_entry_t gdt;` y `extern gdt_descriptor_t GDT_DESC;`? 

<u>En nuestra opinion</u>:

- <u>*extern gdt_descriptor_t GDT_DESC*</u> seguramente contiene el descriptor de la GDT, el cual indica la base y el limite de la tabla.

- <u>*extern gdt_entry_t gdt*</u> contendra la tabla GDT de descriptores, donde estaran todos los descriptores de segmentos.

7 - Respuesta en el [<u>*_defines.h_*</u>](src/defines.h#L44-L50)

8 - Respuesta en el [<u>*_gdt.c_*</u>](src/gdt.c#L36-L95)

9 - Hecho en [<u>*_kernel.asm_*</u>](src/kernel.asm#L45-L63)

10 - La instruccion LGDT (Load Global Descriptor Table Register) sirve para cargar en el registro GDTR (Global Descriptor Table Register) los valores de base y limite de la GDT (Global Descriptor Table). El operando que toma la instruccion LGDT es una estructura de 6 bytes (en modo de 16 o 32 bits) o 10 bytes (en modo de 64 bits) en ambos casos los 2 bytes mas bajos corresponden al limite y los restantes corresponden a la base, ambos datos son copiados dentro del registro GDTR, que el procesador usa para localizar la GDT. 
Con estos valores copiados el registro GDTR se encarga de indicarnos donde se encuentra y de que tamaño es la GDT. 
La instruccion LGDT solo puede ejecutarse en codigo de sistema operativo, no en programas comunes. Generalmente la utilizamos durante la inicializacion del sistema operativo, antes de activar el modo protegido. 

11 - Hecho en [<u>*_kernel.asm_*</u>](src/kernel.asm#L67) 

13 - Si, deberiamos modificar el registro CRO para pasar a modo protegido, ya que este contiene las flags de control del sistema, la cuales controlan el modo de operar y los registros del sistema. Justamente una de estas flags es la PE (Protection Enable), entonces para pasar a modo protegido debemos setear esta flag en 1. 

14 - Hecho en [<u>*_kernel.asm_*</u>](src/kernel.asm#L94-L96)

15 - Utilizamos el CS_RING_0_SEL definido en kernel.asm.

16 - Hecho en [<u>*_kernel.asm_*</u>](src/kernel.asm#L111-L116)

17 - Hecho en [<u>*_kernel.asm_*</u>](src/kernel.asm#L119-L120)

18 - Hecho en [<u>*_kernel.asm_*</u>](src/kernel.asm#L123)

21 - Hecho en [<u>*Excel*</u>](https://docs.google.com/spreadsheets/d/14kKnZ_73GDb6eFquMOuAXC4VqxL-Hm3Z7v-mD-uNP-Q/edit?usp=sharing) y definido en [<u>*_gdt.c_*</u>](src/gdt.c#L96-L111)

22 - ¿Qué creen que hace el método **screen_draw_box**? ¿Cómo hace para acceder a la pantalla? ¿Qué estructura usa para representar cada carácter de la pantalla y cuanto ocupa en memoria?

*_screen_draw_box_* recibe la Fila y Columna iniciales, la cantidad de Filas y Columnas que queremos pintar, el caracter y los atributos que pondremos en cada una de las celdas (Todo acorde a la convencion *_Text_UI_*); el metodo se encargara de colocar en cada celda desde (FInit, CInit) hasta (FInit + FSize, CInit + CSize) el caracter y atributo indicado.

Para acceder a la pantalla declaramos a "p" como un puntero a un arreglo de 80 posiciones (Porque asi es la resolucion de nuestra pantalla). Este arreglo tendra a su vez un arreglo de 50 posiciones en cada una de sus posiciones, que contendra un "ca" formado por el caracter y atributo de la celda.

Para representar cada caracter de la pantalla usamos el tipo de dato ca definido en [<u>*_screen.h_*</u>](src/screen.h#L22-L25). En memoria ocupa 16 bits.

23 - Hecho en [<u>*_screen.c_*</u>](src/screen.c#L76-L82)

24 - Para activar el procesador en modo protegido tuvimos que:

- Desactivar las interrupciones.

- Habilitar A20.

- Cargar la GDT en modo FLAT con la instruccion lgdt.

- Setear el bit PE de CR0.

- Hacer un jmp al Modo Protegido.

En general lo que más nos interesó de esta parte del TP es estructurar el funcionamiento de la pantalla, desde armar el descriptor de segmento de video hasta llegar a dibujar en la pantalla del QEMU. 
