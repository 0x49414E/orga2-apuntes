# Conversión de 817 MiB a límite de segmento (modo protegido x86)

## 1. Datos iniciales
- Tamaño deseado: **817 MiB**
- 1 MiB = 1024 × 1024 bytes  
  → 817 MiB = 817 × 1024 × 1024 = **856,686,592 bytes**

## 2. Segmentación y granularidad
En el descriptor de segmento (GDT/LDT):

- Si **G = 0** → límite en **bytes** (máx. 1 MB)
- Si **G = 1** → límite en **bloques de 4 KiB** (granularidad de 4 KiB)

En segmentación *flat*, normalmente se usa **G = 1** para poder abarcar grandes tamaños.

## 3. Cálculo del límite con granularidad 4 KiB

El límite representa el **último bloque de 4 KiB accesible**, por lo tanto:

\[
\text{limit} = \frac{\text{tamaño en bytes}}{4\,\text{KiB}} - 1
\]

### Sustituyendo:
\[
\text{limit} = \frac{856,686,592}{4096} - 1 = 209,152 - 1 = 209,151
\]

→ En hexadecimal: **0x331FF**

## 4. Justificación del “-1”
El campo *limit* no indica el tamaño, sino la **última dirección válida** dentro del segmento.  
Por eso:
- Tamaño real = (limit + 1) × unidad de granularidad
- Si no restaras 1, el segmento cubriría **4 KiB más** de lo deseado.

Ejemplo:
- Sin restar 1 → tamaño = 856,690,688 B (4 KiB extra)
- Restando 1 → tamaño = 856,686,592 B (exactamente 817 MiB)

## 5. Resultado final

| Parámetro | Valor |
|------------|--------|
| Tamaño deseado | 817 MiB |
| En bytes | 856,686,592 |
| Granularidad | 4 KiB (G = 1) |
| Límite | 209,151 |
| Límite (hex) | 0x331FF |

## 6. Ejemplo de descriptor (conceptual)
```asm
; Segmento de datos (817 MiB)
base = 0x00000000
limit = 0x331FF
granularity = 1  ; bloques de 4 KiB
