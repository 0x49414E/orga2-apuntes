/* ** por compatibilidad se omiten tildes **
================================================================================
 TRABAJO PRACTICO 3 - System Programming - ORGANIZACION DE COMPUTADOR II - FCEN
================================================================================

  Definicion de funciones del manejador de memoria
*/

#include "mmu.h"
#include "i386.h"

#include "kassert.h"

static pd_entry_t* kpd = (pd_entry_t*)KERNEL_PAGE_DIR;
static pt_entry_t* kpt = (pt_entry_t*)KERNEL_PAGE_TABLE_0;
// zurgy
static const uint32_t identity_mapping_end = 0x003FFFFF;
static const uint32_t user_memory_pool_end = 0x02FFFFFF;

static paddr_t next_free_kernel_page = 0x100000;
static paddr_t next_free_user_page = 0x400000;

/**
 * kmemset asigna el valor c a un rango de memoria interpretado
 * como un rango de bytes de largo n que comienza en s
 * @param s es el puntero al comienzo del rango de memoria
 * @param c es el valor a asignar en cada byte de s[0..n-1]
 * @param n es el tamaño en bytes a asignar
 * @return devuelve el puntero al rango modificado (alias de s)
*/
static inline void* kmemset(void* s, int c, size_t n) {
  uint8_t* dst = (uint8_t*)s;
  for (size_t i = 0; i < n; i++) {
    dst[i] = c;
  }
  return dst;
}

/**
 * zero_page limpia el contenido de una página que comienza en addr
 * @param addr es la dirección del comienzo de la página a limpiar
*/
static inline void zero_page(paddr_t addr) {
  kmemset((void*)addr, 0x00, PAGE_SIZE);
}

//zurgy
void mmu_init(void) {}


/**
 * mmu_next_free_kernel_page devuelve la dirección física de la próxima página de kernel disponible. 
 * Las páginas se obtienen en forma incremental, siendo la primera: next_free_kernel_page
 * @return devuelve la dirección de memoria de comienzo de la próxima página libre de kernel
 */
paddr_t mmu_next_free_kernel_page(void) {
    if (next_free_kernel_page < 0x3FF000) {
        paddr_t actual_free_kernel_page = next_free_kernel_page; 
        next_free_kernel_page += PAGE_SIZE;
        return actual_free_kernel_page; // zurgy
    } else {
        return next_free_kernel_page; 
    }
}

/**
 * mmu_next_free_user_page devuelve la dirección de la próxima página de usuarix disponible
 * @return devuelve la dirección de memoria de comienzo de la próxima página libre de usuarix
 */
paddr_t mmu_next_free_user_page(void) {
      if (next_free_user_page < 0x2FFF000) {
          paddr_t actual_free_user_page = next_free_user_page; 
          next_free_user_page += PAGE_SIZE;
          return actual_free_user_page;
      } else {
          return next_free_kernel_page; 
      }
}


/**
 * mmu_init_kernel_dir inicializa las estructuras de paginación vinculadas al kernel y
 * realiza el identity mapping
 * @return devuelve la dirección de memoria de la página donde se encuentra el directorio
 * de páginas usado por el kernel
 */
paddr_t mmu_init_kernel_dir(void) {
  
    zero_page((paddr_t)kpd); // Casteo a puntero de pagina para ponerlo en 0 ya que toma el primer elemento
    zero_page((paddr_t)kpt); 
    
    kpd[0] = (pd_entry_t){
      .attrs = MMU_P | MMU_W, 
      .pt = (KERNEL_PAGE_TABLE_0 >> 12), //Shifteamos 12, porque solo usamos los 20 bits mas altos.
    };

    for (size_t i = 0; i < PAGE_SIZE / 4; i++) //PageSize = 4096 -> /4 = 1024 -> las 1024 entradas de la PT
    {
      paddr_t physical_address = i * PAGE_SIZE;

      kpt[i] = (pt_entry_t) {
        .attrs = MMU_P | MMU_W,
        .page = (physical_address >> 12), //Shifteamos 12, porque solo usamos los 20 bits mas bajos.
      };  
    }

    return (paddr_t)kpd; //retorna la kpd
}

/**
zurgy
 * mmu_map_page agrega las entradas necesarias a las estructuras de paginación de modo de que
 * la dirección virtual virt se traduzca en la dirección física phy con los atributos definidos en attrs
 * @param cr3 el contenido que se ha de cargar en un registro CR3 al realizar la traducción
 * @param virt la dirección virtual que se ha de traducir en phy
 * @param phy la dirección física que debe ser accedida (dirección de destino)
 * @param attrs los atributos a asignar en la entrada de la tabla de páginas
 */
void mmu_map_page(uint32_t cr3, vaddr_t virt, paddr_t phy, uint32_t attrs) {

  uint32_t directory_offset = VIRT_PAGE_DIR(virt); //Definimos el offset para traer la pde
  pd_entry_t* cr3_page_directory = (pd_entry_t*)CR3_TO_PAGE_DIR(cr3); // Definimos la direccion de memoria para traer la pde 

  pd_entry_t pde = cr3_page_directory[directory_offset]; // definimos la pde con los datos anteriores

  if (!(pde.attrs & MMU_P)) { 
    // Lògica no presente
    cr3_page_directory[directory_offset].attrs = attrs | MMU_P | MMU_W;

    uint32_t new_page_table = (uint32_t)mmu_next_free_kernel_page();
    zero_page(new_page_table); 

    cr3_page_directory[directory_offset].pt = HIGHEST_20_BITS(new_page_table); 
    pde = cr3_page_directory[directory_offset];
  }
  
  // Lògica para presente
  uint32_t page_table_offset = VIRT_PAGE_TABLE(virt);

  pt_entry_t* ptr_to_pt = (pt_entry_t*)(MMU_ENTRY_PADDR(pde.pt));

  ptr_to_pt[page_table_offset].page = HIGHEST_20_BITS(phy);
  ptr_to_pt[page_table_offset].attrs = attrs | MMU_P;

  //Reseteo la TLB para evitar que queden traducciones desactualizadas
  tlbflush();

  return; // dede
}

/**
 * mmu_unmap_page elimina la entrada vinculada a la dirección virt en la tabla de páginas correspondiente
 * @param virt la dirección virtual que se ha de desvincular
 * @return la dirección física de la página desvinculada
 */
paddr_t mmu_unmap_page(uint32_t cr3, vaddr_t virt) {
  pd_entry_t* pd = (pd_entry_t*) CR3_TO_PAGE_DIR(cr3); //Obtengo la direccion de la PD ubicada en CR3
  //uint32_t offset_dir_virt : 10; 
  uint32_t offset_dir_virt =  VIRT_PAGE_DIR(virt); //Obtengo el offset de la PD ubicado en mi virtual address
  pd_entry_t pde = pd[offset_dir_virt]; //Lo utilizo como indice para obtener la PDE

  pt_entry_t* pt = (pt_entry_t*) (pde.pt << 12); //Del struct PDE obtengo la direccion de la PT (son 20 bits, lo shifteo 12 para que sean 32)
  //uint32_t offset_table_virt : 10; 
  uint32_t offset_table_virt = VIRT_PAGE_TABLE(virt); //Obtengo el offset de la PT ubicado en mi virtual address

  paddr_t phy_return = (paddr_t) (pt[offset_table_virt].page << 12); //Yo quiero retornar esta direccion fisica
  
  //Limpio la PTE para desvincularla
  pt[offset_table_virt].page = 0;
  pt[offset_table_virt].attrs = 0;

  //Reseteo la TLB para evitar que queden traducciones desactualizadas
  tlbflush();

  return phy_return;
}

#define DST_VIRT_PAGE 0xA00000
#define SRC_VIRT_PAGE 0xB00000

/**
zurgy
 * copy_page copia el contenido de la página física localizada en la dirección src_addr a la página física ubicada en dst_addr
 * @param dst_addr la dirección a cuya página queremos copiar el contenido
 * @param src_addr la dirección de la página cuyo contenido queremos copiar
 *
 * Esta función mapea ambas páginas a las direcciones SRC_VIRT_PAGE y DST_VIRT_PAGE, respectivamente, realiza
 * la copia y luego desmapea las páginas. Usar la función rcr3 definida en i386.h para obtener el cr3 actual
 */
void copy_page(paddr_t dst_addr, paddr_t src_addr) {
  uint32_t cr3 = rcr3();
  mmu_map_page(cr3, SRC_VIRT_PAGE, src_addr, MMU_P | MMU_W | MMU_U); // hay que fijarse
  mmu_map_page(cr3, DST_VIRT_PAGE, dst_addr, MMU_P | MMU_W | MMU_U);
  
  uint32_t* dst_addr_ptr = (uint32_t*)dst_addr;
  uint32_t* src_addr_ptr = (uint32_t*)src_addr;

  for (size_t i = 0; i < PAGE_SIZE / 4; i++) 
  {
    *(dst_addr_ptr + i) = *(src_addr_ptr + i);  
  }
  mmu_unmap_page(cr3, SRC_VIRT_PAGE);
  mmu_unmap_page(cr3, DST_VIRT_PAGE);
}

 /**
 * mmu_init_task_dir inicializa las estructuras de paginación vinculadas a una tarea cuyo código se encuentra en la dirección phy_start
 * @pararm phy_start es la dirección donde comienzan las dos páginas de código de la tarea asociada a esta llamada
 * @return el contenido que se ha de cargar en un registro CR3 para la tarea asociada a esta llamada
 */
paddr_t mmu_init_task_dir(paddr_t phy_start) { 
    //Pedimos una nueva pagina de kernel para el page directory
    paddr_t pd_address_cr3 = mmu_next_free_kernel_page(); 

    pd_entry_t* task_dir = (pd_entry_t*)pd_address_cr3;
    zero_page(pd_address_cr3);

    // **Mapeo los primero 4mb**
    // 4MB = 1024 pags (cada pag son 4KB)
    // Es identity mapping, asi que las virt coinciden con las fisicas
    for (uint32_t i = 0; i < 1024; i++) {
        paddr_t addr = i * PAGE_SIZE;  // Tanto la virt como la fisica
        mmu_map_page(pd_address_cr3, addr, addr, MMU_P | MMU_W);  // Presente + R/W , Kernel lvl
    }
    
    // Mapeamos las 2 paginas de codigo...
    mmu_map_page(pd_address_cr3, TASK_CODE_VIRTUAL, phy_start, MMU_P | MMU_U);
    mmu_map_page(pd_address_cr3, TASK_CODE_VIRTUAL + PAGE_SIZE, phy_start + PAGE_SIZE, MMU_P | MMU_U);
    
    // Pedimos una pagina de usuario para la pila
    paddr_t phy_stack = mmu_next_free_user_page(); 
    // Mapeamos la pagina de pila
    mmu_map_page(pd_address_cr3, TASK_STACK_TOP, phy_stack, MMU_P | MMU_W | MMU_U);
    // Mapeamos la memoria compartida
    mmu_map_page(pd_address_cr3, TASK_SHARED_PAGE, SHARED, MMU_P | MMU_W | MMU_U);
    
    return pd_address_cr3; 
}

// COMPLETAR: devuelve true si se atendió el page fault y puede continuar la ejecución 
// y false si no se pudo atender
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
  
  return false; 
}

/*bool page_equal(paddr_t pag1, paddr_t pag2) {
  uint32_t* pag1_addr = (uint32_t*) pag1;
  uint32_t* pag2_addr = (uint32_t*) pag2;

  for (size_t i = 0; i < PAGE_SIZE / 4; i++) 
  {
    if (*(pag1_addr + i) != *(pag2_addr + i))
    {
      return false;
    }
  }

  return true;
}*/
