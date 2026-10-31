# Prisma FX en After Effects

Los mismos efectos de Prisma FX funcionan también como plugin nativo de After Effects. El puente vive en `src/ae/` y no toca ningún efecto: cada uno se convierte en un efecto SmartFX de AE con soporte de 8, 16 y 32 bits (float) y de render multi-frame.

## Estado

Primera versión. El puente que no depende del SDK (`src/ae/AeBridge.cpp`) tiene sus propios tests. Esos tests renderizan los 53 efectos con buffers como los de AE (ARGB, filas de arriba abajo, 8/16/32 bits, por tiles y a media resolución) y comprueban que el resultado coincide con el render directo. La parte que habla con el SDK (`src/ae/AePlugin.cpp`) se compila solo cuando el SDK está disponible y todavía hay que probarla dentro de After Effects.

Hay que confirmar en AE dos cosas:
- **Controles de punto.** El puente asume que AE da los puntos en píxeles de la capa a resolución completa. Si a resolución Half o Quarter el centro de un efecto (Zoom Blur, Lens Flare…) se desplaza, el fallo está en `pointFromLayer` de `AePlugin.cpp`.
- **Capas recortadas o ampliadas por otros efectos.** El puente toma la capa como el rectángulo `0,0 – ancho,alto`.

## Cómo aparece en AE

- Menú **Efecto > Prisma FX - Light**, **Prisma FX - Blur**, etc., uno por categoría.
- Los nombres internos (match names) son `PrismaFX <Efecto>`, por ejemplo `PrismaFX Glow`. No cambian nunca, porque los proyectos los guardan.
- Las transiciones funcionan como las de AE: tienen un control **Transition Completion** (0–100 %) y una **Destination Layer**, que es la capa a la que se pasa. Si no eliges capa, la transición va a transparente.
- Los parámetros se identifican por su posición. Para no romper proyectos guardados, los parámetros nuevos de un efecto se añaden siempre al final.

## Conseguir el SDK

El SDK de After Effects es gratuito, pero su licencia no permite redistribuirlo, así que no está en el repo.

1. Descárgalo con tu Adobe ID desde <https://developer.adobe.com/after-effects/>. Coge la versión de Windows o la de macOS según dónde compiles.
2. Descomprímelo. Dentro hay una carpeta `Examples` con `Headers`, `Resources` y `Util`. Esa es la ruta que necesita CMake.

## Compilar en tu ordenador

Windows (Visual Studio 2022 con C++):

```bat
cmake -S . -B build-ae -DPRISMA_BUILD_TESTS=OFF -DPRISMA_AE_SDK="C:/ruta/al/SDK/Examples"
cmake --build build-ae --config Release --target PrismaFX_AE
```

El plugin queda en `build-ae/ae-plugin/PrismaFX.aex`. En macOS se genera `PrismaFX.plugin`; añade `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` para un binario universal.

## Compilarlo en GitHub (CI)

El job `ae` del workflow compila el plugin para Windows y macOS si encuentra el SDK. Para dárselo sin hacerlo público:

1. Crea un repositorio **privado** `emivalc/ae-sdk` y sube dentro el SDK descomprimido.
2. Crea un token de GitHub de tipo *fine-grained* con permiso de lectura (*Contents: Read*) solo sobre `emivalc/ae-sdk`.
3. En `emivalc/prisma-fx`, ve a *Settings > Secrets and variables > Actions* y guárdalo como secreto `AE_SDK_TOKEN`.

Sin el secreto, el job se salta y el resto de la CI sigue igual. Con él, los artefactos son `PrismaFX-AE-Win64` y `PrismaFX-AE-MacOS`.

## Instalar

1. Cierra After Effects.
2. Copia `PrismaFX.aex` (Windows) o `PrismaFX.plugin` (macOS) en la carpeta de plugins:
   - **Windows:** `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\` (la comparten todas las versiones de AE).
   - **macOS:** `/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/`
3. Abre AE. Los efectos salen en el menú **Efecto**, en las categorías **Prisma FX - …**.
