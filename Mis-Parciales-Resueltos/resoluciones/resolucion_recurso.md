# Resolucion recursos

## Primera parte: Solicitar y recursos listos
Tenemos que poder controlar procesos de produccion entre tareas. Esto significa que las tareas pueden producir y consumir recursos. 
Lo que nos lleva a lo siguiente:

- La syscall solicitar brinda una interfaz para que las tareas puedan solicitar estos recursos. 
- Cuando una tarea solicita recursos, se queda bloqueada hasta encontrar una productora. 
- La productora debe producir una vez que ya se le solicitaron los recursos. 
 
Cuando un recurso esta listo, la tarea que la llama avisa que el recurso se encuentra en los 4KB a partir de la direccion virtual 0x0AAAA000 dela tarea productora. Esta debera ser copiada a los 4KB de la direccion virtual 0x0BBBB000 de la tarea que solicito el recurso.
- Si las direcciones no estan mapeadas, hay que limpiarlas antes de acceder. 
- La tarea llamadora debe ser desalojada y restaurada a su estado inicial.

El kernel tendra una interrupcion 41. El podra llamar a la syscall solicitar con el recurso ubicado en 0xFAFAFA (virtual = fisica identity mapping).

Ahora tenemos que establecer el mecanismo para que el sistema funcione con la produccion de recursos. Primeramente, nos definimos un struct `producciones_t` donde se guardara la informacion de las producciones. 

```c
typedef struct {
    task_id tarea_solicitante;
    recurso_t recurso_solicitado;
} producciones_t;

// Task id 1 es solicitado por el task id 2 
producciones_t producciones[MAX_TASKS] = {0};
// Si me solicito a mi mismo, es porque necesito que otro venga a producir mi recurso
```

Aniadimos un nuevo estado que represente la espera a la produccion de un recurso. Cuando una tarea produce, simplemente es runnable. Pero cuando solicito un recurso, debemos dejarla bloqueada hasta que este listo.

```c
typedef enum {
  TASK_SLOT_FREE,
  TASK_RUNNABLE,
  TASK_PAUSED
} task_state_t;
```

```c
void solicitar(recurso_t recurso) {
    // Chequeamos si hay una tarea que pueda generar el recurso 

    task_id tarea_productora = hay_tarea_disponible_para_recurso(recurso);

    // Si es 0, simplemente debo marcarla como pendiente y bloquearla
    if (tarea_productora == 0) 
    {
        // Informo que esta pendiente para el recurso
        informar_solicitud(current_task, recurso);
    }
    else {
        // La productora ya tiene forma de empezar a producir el recurso
        producciones[tarea_productora] = (producciones_t){
            .tarea_solicitante = current_task,
            .recurso_solicitado = recurso
        };

        // La habilito
        sched_enable_task(tarea_productora);

    }

    sched_disable_task(current_task);
    jmp_far(); // Jmp far de siempre
}
```

Teniendo esta logica, podemos saltar a como debe estar definida su interrupcion en `isr.asm`. Definiremos la interrupcion 90 con DPL 3 para que pueda ser llamada por tareas con nivel de privilegio de usuario.

```c
// ... Todo lo anterior 
IDT_ENTRY(90);
```

```x86asm
_isr90:
    pushad
    push eax ; pusheamos eax por stack
    call solicitar

    ; el jmp far lo hacemos DENTRO de solicitar
    popad
    iret
```

Ahora tenemos que definir la semantica de `recurso_listo`. Para ello, recordemos varias cosas importantes:
- Las tareas se comparten los recursos a traves de direcciones virtuales. Como sabemos que la productora deja el recurso en su virtual 0x0AAA0000, debemos hacer la traduccion a la fisica para poder copiarla a la virtual de la consumidora. 
- Debemos saber si la consumidora la tiene mapeada o no. Si la tiene mapeada, entonces simplemente hacemos la traduccion y copiamos. Sino, la mapeamos, la zereamos y luego la copiamos.
- Finalmente, marcamos como runnable a nuestra tarea solicitante ya que su recurso esta listo.

```c
#define VIRTUAL_PRODUCER 0x0AAA0000
#define VIRTUAL_CONSUMER 0x0BBBB000

void recurso_listo() {
    task_id tarea_solicitante = para_quien_produce(current_task);

    if (tarea_solicitante != NULL) {
        paddr_t recurso_producido = virt_to_phy(task_id_to_cr3(current_task), VIRTUAL_PRODUCER); // asumo que esta mapeada porque sino no tendria sentido generar un recurso no mapeado???

        uint32_t cr3 = task_id_to_cr3(tarea_solicitante);

        paddr_t solicitante_pagina;

        if (is_mapped(cr3, VIRTUAL_CONSUMER)) {
            solicitante_pagina = virt_to_phy(cr3, VIRTUAL_CONSUMER); // Si no esta mapeada, genera page fault
        }
        else {
            mmu_map_page(cr3, VIRTUAL_CONSUMER, solicitante_pagina = mmu_next_free_user_page(), MMU_P | MMU_U | MMU_W); // asumo no write ya que no tendria sentido 

            zero_page(solicitante_pagina);
        }
        
        copy_page(solicitante_pagina, recurso_producido);
    }

    restaurar_tarea(current_task);
    sched_enable_task(tarea_solicitante);
    jmp_far();
}
```

Ahora definimos en la IDT la interrupcion 91. 

```c
IDT_ENTRY(91);
```

```x86asm
_isr91:
    pushad
    call recurso_listo

    popad
    iret
```

## Segunda parte: restaurar tarea.

La poscondicion de `restaurar_tarea` es que la tarea se desaloje y se restaure a su estado inicial. Es decir, tiene que volver al estado con el cual empezo en el kernel. Para ello, debemos ver la TSS. Debemos asignarle el EIP al principio del codigo de la tarea. Tambien, el ESP y el EBP apuntaran al STACK_BASE. El ESP0 sera el ESP del kernel, el cual asignaremos por simplicidad como mmu_next_free_kernel_page() + PAGE_SIZE. 
El cr3 debera seguir siendo el mismo. 

```c
void restaurar_tarea(task_id task_id) {
    mmu_unmap_page(task_id_to_cr3(task_id), VIRTUAL_PRODUCER);
    mmu_unmap_page(task_id_to_cr3(task_id), VIRTUAL_CONSUMER);

    tss_t* tss = &tss_tasks[task_id];
    tss->eip = TASK_CODE_BASE; // El codigo debe ser el del comienzo.
    tss->esp = TASK_STACK_BASE; // El stack debe ser el del comienzo.
    tss->ebp = TASK_STACK_BASE;
    tss->esp0 = mmu_next_free_kernel_page() + PAGE_SIZE;
    tss->eflags = EFLAGS_IF;

    producciones[task_id] = (producciones_t){0};
    return;
}
```

Ahora paso a definir la interrupcion de kernel para el arranque manual.
EL mismo sabemos que tendra el comportamiento de la syscall `solicitar`. Podemos reutilizar la funcion.

IDT_ENTRY(41)

```x86asm
_isr41:
    pushad
    call pic_finish2
    
    call obtener_recurso
    push eax

    call solicitar
    pop eax
    popad
    iret
```

```c
recurso_t obtener_recurso() {
    return *(recurso_t*)0xFAFAFA;
}
```

El sistema se modificaria como ya lo hicimos, con un arreglo de producciones para cada tarea que produce y solicita.

Para llevar registro de que tarea produce cada recurso, alcanza con aniadir al mismo struct de producciones un campo "recurso_esperado".

```c
task_id hay_tarea_disponible_para_recurso(recurso_t recurso) {
    for (size_t i = 0; i < MAX_TASKS; i ++)
    {
        if (producciones[i].recurso_solicitado == NULL) {
            producciones[i].recurso_solicitado = recurso;
            producciones[i].tarea_solicitante = current_task;
            return i;
        }
    }

    return NULL;
}

task_id para_quien_produce(task_id tarea) {
    return producciones[tarea].tarea_solicitante;
}
```









