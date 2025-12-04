## Ejercicio 1

Para poder definir las syscall `crear_pareja`,`juntarse_con` y `abandonar_pareja` primero debemos añadir las interrupciones que las invocarán:

> Las añado a la idt como entrys de lvl 3, pues las syscalls tienen que poder ser invocadas por tareas de nvl de usuario. Usare la 90, 91 y 92...

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
  IDT_ENTRY3(90); // syscall para crear_pareja
  IDT_ENTRY3(91); // syscall para juntarse_con
  IDT_ENTRY3(92); // syscall para abandonar_pareja
  IDT_ENTRY3(98);
}
```
Ahora añadimos codigo para estas 3 nuevas interrupciones:


```x86asm
%define offset_EAX 28 ;Esto esta en el tp

global _isr90

; Tanto la int 90 como 92 no reciben parametros, asi que simplemente llamo a las respectivas funciones de C

_isr90: 
  pushad
  
  call crear_pareja

  popad
  iret


global _isr91

; La int 91 en cambio recibe la id_tarea a la que se quiere juntar el invocador de la int. No me aclaran por que registro llega este parametro, asi que de momento asumire que es por EDI. Lo cargo en el stack para que llegue como parametro a la funcion "juntarse_con"

_isr91: 
  pushad
  
  push edi
  call juntarse_con
  add esp, 4

; "juntarse_con" devuelve un int en eax, actualizo el stack para evitar que el popad lo sobrescriba.
  mov DWORD[esp + offset_EAX], eax

  popad
  iret


global _isr92

_isr92: 
  pushad
  
  call abandonar_pareja

  popad
  iret
```

Tambien tendriamos que hacer unas modificaciones al scheduler:

- Primero, agregare 2 nuevos estados a `task_state_t`:

    - `TASK_WAITING_JOINING_BUDDY`: Indica que la tarea solicito crear una pareja, por ende le ponemos este estado para indicar que no debe ejecutarse hasta que encuentra un "Buddy".

    - `TASK_WAITING_LEAVING_BUDDY`: Indica que la tarea solicito disolver la pareja, y por ende no se vuelve a ejecutar hasta que el "Buddy" abandone.

```c
typedef enum {
  TASK_SLOT_FREE,
  TASK_RUNNABLE,
  TASK_PAUSED
  TASK_WAITING_JOINING_BUDDY //La tarea esta esperando a que se le una otra
  TASK_WAITING_LEAVING_BUDDY //La tarea esta esperando a que la pareja abandone
} task_state_t;
```

- Segundo, modificare el `shed_entry_t`:

    - `lider` sera un booleano que indica si esta tarea es la lider de la pareja o no.

    - `buddy_task_id` sera un `uint8_t` en el que almacenaré la id del compañero de pareja de la tarea. En caso de no tener compañero, tendra el valor `-1` (Modifico `shed_init` para que este valor se aplique por defecto).

    - `direcciones_mapeadas` sera un arreglo de direcciones virtuales mapeadas en el espacio de memoria de 4MB. Este lo utilizare para poder unmapear cuando la pareja se separe y no sea lider.

    - `cant_direcciones_mapeadas` sera la longitud de este arreglo.

```c
typedef struct {
  int16_t selector;
  task_state_t state;
  bool lider;
  int8_t buddy_task_id;
  vaddr_t direcciones_mapeadas[1024]; //Utilizo el tipo de dato definido en el tp
  uint32_t cant_direcciones_mapeadas;
} sched_entry_t;

void sched_init(void) {
  for (size_t i = 0; i < MAX_TASKS; i++) {
    sched_tasks[i].state = TASK_SLOT_FREE;
    sched_tasks[i].buddy_task_id = -1;
  }
}
```

- Por ultimo, modificare `sched_enable_task`:
    - Le agrego un kassert para que no pueda resumierse una tarea que este esperando a que se le una un compañero o que el compañero abandone.

```c
void sched_enable_task(int8_t task_id) {
  kassert(task_id >= 0 && task_id < MAX_TASKS && , "Invalid task_id");
  kassert(sched_task[task_id].state != TASK_WAITING_JOINING_BUDDY && sched_task[task_id].state != TASK_WAITING_LEAVING_BUDDY, "Task is not ready to resume yet... (- Waiting for partner to join or leave :P -)");
  sched_tasks[task_id].state = TASK_RUNNABLE;
}
```

Ya teniendo todo esto definido, podemos pasar a implementar en C las 3 funciones pedidas.

### crear_pareja:

Simplemente preguntamos si la tarea actual no tiene pareja, y en ese caso le indicamos al sistema que esta lista para emparejarse.

```c
void crear_pareja(){
    if(pareja_de_actual() == 0){
        conformar_pareja(0);
        return;
    }
}
```


### juntarse_con

Para esta funcion alcanza con verificar que:

- La tarea actual no tenga pareja.

- La tarea pasada por parametro este aceptando pareja.

```c
int juntarse_con(int id_tarea){
    if(pareja_de_actual() == 0 && aceptando_pareja(id_tarea) == 1){
        conformar_pareja(id_tarea);

        return 0;
    }

    return 1;
}
```

Se me pide que una vez creada la pareja se tiene que garantizar que ambas tengan acceso a los 4MB a partir de `0xC0C00000`. Modifico `page_fault_handler` para que se encargue de esto:

```c
#define inicio_4MB_compartido 0xC0C00000


bool page_fault_handler(vaddr_t virt) {
  
  if (virt >= ON_DEMAND_MEM_START_VIRTUAL && virt <= ON_DEMAND_MEM_END_VIRTUAL) //Estoy en direccion de on demand
  {
    print("Atendiendo page fault...", 0, 0, C_FG_WHITE | C_BG_BLACK);
    //Traemos cr3
    uint32_t cr3 = rcr3(); 
    //Definimos la direccion fisica de la pagina ondemand
    paddr_t on_demand_start_addr = ON_DEMAND_MEM_START_PHYSICAL; 

    // Pongo todo en 0
    //zero_page(on_demand_start_addr);

    //Llamamos a map_page para que se encargue de verificar si esta o no mapeada
    mmu_map_page(cr3, virt, on_demand_start_addr, MMU_P | MMU_U | MMU_W);
    
    return true;     
  }

  task_id buddy_id = pareja_de_actual();
  if(virt >= inicio_4MB_compartido && virt < fin_4MB_compartido){ //Si la dir recibida esta dentro del espacio de 4MB
    if(buddy_id != 0){ //Y mi pareja actual tiene compañero
        sched_entry_t* tarea_actual = &sched_task[current_task];
        sched_entry_t* buddy_actual = &sched_task[tarea_actual->buddy_task_id];

        paddr_t phy = mmu_next_free_user_page();

        if(es_lider(current_task)){
            mmu_map_page(rcr3(), virt, phy, MMU_P | MMU_U | MMU_W); //Mapeo desde 0xC0C00000 con el cr3 de la lider, le doy permiso W/R.
            mmu_map_page(task_id_to_cr3(tarea_actual->buddy_task_id), virt, phy, MMU_P | MMU_U); // A la compañera le mapeo el mismo espacio con su respectivo cr3, pero solo le permito leer.
        } else {
            // Aqui hago lo mismo, solo que accedo de manera contraria, pues en este caso current_task no es la lider.
            mmu_map_page(rcr3(), virt, phy, MMU_P | MMU_U );
            mmu_map_page(task_id_to_cr3(tarea_actual.buddy_task_id), virt, phy, MMU_P | MMU_U | MMU_W);
        }
        
        //Añado la virt recien mapeada y aumento la longitud del arreglo
        tarea_actual->direcciones_mapeadas[cant_direcciones_mapeadas] = virt;
        tarea_actual->cant_direcciones_mapeadas += 1;

        buddy_actual->direcciones_mapeadas[cant_direcciones_mapeadas] = virt;
        buddy_actual->cant_direcciones_mapeadas += 1;
    }
  }
  return false; 
}
```

```c
uint32_t task_id_to_cr3(task_id id_tarea) {
  sched_entry_t* tarea = &sched_tasks[id_tarea]; 
  uint16_t task_selector = tarea->selector; 
  gdt_entry_t tss_descriptor = gdt[task_selector]; 
  tss_t* task_segment_state = (tss_descriptor.base_31_24 << 24) | (tss_descriptor.base_23_16 << 16)  | tss_descriptor.base_15_0; 
  return task_segment_state->cr3; 
}
```


### abandonar_pareja

En este caso nos alcanza con:

- Llamar a `romper_pareja()` en caso de que la current_task no sea lider.

- En caso de si ser lider, la marcamos como `TASK_WAITING_LEAVING_BUDDY`.

```c
void abandonar_pareja(){
    if(!(es_lider(current_task))){
        
        if(pareja_de_actual() != 0) return; //Si la tarea no pertenece a ninguna pareja: No pasa nada.


        //Caso contrario, toca desvincular
        sched_entry_t* tarea_actual = &sched_task[current_task];
        sched_entry_t* buddy_actual = &sched_task[tarea_actual->buddy_task_id];

        task_id buddy_id = tarea_actual->buddy_task_id;

        //Si la tarea pertenece a una pareja y no es su líder: La tarea abandona la pareja, por lo que pierde acceso al área de memoria especificada.
    
        for(i=0; i < tarea_actual->cant_direcciones_mapeadas;i++){
            //Unmapeo el espacio de 4MB de la tarea que no es lider
            mmu_unmap_page(rcr3(),tarea_actual->direcciones_mapeadas[i]);
        }
        //Reseteo la longitud del arreglo
        tarea_actual->cant_direcciones_mapeadas = 0;

        if(buddy_actual->state == TASK_WAITING_LEAVING_BUDDY){
            //Marco la ta
            buddy_actual->state = TASK_RUNNABLE;

            for(i=0; i < tarea_actual->cant_direcciones_mapeadas;i++){
                //Unmapeo el espacio de 4MB de la tarea lider
                mmu_unmap_page(task_id_to_cr3(buddy_id),buddy_actual->direcciones_mapeadas[i]);
            }
            //Reseteo la longitud del arreglo
            buddy_actual->cant_direcciones_mapeadas = 0;
        } 
        
        romper_pareja();

    } else {
        sched_tasks[current_task].state = TASK_WAITING_LEAVING_BUDDY;
    }
}
```

## Ejercicio 2

### conformar_pareja:

`conformar_pareja` funcionara de la siguiente manera:

- Primero que nada verifico si la tarea pasada por parametro es realmente una id o un 0:
    - Si es 0, significa que la `current_task` debe marcarse como "apta para emparejar". Por ende la marcamos como lider y le cambiamos el estado a `TASK_WAITING_JOINING_BUDDY`

    - Caso contrario; la pongo en estado `TASK_RUNNABLE` y vinculo las tareas entre sí. (No es necesario marcar la otra tarea como no lider, de esto se encarga por default romper_pareja).

```c
void conformar_pareja(task_id tarea){
    if(task_id == 0){
        sched_tasks[current_task].lider = TRUE;
        sched_tasks[current_task].state = TASK_WAITING_JOINING_BUDDY;
    } else {
        sched_tasks[tarea].state = TASK_RUNNABLE;

        sched_tasks[tarea].buddy_task_id = current_task;
        sched_task[current_task].buddy_task_id = tarea;
    }
}
```

### romper_pareja:

```c
void romper_pareja(){
    if(pareja_de_actual() != 0) return; //Si la tarea no pertenece a ninguna pareja: No pasa nada.
    //Desvinculo las tareas entre si
    tarea_actual->buddy_task_id = -1;
    buddy_actual->buddy_task_id = -1;
    //Le quito el status de lider a la tarea
    buddy_actual->lider = FALSE;
}
```

## Ejercicio 3

### pareja_de_actual():

Me creo una variable que almacena el id de tarea de la pareja de `current_task`.
-   Si `id == -1`, entonces no tiene pareja y por ende devuelvo 0.
-   Caso contrario, devuelvo la id guardada.

```c
task_id pareja_de_actual(){
    int8_t buddy_id = sched_tasks[current_task].buddy_task_id;

    if (buddy_id == -1) return 0;

    return buddy_id;
}
```


### es_lider(task_id tarea):

Simplemente devuelvo el bool de lider que cree en mi struct.

```c
bool es_lider(task_id tarea){
    return sched_tasks[current_task].lider
}
```

### aceptando_pareja(task_id tarea):

Si el id del compañero esta seteado en `-1`, significa que no tiene pareja.

```c
bool aceptando_pareja(task_id tarea){
    return (sched_tasks[tarea].buddy_task_id == -1);
}
```


