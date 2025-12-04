# Parte 2 - Respuestas Teoricas

### Primera parte: Definiendo la IDT

1 -
- a) Campos de la IDT_ENTRY0: 
    - offset_15_0 y offset_31_16 corresponden a la parte baja y parte alta de la dirección del entry point de la rutina que manejará la interrupción.
    - segsel (Segment Selector) es un selector de 16 bits que apunta a una entrada de la GDT o LDT, esta debe ser un segmento de código.
    - present (Present Flag) indica si un descriptor es válido o no.
    - dpl (Descriptor Privilege Level) son 2 bits que definen el nivel de privilegio del segmento. 
    - type indica si un segmento es de Interrupt, Trap o Task.

- b) Completado en [<u>*_idt.c_*</u>](src/idt.c#L48-L56) 

    - Teniendo en cuenta que en la consigna nos dicen que toda rutina de interrupcion debe correr en el nivel del kernel, el segsel tendria que ser uno de Nivel 0. Por ende usamos GDT_CODE_0_SEL que ya definimos en [<u>*_defines.h_*</u>](src/defines.h#L47)

    - Si usamos un size de 32 bits, tenemos que marcar como "1" el bit D del campo Type. De otro modo, estaria teniendo un size de 16 bits.

- c) Resuelto en [<u>*_idt.c_*</u>](src/idt.c#L62-L70)

2 - Resuelto en [<u>*_kernel.asm_*</u>](src/kernel.asm#L147-L149)

- Utilizamos la macro IDT_ENTRY0 con 32 y 33 como número. Pues la interrupcion de reloj y teclado son de Nivel 0.

### Segunda parte: Rutinas de Atención de Interrupción

3 - Resuelto en [<u>*_isr.asm_*</u>](src/isr.asm#L151-L172)

- pushad y popad funcionan como prólogo y epílogo respectivamente. Utilizamos iret para poder sacar EFLAGS, CS, EIP y en caso de ser necesario el ERR. 

4 - Resuelto en [<u>*_isr.asm_*</u>](src/isr.asm#L178-L192)

5 - Resuelto en [<u>*_isr.asm_*</u>](src/isr.asm#L202-L222)

6 - Resuelto en [<u>*_kernel.asm_*</u>](src/kernel.asm#L156-L160)

7 - El reloj funciona de la siguiente manera: 
    En un espacio de memoria (al que llamamos isrNumber) almacenamos un 0, lo vamos a usar como contador por eso comienza en 0. Luego en otro espacio de memoria (al que llamamos isrClock) almacenamos en una cadena de caracteres cada frame que tendria el reloj. 
    Con estos dos valores guardados en memoria definimos una funcion, llamada next_clock, que incremente en 1 el isrNumber y lo compare con la longitud de isrClock. Si isrNumber es menor imprimimos el caracter de la posicion isrNumber, ahora si no es menor, reseteamos isrNumber a 0. 
    Como next_clock se ejecuta en la rutina de _isr32 la cual se dispara constantemente, esto funciona como un ciclo que no corta nunca, entonces por eso vemos que el reloj gira sin parar. 