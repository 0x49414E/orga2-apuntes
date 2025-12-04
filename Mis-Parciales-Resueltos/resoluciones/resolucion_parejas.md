-- todas las tareas pueden tener 0 o 1 parejas 
-- todas las tareas pueden abandonar las parejas 
-- todas las tareas pueden formar parejas las veces que quieran, pero siempre tienen una sola
-- las parejas comparten 4mb de memoria despues de 0xC0C00000 (osea, 0xC0C00000 + 0x4000000). La region fisica es la misma para ambas
-- la region anteriormente mencionada se asigna bajo demanda (lazy loading) 
-- solo la tarea que crea la pareja puede escribir en la memoria compartida
-- cuando una tarea abandona a su pareja pierde los 4mb

`task_id pareja_de_actual()`: si la tarea actual está en pareja devuelve el task_id de su pareja, o devuelve 0 si la tarea actual no está en pareja.
`bool es_lider(task_id tarea)`: indica si la tarea pasada por parámetro es lider o no.
`bool aceptando_pareja(task_id tarea)`: si la tarea pasada por parámetro está en un estado que le permita formar pareja devuelve 1, si no devuelve 0.
`void conformar_pareja(task_id tarea)`: informa al sistema que la tarea actual y la pasada por parámetro deben ser emparejadas. Al pasar 0 por parámetro, se indica al sistema que la tarea actual está disponible para ser emparejada.
`void romper_pareja()`: indica al sistema que la tarea actual ya no pertenece a su pareja actual. Si la tarea actual no estaba en pareja, no tiene efecto.

1. (50 pts) Definir el mecanismo por el cual las syscall crear_pareja, juntarse_con y abandonar_pareja recibirán sus parámetros y retornarán sus resultados según corresponda. Dar una implementación para cada una de las syscalls. Explicitar las modificaciones al kernel que sea necesario realizar, como pueden ser estructuras o funciones auxiliares.

Se que current_task refiere al indice de la tarea actual en la tss y en el sched_tasks. Si la tarea actual ya pertenece a una pareja, entonces debo retornar. Sino, la tarea debe quedar esperando hasta que se una otra tarea. La tarea que crea la pareja es la lider y es la unica que puede escribir sobre la memoria compartida.

Para este ejercicio, necesitare modificar el scheduler para que las tareas tengan un estado mas (que sera esperando IO de las otras tareas). Ademas debemos saber si es lider o no, y su pareja correspondiente. Podemos ir aniadiendo los campos y usamos las funciones que ya teniamos implementadas.

```c
typedef enum {
  TASK_SLOT_FREE,
  TASK_RUNNABLE,
  TASK_PAUSED,
  TASK_ESPERANDO_PAREJA,
  TASK_BLOCKED
} task_state_t;

typedef struct {
  int16_t selector;
  task_state_t state;
  bool leader;
  int8_t pareja;
} sched_entry_t;

bool crear_pareja_internal() {
    if (pareja_de_actual()) return true;

    sched_tasks[current_task].leader = true;    
    conformar_pareja(0);

    return false;
}

```

Aqui estamos asumiendo que conformar_pareja nos pone el estado de esperando pareja y ademas nos pone en 0 la pareja de la tarea actual.

Para juntarse_con(id_tarea) nos fijamos lo siguiente:
- Si ya tiene una pareja, retorna 1
- Si no estaba buscando pareja, retorna 1
- Si no, se conforma una pareja en la id_tarea es la lider. Retorna 0

- Una vez creada la tarea, AMBAS DEBEN TENER ACCESO A LOS 4MB a partri de 0xCOCOOOO a medida que lo requieren. Sólo la líder podrá escribir y ambas podrán leer. Esto requiere mapear a una como R&W y a otra como solo R. 


```c
int juntarse_con(int id_tarea) {
    if (pareja_de_actual() || !aceptando_pareja(id_tarea)) return 1;

    conformar_pareja(id_tarea);
    sched_tasks[id_tarea].leader = true;
    return 0;
}
```

Para asegurarnos de que puedan acceder a memoria a medida que lo necesiten, modificamos el page fault handler para ver el acceso a memoria compartida. Para ello, debemos verificar que:
1- Este en el rango de memoria compartida virtual
2- Si esta, nos fijamos que tenga pareja
3- Si tiene pareja, nos fijamos cual es la lider. La lider tendra los atributos de presente, usuario y write. A la que no es lider, le ponemos los atributos de presente y usuario nada mas, ya que no puede escribir.

```c 
#define SHARED_REGION_BASE 0xC0C00000
#define FOUR_MB 0X4000000

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

  // Nueva logica para acceso on-demand para las tareas que esten en pareja 

  if (virt >= SHARED_REGION_BASE && virt <= SHARED_REGION_BASE + FOUR_MB) {
    // Aqui ya estoy en la seccion de memoria shared para las tareas. Chequeo que tenga pareja realmente
    if (!tiene_pareja(current_task)) return false;
    
    paddr_t physical_shared = mmu_next_free_user_page();

    if (es_lider(current_task)) {
        // mapeo la virtual con la proxima fisica para la pareja con R y la mia con R&W

        // Asumo que, como es la misma fisica, debo mapear ambas al mismo tiempo por simplicidad
        mmu_map_page(rcr3(), virt, physical_shared, MMU_P | MMU_W | MMU_U);
        mmu_map_page(tss_tasks[pareja_de_actual()].cr3, virt, physical_shared, MMU_P | MMU_U);
    }
    else {
        mmu_map_page(rcr3(), virt, physical_shared, MMU_P | MMU_W);
        mmu_map_page(tss_tasks[pareja_de_actual()].cr3, virt, physical_shared, MMU_P | MMU_U | MMU_W);
    }

    return true;
  }
  
  return false; 
}

void abandonar_pareja() {
    // caso 1: no tiene pareja, no pasa nada
    if (!pareja_de_actual()) return;

    // caso 2: si pertenece y no es lider, abandona la pareja y pierde el acceso a la memoria especificada
    if (!es_lider(current_task)) {
        if (estado_lider(current_task) == TASK_BLOCKED) {
            unmap_all_shared(pareja_de_actual());
            set_estado_tarea(pareja_de_actual(), TASK_RUNNABLE);
        }
        unmap_all_shared(current_task);
        sched_tasks[pareja_de_actual()].leader = 0;
        romper_pareja();
        return;
    }

    // caso 3: es lider, queda bloqueada hasta que la otra pareja abandone. Luego pierde el acceso al area de memoria.
    // Si la otra pareja es 0, significa que estoy solo en pareja.
    // Caso contrario, seteo en bloqueada hasta que la otra tarea abandone
    if (sched_tasks[current_task].pareja == 0) {
        unmap_all_shared(current_task);
    }
    else {
        set_estado_tarea(current_task, TASK_BLOCKED);
    }
    return;
}```

Definimos las interrupciones 

IDT_ENTRY(90)
IDT_ENTRY(91)
IDT_ENTRY(92)

x86

global _isr90:
  pushad
  call crear_pareja_internal

  cmp eax, 0
  jnz .fin

  mov [sched_task_selector], ax 
  jmp far [sched_task_offset]

.fin:
  popad
  iret

global _isr91:
  pushad
  push eax
  call juntarse_con

  cmp eax, 0
  jz .fin
  mov [esp + eax_OFFSET], 1

.fin:
  popad
  iret

global _isr92:
  pushad
  call abandonar_pareja
  popad
  iret

# 2 (20 pts) Implementar conformar_pareja(task_id tarea) y romper_pareja()

Ahora procederemos a definir e implementar conformar_pareja y romper_pareja

La semantica de conformar_pareja sera la siguiente:

Si la task_id es 0, entonces pongo a la tarea actual con el estado de esperando pareja. Si la task_id != 0, pongo a la tarea actual con la pareja que viene por parametro, ambas runnables ahora.
```c
void conformar_pareja(task_id tarea) {
    if (tarea == 0) {
      set_estado_tarea(current_task, TASK_ESPERANDO_PAREJA);
    }
    else {
      set_estado_tarea(current_task, TASK_RUNNABLE);
      set_estado_tarea(tarea, TASK_RUNNABLE);
    }
}

bool es_lider(task_id tarea) {
  return sched_tasks[tarea].leader;
}
 // romper pareja indica que la tarea abandona la pareja. Si es lider, se bloquea. Si no es lider, vuelve a runnable
void romper_pareja() {
    if (es_lider(current_task)) {
      set_estado_tarea(current_task, TASK_BLOCKED);
    }
    set_pareja(current_task, 0);
}

int pareja_de_actual(task_id tarea) {
  return sched_tasks[tarea].pareja;
}


```

