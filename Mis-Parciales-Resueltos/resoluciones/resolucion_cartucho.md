- Se copian los graficos del cartucho a un buffer de 4kb (4000) 
- Cuando el buffer se llena, se llama una interrupcion a la irq40
- Hay varios mecanismos para que las tareas accedan al buffer de video:
    * DMA (Direct Memory Access): se mapea la direccion virt 0xBABAB000 del espacio de direcciones de la tarea directamente al buffer de video 
    * Por copia: se realiza una copia del buffer en una fisica especifica y se mapea en una direccion virtual provista por la tarea. Cada tarea debe tener una copia unica
- El buffer se encuentra en la phy = 0xF151C000 y solo debe ser modificable por el lector de cartuchos

Las tareas piden acceso al buffer mediante la syscall opendevice. El acceso del buffer se configura en la direccion virtual 0xACCE5000 (mapeada como r&w).
Alli se almacena un estado de acceso de 0 (no puede acceder al buffer), 1 (accede con DMA) y 2 (accede por copia). (uint8_t acceso)
De acceder por copia, la direccion virtual donde realizar la copia estara dada por el valor del registro ECX al momento de llamar a open device, y sus permisos seran r&w. Asumimos que las tareas tienen esa direccion virtual mapeada a alguna direccion fisica.

- Las tareas no pueden retomar ejecucion hasta que este listo el buffer y se haya hecho la copia o DMA. 
- Cuando la tarea termina de utilizar el buffer, usa la syscall closedevice. Se debe retirar el acceso al buffer por DMA o dejar de actualizar la copia.
- Interrupcion de buffer completo informa a las tareas que esta listo, actualiza las copias 
- Cada tarea mantiene una unica copia del buffer

Entonces tenemos:

Las tareas acceden a un buffer de video con un acceso especifico (BUFFER_ACCESS). Opendevice se encarga de solicitar el buffer por la tarea. Hay que dividir casos segun BUFFER_ACCESS.

Si BUFFER_ACCESS es 0, retornamos.
Si BUFFER_ACCESS es 1, se mapea la direccion virtual 0xBABAB000 del espacio de direcciones de la tarea directamente al buffer de video (phy = 0xF151C000).
Si BUFFER_ACCESS es 2, agarramos ECX, traducimos a fisica y copiamos del buffer a la fisica.

Para deviceready(), debemos tener en cuenta que todas las tareas tienen un acceso especifico. Por ello, dividiremos en casos como lo dijimos anteriormente.

Ya tenemos todo listo para implementar deviceready().

```c
#define BUFFER_PHY 0xF151C000
#define BUFFER_VIRT 0xBABAB000
#define ACCESO 0xACCE5000

Agrego al scheduler

// NO tiene sentido en C, solamente es semantica para decir que añado dos campos
sched_entry_t = sched_entry_t | uint8_t buffer_access | vaddr_t virtual_address;

ejercicio para mañana: hacer todas las auxiliares
void deviceready() {
    for (size_t i = 0; i<MAX_TASKS; i++) {
        uint8_t acceso = get_acceso(i);

        if (acceso == 0) {
            continue;
        }

        uint32_t task_cr3 = tss_tasks[i].cr3;

        if (acceso == 1) 
        {
            mmu_map_page(task_cr3, BUFFER_VIRT, BUFFER_PHY, MMU_P | MMU_U);
        }
        else if (acceso == 2) 
        {
            // Agarro la virtual que ya viene dada
            uint32_t virtual_to_copy = sched_tasks[i].virtual_address;
            
            // Me fijo si esta mapeada 

            if (mmu_is_mapped(task_cr3, virtual_to_copy))
            {
                // Actualizo la copia 
                copy_page(virt_to_phy(task_cr3,virtual_to_copy), BUFFER_PHY);
            }
            else {
                // Mapeo 
                paddr_t free = mmu_next_free_user_page();

                mmu_map_page(task_cr3, virtual_to_copy, free, MMU_P | MMU_U);
                copy_page(free, BUFFER_PHY);
            }
        }

        sched_tasks[i].state = TASK_RUNNABLE;
    }
}

uint8_t opendevice(uint32_t copyDir){
 sched_tasks[current_task].status = TASK_BLOCKED;
 sched_tasks[current_task].buffer_access = *(uint8_t*)0xACCE5000;
 sched_tasks[current_task].virtual_address = copyDir;
}

void closedevice() {
    sched_entry_t* current = &sched_tasks[current_task];
    
    if (current->buffer_access == 1) {
        mmu_unmap_page(rcr3(), BUFFER_VIRT);
    }
    else if (current->buffer_access == 2) {
        mmu_unmap_page(rcr3(), current->virtual_address);
    }

    current->buffer_access = 0;
    current->virtual_address = 0;
}

```

```x86
global _isr40

_isr40:
    pushad
    call pic_finish1

    call deviceready
.fin:
    popad
    iret
```

