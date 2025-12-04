; ** por compatibilidad se omiten tildes **
; ==============================================================================
; TALLER System Programming - Arquitectura y Organizacion de Computadoras - FCEN
; ==============================================================================

%include "print.mac"

global start
; COMPLETAR - Agreguen declaraciones extern según vayan necesitando
extern GDT_DESC
extern IDT_DESC
extern idt_init
extern screen_draw_layout
extern clean_screen
extern pic_reset
extern pic_enable
extern _isr32
extern _isr33
extern _isr88
extern _isr98
extern mmu_init_kernel_dir
extern mmu_next_free_kernel_page
extern copy_page
extern page_equal
extern mmu_init_task_dir
extern tss_init
extern tasks_screen_draw
extern sched_init
extern tasks_init

; COMPLETAR - Definan correctamente estas constantes cuando las necesiten
%define CS_RING_0_SEL 0x8    
%define DS_RING_0_SEL 0x18   
%define CR0_PG_ENABLE_MASK 0x80000000
%define KERNEL_PAGE_DIR 0x00025000
%define ON_DEMAND_MEM_START_VIRTUAL    0x07000000
%define GDT_IDX_TASK_INITIAL         11
%define GDT_IDX_TASK_IDLE            12
%define INITIAL_TSS_SELECTOR (GDT_IDX_TASK_INITIAL << 3)
%define IDLE_TSS_SELECTOR (GDT_IDX_TASK_IDLE << 3)
%define DIVISOR 0x0800

BITS 16
;; Saltear seccion de datos
jmp start

;;
;; Seccion de datos.
;; -------------------------------------------------------------------------- ;;
start_rm_msg db     'Iniciando kernel en Modo Real'
start_rm_len equ    $ - start_rm_msg

start_pm_msg db     'Iniciando kernel en Modo Protegido'
start_pm_len equ    $ - start_pm_msg

;;
;; Seccion de código.
;; -------------------------------------------------------------------------- ;;

;; Punto de entrada del kernel.
[BITS 16]
start:
    ; ==============================
    ; ||  Salto a modo protegido  ||
    ; ==============================

    ; COMPLETAR - Deshabilitar interrupciones (Parte 1: Pasake a modo protegido)

    cli

    ; Cambiar modo de video a 80 X 50
    mov ax, 0003h
    int 10h ; set mode 03h
    xor bx, bx
    mov ax, 1112h
    int 10h ; load 8x8 font

    ; COMPLETAR - Imprimir mensaje de bienvenida - MODO REAL (Parte 1: Pasake a modo protegido)
    ; (revisar las funciones definidas en print.mac y los mensajes se encuentran en la
    ; sección de datos)

    print_text_rm start_rm_msg, start_rm_len, 0x6, 10, 10

    ; COMPLETAR - Habilitar A20 (Parte 1: Pasake a modo protegido)
    ; (revisar las funciones definidas en a20.asm)

    call A20_enable

    ; COMPLETAR - los defines para la GDT en defines.h y las entradas de la GDT en gdt.c
    ; COMPLETAR - Cargar la GDT (Parte 1: Pasake a modo protegido)
    lgdt [GDT_DESC]

    ; COMPLETAR - Setear el bit PE del registro CR0 (Parte 1: Pasake a modo protegido)
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; COMPLETAR - Saltar a modo protegido (far jump) (Parte 1: Pasake a modo protegido)
    ; (recuerden que un far jmp se especifica como jmp CS_selector:address)
    ; Pueden usar la constante CS_RING_0_SEL definida en este archivo
    jmp CS_RING_0_SEL:modo_protegido

[BITS 32]
modo_protegido:
    nop

    ; COMPLETAR (Parte 1: Pasake a modo protegido) - A partir de aca, todo el codigo se va a ejectutar en modo protegido
    ; Establecer selectores de segmentos DS, ES, GS, FS y SS en el segmento de datos de nivel 0
    ; Pueden usar la constante DS_RING_0_SEL definida en este archivo

    mov ax, DS_RING_0_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; COMPLETAR - Establecer el tope y la base de la pila (Parte 1: Pasake a modo protegido)
    mov ebp, 0x25000
    mov esp, ebp

    ; COMPLETAR - Imprimir mensaje de bienvenida - MODO PROTEGIDO (Parte 1: Pasake a modo protegido)
    print_text_pm start_pm_msg, start_pm_len,  0xB, 12, 15


    ; COMPLETAR - Inicializar pantalla (Parte 1: Pasake a modo protegido)
    call screen_draw_layout
    nop
    ; ===================================
    ; ||     (Parte 3: Paginación)     ||
    ; ===================================

    ; COMPLETAR - los defines para la MMU en defines.h // supongo que se refiere a los de la teorica
    ; COMPLETAR - las funciones en mmu.c // listo
    ; COMPLETAR - reemplazar la implementacion de la interrupcion 88 (ver comentarios en isr.asm) //listo(?
    ; COMPLETAR - La rutina de atención del page fault en isr.asm // listo(?
    
    ; COMPLETAR - Inicializar el directorio de paginas
    ; COMPLETAR - Cargar directorio de paginas 
    call mmu_init_kernel_dir

    ; COMPLETAR - Habilitar paginacion 
    ;inicializar CR3
    ;mov eax, KERNEL_PAGE_DIR ;<--- Esta linea no hace falta, mmu_init_kernel_dir ya devuelve KERNEL_PAGE_DIR en eax :)
    mov cr3, eax
    
    ;activar el bit PG de CR0
    mov eax, cr0 
    or eax, CR0_PG_ENABLE_MASK
    mov cr0, eax
    call mmu_next_free_kernel_page
    

    ;TEST DEL COPY_PAGE
    ;call mmu_next_free_kernel_page

    ;mov esi, eax 

    ;or DWORD[esi], 0x1

    ;push esi
    ;add esp, 12 

    ;call mmu_next_free_kernel_page

    ;sub esp, 12
    ;pop esi

    ;mov edi, eax

    ;push edi 
    ;push esi 

    ;call copy_page

    ;pop esi 
    ;pop edi

    ;push esi 
    ;push edi 

    ;call page_equal ;<-- Esta funcion esta en mmu.c, esta comentada

    ;pop edi 
    ;pop esi

    ; ========================
    ; ||  (Parte 4: Tareas) ||
    ; ========================

    ; COMPLETAR - reemplazar la implementacion de la interrupcion 88 (ver comentarios en isr.asm)
    ; COMPLETAR - las funciones en tss.c
    ; COMPLETAR - Inicializar tss
    call tss_init

    ; COMPLETAR - Inicializar el scheduler
    call sched_init

    ; COMPLETAR - Inicializar las tareas
    call tasks_init

    ; ===================================
    ; ||   (Parte 2: Interrupciones)   ||
    ; ===================================

    ; COMPLETAR - las funciones en idt.c

    ; COMPLETAR - Inicializar y cargar la IDT
    call idt_init
    lidt [IDT_DESC]

    ; COMPLETAR - Reiniciar y habilitar el controlador de interrupciones (ver pic.c)
    call pic_reset ; remapear PIC
    call pic_enable ; habilitar PIC
    sti ; habilitar interrupciones

    mov al, 0x34 
    out 0x43, al  ; Configura el registro de control del PIT

    ;acelero el clock
    mov ax, DIVISOR ;muevo mi divisor a ax

    out 0x40, al ;le paso la parte baja al puerto 0x40

    rol ax, 8 ;muevo la parte alta a la parte baja

    out 0x40, al ;se lo paso a 0x40


    ; COMPLETAR - Rutinas de atención de reloj, teclado, e interrupciones 88 y 98 (en isr.asm)
    int 88
    int 98

    mov edi, 0x18000 
    mov ecx, cr3
    push ecx
    push edi

    call mmu_init_task_dir

    mov cr3, eax ;actualizamos el cr3, tendriamos que conservar el cr3 de kernel?
    
    mov eax, ON_DEMAND_MEM_START_VIRTUAL
    mov DWORD[eax], 1212 ;primer escritura, tendria que tirar "page fault" 
    mov DWORD[eax], 6767 ;segunda escritura

    pop edi
    pop ecx

    mov cr3, ecx ;Devuelvo el cr3 a su valor original

    ; COMPLETAR (Parte 4: Tareas)- Cargar tarea inicial
    ;call tasks_screen_draw
    xor eax, eax
    mov ax, INITIAL_TSS_SELECTOR
    ltr ax 
    ; COMPLETAR - Habilitar interrupciones (!! en etapas posteriores, evaluar si se debe comentar este código !!)
    
    ; NOTA: Pueden chequear que las interrupciones funcionen forzando a que se
    ;       dispare alguna excepción (lo más sencillo es usar la instrucción
    ;       `int3`)
    ;int3

    ; COMPLETAR - Probar Sys_call (para etapas posteriores, comentar este código)

    ; COMPLETAR - Probar generar una excepción (para etapas posteriores, comentar este código)
    
    ; ========================
    ; ||  (Parte 4: Tareas)  ||
    ; ========================
    
    ; COMPLETAR - Inicializar el directorio de paginas de la tarea de prueba

    ; COMPLETAR - Cargar directorio de paginas de la tarea

    ; COMPLETAR - Restaurar directorio de paginas del kernel

    ; COMPLETAR - Saltar a la primera tarea: Idle
    jmp IDLE_TSS_SELECTOR:0 ;salto a Idle Task usando mi selector para Idle y 0 de offset
    nop

    ; Ciclar infinitamente 
    mov eax, 0xFFFF
    mov ebx, 0xFFFF
    mov ecx, 0xFFFF
    mov edx, 0xFFFF
    jmp $

;; -------------------------------------------------------------------------- ;;

%include "a20.asm"
