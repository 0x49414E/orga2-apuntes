Definimos algunas auxiliares importantes.

```c
uint32_t task_selector_to_CR3(uint16_t selector) {
    uint16_t index = selector >> 3; // Sacamos los atributos
    gdt_entry_t* taskDescriptor = &gdt[index]; // Indexamos en la gdt
    tss_t* tss = (tss_t*)((taskDescriptor->base_15_0) |
    (taskDescriptor->base_23_16 << 16) |
    (taskDescriptor->base_31_24 << 24));
    return tss->cr3;
}

uint32_t task_id_to_cr3(uint32_t task_id) {
    return tss_tasks[task_id].cr3;
}

paddr_t virt_to_phy(uint32_t cr3, vaddr_t virt) {

 uint32_t* cr3 = task_selector_to_cr3(task_id);
 uint32_t pd_index = VIRT_PAGE_DIR(virtual_address);
 uint32_t pt_index = VIRT_PAGE_TABLE(virtual_address);
 pd_entry_t* pd = (pd_entry_t*)CR3_TO_PAGE_DIR(cr3);

 pt_entry_t* pt = pd[pd_index].pt << 12;
 return (paddr_t) (pt[pt_index].page << 12);
}

bool is_mapped(uint32_t cr3, vaddr_t virt) {
    uint32_t pd_index = VIRT_PAGE_DIR(virt);
    uint32_t pt_index = VIRT_PAGE_TABLE(virt);
    pd_entry_t* pd = (pd_entry_t*)CR3_TO_PAGE_DIR(cr3);

    if (pd[pd_index].attrs & MMU_P) {
        pt_entry_t* pt = (pt_entry_t*)MMU_ENTRY_PADDR(pd[pd_index].pt);
        
        return pt[pt_index].attrs & MMU_P;
    }

    return false;
}

// inspiradas en las funciones que tenemos para tareas de nivel 3. Esto es por si me piden tareas de nivel 0.
static int8_t create_task_kill() {
    size_t gdt_id;

    for (gdt_id = GDT_TSS_START; gdt_id < GDT_COUNT; gdt_id++) {
        if (gdt[gdt_id].p == 0) break;
    }

    kassert(gdt_id < GDT_COUNT,
    "No hay entradas disponibles en la GDT");

    int8_t task_id = sched_add_task(gdt_id << 3);
    tss_tasks[task_id] = tss_create_kernel_task(&killer_main_loop);
    gdt[gdt_id] = tss_gdt_entry_for_task(&tss_tasks[task_id]);
    return task_id;
}


tss_t tss_create_kernel_task(paddr_t code_start) {
    vaddr_t stack = mmu_next_free_kernel_page();
    return (tss_t) {
        .cr3 = create_cr3_for_kernel_task(),
        .esp = stack + PAGE_SIZE,
        .ebp = stack + PAGE_SIZE,
        .eip = (vaddr_t)code_start,
        .cs = GDT_CODE_0_SEL,
        .ds = GDT_DATA_0_SEL,
        .es = GDT_DATA_0_SEL,
        .fs = GDT_DATA_0_SEL,
        .gs = GDT_DATA_0_SEL,
        .ss = GDT_DATA_0_SEL,
        .ss0 = GDT_DATA_0_SEL,
        .esp0 = stack + PAGE_SIZE,
        .eflags = EFLAGS_IF,
    };
}

paddr_t create_cr3_for_kernel_task() {
    // Inicializamos el directorio de paginas
    paddr_t task_page_dir = mmu_next_free_kernel_page();
    zero_page(task_page_dir);
    // Realizamos el identity mapping
    for (uint32_t i = 0; i < identity_mapping_end; i += PAGE_SIZE) {
    mmu_map_page(task_page_dir,i, i, MMU_W);
    }
    return task_page_dir;
}

/**
 * zero_page pone en cero el contenido de una página física
 * @param addr la dirección física de la página que queremos zerear
 */
void zero_page(paddr_t addr) {
  uint32_t cr3 = rcr3();
  
  // Mapear la página física a una dirección virtual temporal
  mmu_map_page(cr3, TEMP_VIRT_PAGE, addr, MMU_P | MMU_W);
  
  // Zerear usando la dirección virtual
  uint32_t* ptr = (uint32_t*)TEMP_VIRT_PAGE;
  for (size_t i = 0; i < PAGE_SIZE / 4; i++) {
    ptr[i] = 0;
  }
  
  // Alternativamente, puedes usar memset:
  // memset((void*)TEMP_VIRT_PAGE, 0, PAGE_SIZE);
  
  // Desmapear
  mmu_unmap_page(cr3, TEMP_VIRT_PAGE);
}
```

Siempre que atendemos una interrupcion de kernel, debemos hacerle saber al CPU que ya la atendimos. Para ello nos comunicamos con el PIC.

pic_finish1 hace que el PIC sepa que ya atendimos la interrupcion X donde X pertenece a [32,39]
pic_finish2 hace que el PIC sepa que ya atendimos la interrupcion X donde X pertenece a [40,47]

Para agarrar el cr3 con el task_id, solo alcanza con fijarse la estructura de la tss_tasks con el task_id y ver cual es el cr3.
Siempre que nos pidan una region de memoria especifica, quizas habra que tocar el page fault handler. Hay workarounds como ver si esta presente la pagina pero en lineas generales se hace asi.

El environment tiene cosas interesantes, como el tick_count, la tecla del teclado y el current_task.
SIempre que bloqueemos una tarea debemos saltar a la proxima desde el assembler.

No te olvides de definir las IDT_ENTRY segun el nivel de privilegio.
No te olvides de definir el numero de interrupcion y la interrupcion en el idt_init.

```x86
  call sched_next_task
 
  mov word [sched_task_selector], ax
  jmp far [sched_task_offset]
```
  
Codigo para saltar a la proxima tarea.

ESP0/SS0: Para syscalls e interrupciones (cambio de privilegio dentro de la misma tarea). Si va de ring 3 a ring 0, como sabe cual stack tenia el kernel?? facil, lo guardo y listo.
ESP, EIP, registros generales: Para task switching completo entre tareas diferentes

El kernel tiene identity mapping en el primer MiB de memoria. Todas las tareas deben tener el identity mapping del kernel.

EFLAGS: Registro de estado y control del procesador (32 bits)

# IF (Interrupt Flag, bit 9):

- Controla si las interrupciones externas están habilitadas
- IF=1: Interrupciones habilitadas
- IF=0: Interrupciones bloqueadas (excepto NMI)
- Se usa con STI (set) y CLI (clear)
- El kernel lo desactiva en secciones 


# NT (Nested Task, bit 14):

- Indica si hay un task switch anidado activo
- NT=1: La tarea actual fue invocada por otra tarea (hay un "return link")
- Usado con IRET para volver a la tarea anterior
- Relevante para task switching por hardware


## RECOMENDACIONES

Hacer un punta a punta de todo el sistema. Es recomendable ver la resolucion del 2p de maca.

Primero las syscalls, luego el IDT_ENTRY, luego el assembler, luego las estructuras necesarias.

De la 32 hasta la 39 atiende el pic1. De la 40 a la 47 atiende el pic2.

Un MiB son 0x100000 bytes.
Un KiB son 0x400 bytes.

Un MB son 1_000_000 bytes
Un KB son 1_000 bytes.

* **VIRT_PAGE_DIR(X)**: Dada X una dirección virtual calcula el índice dentro del directorio de páginas de la PDE asociada.

* **VIRT_PAGE_TABLE(X)**: Dada X una dirección virtual calcula el índice dentro de la tabla de páginas de la PTE asociada.

* **VIRT_PAGE_OFFSET(X)**: Dada X una dirección devuelve el offset dentro de su página.

* **CR3_TO_PAGE_DIR(X)**: Obtiene la dirección física del directorio donde X es el contenido del registro CR3.

* **MMU_ENTRY_PADDR(X)**: Obtiene la dirección física correspondiente, donde X es el campo address de 20 bits en una entrada de la tabla de páginas o del page directory

**SIEMPRE ACLARAR CONVENCION DE PASAJE DE ARGUMENTOS**

Si finaliza una tarea quizas lo ideal es hacer un jmp $ para evitar que ejecute codigo basura.