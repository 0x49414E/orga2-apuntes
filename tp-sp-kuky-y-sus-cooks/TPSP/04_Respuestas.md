### Primera parte: Inicialización de tareas

#### Convención Pasaje de Parametros - Int 88:

El usuario le pasa a la int88 el parametro a través del registro EAX. Este es luego pusheado al stack para pasarla como parametro a la función de C.


**1.** Si queremos definir un sistema que utilice sólo dos tareas, ¿Qué
nuevas estructuras, cantidad de nuevas entradas en las estructuras ya
definidas, y registros tenemos que configurar?¿Qué formato tienen?
¿Dónde se encuentran almacenadas?

Para definir un sistema que utilice sólo dos tareas necesitamos definir:

-   El Task Register, que almacena el selector de segmento de la tarea en ejecución. Se utiliza para encontrar la TSS de la tarea actual.

    Esta tiene la siguiente forma:
    ![](img/task_register.webp)
    Este registro esta conformado por dos partes
    - **Parte visible**: Esta parte puede leerse y escribirse mediante instrucciones de software (LTR, STR). Contiene un selector de 16bits que apunta a un TSS Descriptor de la GDT.
    - **Parte invisible**: Esta parte no es accesible al software. El procesador la mantiene internamente para optimizar el acceso a la TSS. Contiene una direccion base de 32bits en memoria de la TSS y un Limite de tamaño de Segmento de 16bits. 

-   La TSS es el Segmento de Estado de Tarea, que guarda el contexto de ejecucion de la tarea, es decir que va a guardar los valores en los registros de la CPU y que usara en la ejecucion de la misma. Los campos mas relevantes de la TSS son: 
    - **EIP**
    - **ESP**,**EBP**,**ESP0**
    - Los selectores de segmento **CS**,**DS**,**ES**,**FS**,**GS**,**SS**,**SS0**
    - El **CR3** que va a tener la paginacion asociada a la tarea. Asi cada tarea tendra su propio Page Directory
    - **EFLAGS** en 0x00000202 para tener las interrupciones habilitadas. 

-   TSS Descriptor: Es una entrada en la GDT de tipo TSS descriptor.

```
EL TSS DESCRIPTOR ES EXCLUSIVO DE LA GDT.
NO PUEDE SER INGRESADO EN LA LDT O IDT.
```

Sus atributors son los siguientes: 
    -  **AVL** 
    -  **B** (Busy): Indica si la tarea esta "Busy", una tarea "Busy" es aquella que esta en ejecución o suspenso. Lo inicializaremos siempre en 0. El procesador utiliza la **B flag** para detector intento de llamada a una tarea cuya ejecución fue interrumpida. Para asegurarse de que solo hay una **B Flag** asociada a una tarea, cada **TSS** tendría que tener un solo **TSS Descriptor** que lo apunte.
    -  **DPL** (Descriptor Privilege Level): Es el nivel de privilegio que se precisa para acceder al segmento que se precisa para acceder al segmento. Usamos nivel 0 porquer solo el kernel puede intercambiar tareas.
    En la mayoria de sistemas, los DPLs de TSS Descriptors se setean en valores menores a 3, para que solo software privilegiado pueda realizar cambios de tareas. Sin embargo, en aplicaciones multitarea, DPLs de algunos TSS Descriptors podrian estar seteados a 3 para permitir a el cambio de tareas a nivel de privilegio de aplicación o usuario. 
    -  **LIMIT**: Es el tamaño maximo de la TSS. 67h es el minimo requerido. 
    -  **BASE**: Indica la direccion base de la TSS. 
    -  **G** (Granularity): Cuando esta seteada en 0, el limite debe tener un valor igual o mayor a *0x67*; un byte menos que el tamaño minimo de una **TSS**. Intentar cambiar a una tarea cuyo **Descriptor TSS** tenga un limite menor a *0x67* genera una *invalid-TSS-exception* (#TS).
    -  **P**: Indica que el segmento este presente en memoria y se pueda acceder a el. 
    -  **TYPE**: 

**2.** ¿A qué llamamos cambio de contexto? ¿Cuándo se produce? ¿Qué efecto
tiene sobre los registros del procesador? Expliquen en sus palabras que
almacena el registro **TR** y cómo obtiene la información necesaria para
ejecutar una tarea después de un cambio de contexto.

Llamamos **cambio de contexto** al proceso de guardar el contexto de la tarea actual para poder restaurarlo en caso de ser necesario, y cargar el contexto de la nueva tarea a ejecutar. Este se produce al momento de realizar un **cambio de tarea**. Al momento de realizar un cambio de contexto, los registros del procesador veran sus valores cambiados por los que esten guardados en el contexto de la tarea nueva. Lo que almacena el registro **TR** esta explicado en el punto 1 en la seccion **Task Register**. 
Al momento de realizarse un cambio de tarea, el **TR** es automaticamente cargado con el **segment selector y descriptor** de la **TSS** correspondiente a la nueva tarea.

**3.** Al momento de realizar un cambio de contexto el procesador va
almacenar el estado actual de acuerdo al selector indicado en el
registro **TR** y ha de restaurar aquel almacenado en la TSS cuyo
selector se asigna en el *jmp* far. ¿Qué consideraciones deberíamos
tener para poder realizar el primer cambio de contexto? ¿Y cuáles cuando
no tenemos tareas que ejecutar o se encuentran todas suspendidas?

**4.** ¿Qué hace el scheduler de un Sistema Operativo? ¿A qué nos
referimos con que usa una política?

El **Scheduler** es la parte del sistema operativo que decide que tarea o proceso debe ejecutarse en el procesador en un momento dado. Como se explico en puntos anteriores el CPU tiene mecanismos, como la **TSS** y el **TR**, que permiten cambiar de tarea guardando y cargando automaticamente el contexto de ejecucion. Sin embargo, nuestro amigo el scheduler, es quien controla cuando y a quien se le da el control del CPU. Este invoca instrucciones como **LTR** o configura la GDT y las TSS para establecer que tareas pueden ejecutarse. 
Cuando decimos que el scheduler usa una **politica**, nos referimos a que sigue un conjunto de reglas o criterios para decidir que tarea ejecutar entre todas las disponibles. 
Segun el manual, un cambio de tarea se puede dar por: 
    - El programa, tarea o proceso actual ejecuta una instruccion JMP o CALL a un **TSS descriptor** almacenado en la **GDT**.
    - El programa, tarea o proceso actual ejecuta una instruccion JMP o CALL a un *task-gate descriptor* almacenado en la **GDT** o **LDT**
    - Una interrupcion o excepcion que apunten a un *task-gate descriptor* almacenado en la **IDT**.
    - La tarea actual ejecuta un **IRET** cuando la **flag NT** de **EFLAGS** esta seteada. 

**5.** En un sistema de una única CPU, ¿cómo se hace para que los
programas parezcan ejecutarse en simultáneo?

El scheduler se encarga de ir dando tiempo a cada tarea para poder ejecutarse, este proceso sucede de manera tan rapida que para el ojo humano es como si todas las tareas se ejecutaran en simultaneo en la CPU; sin embargo cada una se esta turnando para utilizarlo.

**9.** Utilizando **info tss**, verifiquen el valor del **TR**.
También, verifiquen los valores de los registros **CR3** con **creg** y de los registros de segmento **CS,** **DS**, **SS** con
***sreg***. ¿Por qué hace falta tener definida la pila de nivel 0 en la
tss?

Porque el procesador necesita saber dónde está la pila del kernel para poder cambiar de contexto de forma segura cuando ocurre una interrupción o excepción mientras el código de usuario está corriendo.

### Segunda parte: Poniendo todo en marcha

**11.** 
a)  Expliquen con sus palabras que se estaría ejecutando en cada tic
del reloj línea por línea

    - Con 'call pic_finish1' le avisamos a la pic que estamos por atneder una interrupcion
    
    - Luego con 'call sched_next_task' avanzamos a la siguiente tarea en el scheduler 
    
    - Si la siguiente tarea es la misma que esta cargada actualmente en el **TR** le damos mas ciclos de clock finalizando la interrupcion de reloj actual.
    
    - Caso contrario cargamos la siguiente tarea en la direccion de memoria del descriptor de la tss.
    
    - Y por ultimo hacemos un jmp far al offset de la siguiente tarea. 

b)  En la línea que dice ***jmp far \[sched_task_offset\]*** ¿De que
    tamaño es el dato que estaría leyendo desde la memoria? ¿Qué 
    indica cada uno de estos valores? ¿Tiene algún efecto el offset
    elegido? 
    
    El tamaño del dato que se esta leyendo en *'jmp far [sched_task_offset]'* es de 6bytes (4 bytes son del offset, y 2 del selector). 
    No tiene ningun efecto pues el selector es de TSS, entonces realizamos un task switch y por ende el offset se ignora.

c)  ¿A dónde regresa la ejecución (***eip***) de una tarea cuando
    vuelve a ser puesta en ejecución? 

    El EIP vuelve al punto donde habia quedado antes de cambiar a la tarea finalizada (osea, la dirección de EIP guardada en la TSS).


**12.** Para este Taller la cátedra ha creado un scheduler que devuelve
la próxima tarea a ejecutar.

a)  En los archivos **sched.c** y **sched.h** se encuentran definidos
    los métodos necesarios para el Scheduler. Expliquen cómo funciona
    el mismo, es decir, cómo decide cuál es la próxima tarea a
    ejecutar. Pueden encontrarlo en la función ***sched_next_task***.

    Sched_next_task selecciona la siguiente tarea ejecutable implementando un buffer circular, en el cual si dicha tarea se pasa de cantidad maxima de tareas, la selecciona como si fuera la primera. 
    Una vez tenemos la siguiente tarea disponible, la ejecutamos y retorna el selector de la misma. 
    Si no hay ninguna tarea disponible para ejecutar, retorna el selector de la tarea idle.

### Tercera parte: Tareas? Qué es eso?

**14.** Como parte de la inicialización del kernel, en kernel.asm se
pide agregar una llamada a la función **tasks_init** de
**task.c** que a su vez llama a **create_task**. Observe las
siguientes líneas:
```C
int8_t task_id = sched_add_task(gdt_id << 3); 
//Rellena el primer slot de tarea libre en el scheduler con el selector (gdt_id << 3) y le asigna como estado "pausado".

tss_tasks[task_id] = tss_create_user_task(task_code_start[tipo]); 
//Creamos una tarea de usuario y la almacenamos en id que definimos en sched_add_task, dentro de la tss

gdt[gdt_id] = tss_gdt_entry_for_task(&tss_tasks[task_id]);
//Crea una entrada 
```
a)  ¿Qué está haciendo la función ***tss_gdt_entry_for_task***?

La funcion **tss_gdt_entry_for_task** nos crea una entrada para la **GDT** , de tipo **TSS**, en base a la task que añadimos al sched.

b)  ¿Por qué motivo se realiza el desplazamiento a izquierda de
    **gdt_id** al pasarlo como parámetro de ***sched_add_task***?

Necesitamos desplazar el id de la gdt por 3 para asi generar espacio para los bits que indican: 

    -   Si el selector apunta a la GDT o LDT.

    -   Cual es su nivel de privilegio.

**15.** Ejecuten las tareas en *qemu* y observen el código de estas
superficialmente.

a) ¿Qué mecanismos usan para comunicarse con el kernel?

Existe un espacio de memoria compartido entre tareas de usuario y el kernel, este esta definido en [shared.h](src/shared.h) y almacena lo siguiente:

    - El tick_count. 
    
    - El task_id de la tarea actual.
    
    - El keyboard_state (Un struct con booleanos por cada tecla del teclado).

Este esta definido como volatile, lo que indica que puede cambiar por razones que estan fuera del control de la tarea. Esto permite por ejemplo que exista la nocion del paso del tiempo gracias a la modificacion de tick_count.
Las tareas tienen permitido leerlo y el kernel puede modificarlo, de esta manera logran comunicarse entre si.

b) ¿Por qué creen que no hay uso de variables globales? ¿Qué pasaría si
    una tarea intentase escribir en su `.data` con nuestro sistema?

Por lo que entendimos leyendo de la OSDev Wiki, en .data se almacenan las variables globales. Como nuestro `.data` no esta mapeado (Info sacada del discord), el intentar usar variables globales produciria automaticamente un **PAGE FAULT**; y este no fue programado para poder mapear cosas del `.data` por lo que no podria manejar correctamente la excepción.

**16.** Observen **tareas/task_prelude.asm**. El código de este archivo
se ubica al principio de las tareas.

a. ¿Por qué la tarea termina en un loop infinito?

Nos interesa que la tarea termine en un loop infinito para evitar que se ejecute basura luego de la finalización de la misma.

\[Opcional\]

b. ¿Qué podríamos hacer para que esto no sea necesario?

### Cuarta parte: Hacer nuestra propia tarea

#### Análisis:

**18.** Analicen el *Makefile* provisto. ¿Por qué se definen 2 "tipos"
de tareas? ¿Como harían para ejecutar una tarea distinta? Cambien la
tarea *Snake* por una tarea *PongScoreboard*.

Tenemos 2 tipos de tareas distintas. Las tareas ejecutadas en asm puro, mayormente utilizadas para la tarea idle que su unica funcionalidad es esperar. Este tipo de tareas pesa 4KB (ocupa una pagina) y no tiene preludio.
Luego tenemos las tareas que ejecutan codigo en c, que son compiladas con gcc y linkeadas con ld, estas pesan 8KB (2 paginas), y tienen un preludio (que puede estar escrito en c o en asm) que se encarga de ejecutar la funcion _start para que comience la tarea. 
Sabiendo esto, suponemos que la utilidad de definir 2 "tipos" de tareas es que nos permite gastar menos recursos en ciertas tareas si es que estas no lo precisan; si queremos codear una nueva tarea y no necesitamos utilizar las ventajas que nos da C (Como por ejemplo las 2 paginas) podemos hacerla en asm y que consuma menos.
Para ejecutar una tarea distinta, seria cambiar la tarea a ejecutar en el lugar en la que se la define. 

**19.** Mirando la tarea *Pong*, ¿En que posición de memoria escribe
esta tarea el puntaje que queremos imprimir? ¿Cómo funciona el mecanismo
propuesto para compartir datos entre tareas?

El puntaje de la partida se escribe en la posicion **PAGE_ON_DEMAND_BASE_VADDR + 0xF00**
Existe un puntero hacia el espacio de memoria compartido al que tienen acceso todas las tareas de usuario, gracias a este es posible la comunicacion entre tareas a travez de lecturas/escrituras.

#### Programando:

**20.** Completen el código de la tarea *PongScoreboard* para que
imprima en la pantalla el puntaje de todas las instancias de *Pong* usando los datos que nos dejan en la página compartida.

**21.** \[Opcional\] Resuman con su equipo todas las estructuras vistas
desde el Taller 1 al Taller 4. Escriban el funcionamiento general de
segmentos, interrupciones, paginación y tareas en los procesadores
Intel. ¿Cómo interactúan las estructuras? ¿Qué configuraciones son
fundamentales a realizar? ¿Cómo son los niveles de privilegio y acceso a
las estructuras?

**22.** \[Opcional\] ¿Qué pasa cuando una tarea dispara una
excepción? ¿Cómo podría mejorarse la respuesta del sistema ante estos
eventos?
