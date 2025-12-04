# Explicacion de Malloco

## Cosas que podemos ir pensando 

- Modificar `idt_init()` para agregar nuevas idt_entrys para 2 syscalls en alguno de los codigos de interrupcion disponibles

- Modificar `isr.asm` para codear una rutina de atencion para el codigo de interrupcion de la syscall malloco y chau

- Modificar el page_fault_handler en `mmu.c` para que si el estado de la reserva de memoria esta en 1 (Activa), se mapee la direccion fisica. Cualquier otro caso se trata de un acceso incorrecto debemos desalojar la tarea inmediatamente yy aseguyrarse de que no vuelva a correr. 

## Ejercicio 1 

## Modificacion de idt.c 

Necesitamos modificar el archivo **idt.c** exactamente en la funcion `idt_init()`. La modificacion consiste en agregar dos nuevas entradas a la idt de *nivel de prioridad usuario* (nivel 3), ya que seran syscalls utilizadas por usuario para invocar a `malloco(size_t size)` y a `chau(virtaddr_t virt)`. Los numeros que les asignamos son 90 y 91 respectivamente. 

```c
void idt_init() {
  // Excepciones
  IDT_ENTRY0(0);
  IDT_ENTRY0(1);
  IDT_ENTRY0(2);
  IDT_ENTRY0(3);
  IDT_ENTRY0(4);
  IDT_ENTRY0(5);
  IDT_ENTRY0(6);
  IDT_ENTRY0(7);
  IDT_ENTRY0(8);
  IDT_ENTRY0(9);
  IDT_ENTRY0(10);
  IDT_ENTRY0(11);
  IDT_ENTRY0(12);
  IDT_ENTRY0(13);
  IDT_ENTRY0(14);
  IDT_ENTRY0(15);
  IDT_ENTRY0(16);
  IDT_ENTRY0(17);
  IDT_ENTRY0(18);
  IDT_ENTRY0(19);
  IDT_ENTRY0(20);
  // Reloj Teclado
  IDT_ENTRY0(32);
  IDT_ENTRY0(33); 
  //Syscalls
  IDT_ENTRY3(88); 
  IDT_ENTRY3(90); // syscall para malloco
  IDT_ENTRY3(91); // syscall para chau 🫡
  IDT_ENTRY3(98);
}
```

## Modificacion de isr.asm
Ahora toca agregar las interrupciones 90 y 91 definidas en `idt_init()` en `isr.asm`. 
- Para la 90 llamamos a la funcion malloco y nos guardaos en la pila el valor de eax, para que a la hora de hacer popad no perdamos el retorno que queremos
- Para la 91 llamamos a la funcion chau para marcar la reserva para que sea liberada

*Modificacion de codigo en isr.asm*

```x86asm
%define offset_EAX 28 ;Esto esta en el tp

global _isr90

_isr90: ;<-- La interrupcion es malloco en si
  pushad
  
  push eax ;El size se pasa por eax
  call malloco
  add esp, 4 
  mov DWORD[esp + offset_EAX], eax  

  popad
  iret

_isr91: ;<-- La interrupcion es chau en si
  pushad
  
  push eax ;El size se pasa por eax 
  call chau
  add esp, 4

  popad
  iret
```

## Modificacion del mmu.c 
Ahora debemos modificar la funcion `page_fault_handler()` 
Si la memoria es reservada por malloco, debemos mapearlo a la memoria fisica. 
Caso contrario accedemos a las reservas de la tarea actual, las marcamos para liberar, cortamos la ejecucion de la tarea y pasamos a la siguiente. 

Ahora necesitamos modificar el archivo **mmu.c** especificamente en la funcion `page_fault_handler()`, lo que ya estaba programado en esta funcion (mappeo de la memoria on_demand en caso de que no este mappeada) queda de la misma manera. Simplemente agregamos un caso extra para manejar las reservas de malloco. 
Si la reserva de malloco ya esta reservada, mappeamos a la memoria fisica lo que se pidio con la syscall. 
Caso contrario; liberamos todas las reservas de la tarea con la syscall chau, marcamos a la entrada de la tarea en el scheduler como SLOT_FREE y pasamos a la siguiente tarea.  

```c
bool page_fault_handler(vaddr_t virt) {
  
  if ((ON_DEMAND_MEM_START_VIRTUAL <= virt) && (virt <= ON_DEMAND_MEM_END_VIRTUAL)) {
    print("Atendiendo page fault...", 0, 0, C_FG_WHITE | C_BG_BLACK);
    uint32_t cr3 = rcr3();
    mmu_map_page(cr3, virt, (paddr_t)ON_DEMAND_MEM_START_PHYSICAL, MMU_U | MMU_W | MMU_P);

    return true;
  }else{
    if (esMemoriaReservada(virt)) {
        mmu_map_page(rcr3(), virt, mmu_next_free_user_page(), MMU_P | MMU_W | MMU_U);
    } else {
        
        reservas_por_tarea* reservas = dameReservas(current_task); //Pedimos las reservas de la tarea a desalojar
        uint32_t len = reservas->reservas_size; //Nos guardamos el len para recorrer el arreglo de reservas
        reserva_t* arr_reservas = reservas->array_reservas; 

        for (i=0; i < len ;i++) {
          chau(arr_reservas[i].virt); //Marcamos para liberrar a cada dir. virt del arreglo
        }

        sched_tasks[current_task].state = TASK_SLOT_FREE; //Marcamos a la tarea como slot 
        sched_next_task(); //pasamos a la siguiente tarea, ESTO ESTA MAL PQ NUNCA HACEMOS UN JMP FAR (PERO ME DA MUCHA PAJA ARREGLARLO :D)
    }

    return true; 
  }
  return false; 
}
```

## Ejercicio 2

Para poder ejecutar a ``garbage_collector`` cada 100 ticks, podriamos crear una funcion aux en ``C``, con nombre `ver_garbage()`, que sea llamada desde la ``interrupcion de clock``:

Primero llamamos a ver_garbage desde la interrupcion, esta devolvera un valor booleano en dependiendo de si los ticks actuales son o no multiplo de 100.
En caso de que el resultado devuelto sea `TRUE`, saltamos a la etiqueta `.garbage`.
En esta etiqueta cargaremos el selector de la tarea `garbage_collector` para asi poder realizar el JMP FAR correspondiente. 

> Aclaremos que, para evitar que esta tarea sea invocada cuando los ticks no sean multiplos de 100, este selector no estara presente en el arreglo provisto por el scheduler. Se manejara por fuera del mismo...

> Asumamos que garbage_collector es la TAREA_B en mi makefile.

```x86asm
global _isr32
  
_isr32:
  pushad
  call pic_finish1
  
  call ver_garbage
  test eax, eax
  jne .garbage

  call sched_next_task
  
  str cx
  cmp ax, cx
  je .fin
  
  mov word [sched_task_selector], ax
  jmp far [sched_task_offset]
  jmp .fin

  .garbage:
    mov ax, GDT_TASK_B_SEL
    mov word [sched_task_selector], ax
    jmp far [sched_task_offset]


  .fin:
  call tasks_tick
  call tasks_screen_update
  popad
  iret
```

```c
bool ver_garbage(){
  return (ENVIROMENT->tick_count % 100 == 0)
}
```

Por el lado de la tarea ```garbage_collector```, una posible implementacion que se me ocurre es:

<!-- garbage_collector que recorrerá continuamente las reservas de las tareas en busca de reservas marcadas para liberar.-->

Esta implementacion estaria ubicada en un hipotetico archivo `taskGarbageCollector.c`.

`task` vendria a ser el main de nuestra tarea, que se encarga de ejecutarla.

La tarea consiste de un for que recorre las diferentes tareas, solicitando sus reservas para verificar cuales estan marcadas para liberar.
Aquellas que esten marcadas seran unmapeadas y su estado actualizado a liberadas.

```c
void task(void){
  garbage_collector();
}


void garbage_collector(){
  for(i=0, i < MAX_TASKS, i++){ //Recorro todas las tareas
    reservas_por_tarea_t* reservas = dameReservas(i); //Pido las reservas de la iesima tarea

    uint32_t len = reservas->reservas_size; //Nos guardamos el len para recorrer el arreglo de reservas
    reserva_t* arr_reservas = reservas->array_reservas; //Obtengo el arreglo de reservas

    for (i=0; i < len ;i++) { //Recorro todas las reservas
      if(arr_reservas[i].estado == 2){
        mmu_unmap_page(task_selector_to_cr3(sched_task[i].selector), arr_reservas[i].virt);

        arr_reservas[i].estado == 3;
      }
    }

  }
}
```

![LET IT RIDE](letitride.gif)

## Ejericio 3

## a)

Podriamos incluirlo en `sched.c`, con el nombre `reservas_de_tareas`. Ya que el scheduler es el que se encarga de recorrer las tareas que tengamos en el sistema. 

## b)

*Implementacion de malloco en c* 

Por comodidad, modificamos el struct `reservas_por_tarea`. Agregamos un campo `actual_virt_addr_base` que se incrementa cada vez que solicitamos espacio.

```c
uint32_t next_free_virtual = 0xA10C0000; 

void* malloco(size_t size){
    reservas_por_tarea_t* reservas = dameReservas(current_task); //Pido las reservas de la iesima tarea
    reserva_t* reservas_de_tarea = reservas->array_reservas;
    uint32_t memoria_ocupada_por_tarea = calcular_memoria_ocupada_tarea_actual(reservas_de_tarea) + (uint32_t) size;

    if(memoria_ocupada_por_tarea > 4000000) return NULL; 

    uint32_t ultima_reserva = reservas->reservas_size; 
    reservas_de_tarea[ultima_reserva] = (reserva_t) 
    {
      .virt = next_free_virtual_address(reservas, size),
      .tamanio = size,
      .estado = 1,
    }; 

    return reservas_de_tarea[ultima_reserva].virt; 
    
    

}

uint32_t calcular_memoria_ocupada_tarea_actual(reserva_t* reservas_de_tarea){
    uint32_t len = reservas->reservas_size; 
    uint32_t acumulador_memoria_reservada = 0; 

    for (int i = 0; i < len; i++){
      acumulador_memoria_reservada += reservas_de_tarea[i].tamanio; 
    }
    
    return acumulador_memoria_reservada; 
}

uint32_t next_free_virtual_address(reservas_por_tarea_t* reservas,size_t size){
    uint32_t actual_virtual_address = reservas->actual_virt_addr_base;
    reservas->actual_virt_addr_base += size;
    return actual_virtual_address;
}
```

## c)

```c
void chau(virtaddr_t virt){
    reservas_por_tarea_t* reservas = dameReservas(current_task); 
    reserva_t* reservas_de_tarea = reservas->array_reservas;
    uint32_t len = reservas->reservas_size;  
    for(int i = 0; i < len; i++){
      if(reservas_de_tarea[i].virt == virt){
          reservas_de_tarea[i].estado = 2; 
      }
    }
}
```