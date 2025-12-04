# 1

Primeramente, debemos modificar idt_init para que tome la nueva interrupcion para la syscall de malloco.


IDT_ENTRY3(90);

_isr90:
    pushad 
    push eax
    ; hay que ver como debería ser esto 
    call malloco
    add esp, 4

    popad
    iret

Tambien debemos cambiar el page fault handler para que, cuando accedamos a una memoria no mapeada y reservada, la mapeemos.

Si la memoria no es reservada, la debemos desalojar del kernel y liberar la memoria reservada (esto quiere decir que tenemos que eliminarla de la TSS y del array de tasks, y ademas marcar como para liberar las direcciones virtuales correspondientes)

```c
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

  // Nueva lógica para la lazy allocation
  // consultar si es de la actual, asumo esto
  if (esMemoriaReservada(virt)) {
    paddr_t phy = mmu_next_free_user_page();
    mmu_map_page(rcr3(), virt, phy, MMU_P | MMU_U | MMU_W);
    zero_page(phy);
    return true;
  }
  
  desalojar_tarea();
  
  return false; 
}

void desalojar_tarea() {
    // Desalojo de la tss
    tss_tasks[current_task] = {0}; // Pongo todo en 0
    sched_tasks[current_task] = {0};
    
    reservas_por_tarea* reservas_tarea_actual = dameReservas(current_task);

    uint32_t cantidad_reservas = reservas_tarea_actual->reservas_size;
    reserva_t* array_reservas = reservas_tarea_actual->array_reservas;

    for (size_t i = 0; i<cantidad_reservas; i++) {
        chau(array_reservas[i].virt);
    }
}

```

Ahora debemos de modificar la interrupcion del page fault handler

```x86
_isr14:
  pushad

  ; Obtengo la direccion a la se intento acceder
  mov edi, cr2

  push edi 
  
  ;Llamo al page_fault_handler que verifica si el page fault
  ;se dio en el area compartida.
  call page_fault_handler 

  pop edi 

  
  ; Si eax != 0 entonces el page_fault fue en el area compartida
  test eax, eax
  jnz .fin

  ;Caso contrario, entra aca...
  .ring0_exception:
    call sched_next_task 
    mov word [sched_task_selector], ax ; voy a la proxima tarea con el selector que me devuelve el sched_task_selector
    jmp far [sched_task_offset]
    jmp $
  
  .fin:
    popad
	  add esp, 4 ; error code
    ;mov DWORD[ebp + offset_ESP], esp
	  iret
```

Con esto implementado, ya el usuario podria pedir memoria tranquilamente, ya que accederia al page fault en caso de que este reservada y no mapeada, y sino la desalojaria automaticamente.

# 2 

Necesitamos ver que el tick_count sea multiplo de 100. Si es multiplo de 100, hacemos correr a la tarea del garbage collector. SIno, ejecutamos sched_next_task 

global _isr32
  
_isr32:
  pushad
  call pic_finish1

  call tick_count_es_multiplo_de_100

  cmp eax, 0
  jnz .gc
  
  call sched_next_task
  
  str cx
  cmp ax, cx
  je .fin
  
  mov word [sched_task_selector], ax
  jmp far [sched_task_offset]

  .gc:
  mov ax, GARBAGE_COLLECTOR_SELECTOR
  mov word [sched_task_selector], ax
  jmp far [sched_task_offset]
  
  .fin:
  call tasks_tick
  call tasks_screen_update
  popad
  iret

# 3 

a) scheduler 

```c 
typedef struct {
  int16_t selector;
  task_state_t state;
  reservas_por_tarea* reservas_de_tarea; // NUEVO
} sched_entry_t;
```

b) 


```c

#define MAX_RESERVAS_SIZE 4000000
#define START_AREA_RESERVABLE 0xA10C0000

void* malloco(size_t size) 
{
    sched_entry_t* tarea_actual = &sched_tasks[current_task];

    reservas_por_tarea* reservas = tarea_actual->reservas_de_tarea;

    if (reservas->reservas_size * PAGE_SIZE == MAX_RESERVAS_SIZE) {
        return NULL;
    }

    reserva_t nueva_reserva = (reserva_t) {
        virt = next_free_virtual_page(tarea_actual),
        tamanio = size,
        estado = 1
    };

    reservas->array_reservas[reservas->reservas_size - 1] = nueva_reserva;
}

void chau(virtaddr_t virt) {
    uint32_t cr3 = rcr3();

    sched_entry_t* tarea_actual = &sched_tasks[current_task];

    reservas_por_tarea* reservas = tarea_actual->reservas_de_tarea;

    if (esMemoriaReservada(virt)) {
        for (size_t i = 0; i < reservas->reservas_size; i++) 
        {
            if (virt == reservas->array_reservas[i].virt) {
                reservas->array_reservas[i].estado = 3;
            }
        }
    }
}
```