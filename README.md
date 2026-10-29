# Prisma FX

Pack de efectos open source (licencia BSD-3) para cualquier programa compatible con **OpenFX**: Autograph, DaVinci Resolve/Fusion, Nuke, Natron, Vegas, Flame…
La idea es ir recreando, uno a uno, el tipo de efectos que ofrecen suites comerciales como Sapphire o Universe, sin depender de Adobe. El plan completo, con la equivalencia de cada efecto, está en [docs/hoja-de-ruta.md](docs/hoja-de-ruta.md).

![Muestras](docs/muestras.png)

## Efectos incluidos (39)

| Efecto | Menú | Qué hace |
|---|---|---|
| **Glow** | Prisma FX / Light | Bloom suave a varias escalas a partir de las zonas brillantes. |
| **Rays** | Prisma FX / Light | Rayos volumétricos que salen de las zonas brillantes desde un punto central. |
| **Glint** | Prisma FX / Light | Destellos en estrella sobre los brillos, como un filtro de cruz. |
| **Lens Flare** | Prisma FX / Light | Flare procedural con halo, streak anamórfico y fantasmas; punto de luz animable. |
| **Edge Rays** | Prisma FX / Light | Rayos que salen de los bordes del alfa o de la luma. |
| **Light Leaks** | Prisma FX / Light | Fugas de luz orgánicas y animadas, con dos colores. |
| **Glow Rings** | Prisma FX / Light | Anillos de luz alrededor de los brillos, con separación de espectro. |
| **Streaks** | Prisma FX / Light | Glow direccional: los brillos se estiran en líneas, como un flare anamórfico. |
| **Lux** | Prisma FX / Light | Luz puntual o foco visible en neblina, que el propio clip puede tapar. |
| **Blur** | Prisma FX / Blur | Desenfoque gaussiano con aspecto y ángulo. |
| **Zoom Blur** | Prisma FX / Blur | Desenfoque radial de zoom desde un centro. |
| **Spin Blur** | Prisma FX / Blur | Desenfoque de giro alrededor de un centro. |
| **Defocus** | Prisma FX / Blur | Desenfoque de lente con diafragma redondo o poligonal; las luces se abren en bokeh. |
| **Tilt Shift** | Prisma FX / Blur | Banda enfocada y desenfoque progresivo alrededor: efecto miniatura. |
| **Lens Distort** | Prisma FX / Distort | Distorsión de barril y cojín. |
| **Kaleidoscope** | Prisma FX / Distort | Caleidoscopio de N segmentos o espejo simple. |
| **Shake** | Prisma FX / Distort | Temblor de cámara con rotación, zoom y desenfoque de movimiento. |
| **Warp** | Prisma FX / Distort | Distorsión orgánica con ruido animado. |
| **Prism** | Prisma FX / Stylize | Aberración cromática espectral, radial (tipo lente) o lineal. |
| **Film Grain** | Prisma FX / Stylize | Grano de película animado. Su tamaño no cambia con la resolución de previsualización. |
| **Glitch** | Prisma FX / Stylize | Daño digital: franjas desplazadas, bloques corruptos, separación RGB y scanlines. |
| **VHS** | Prisma FX / Stylize | Cinta gastada: jitter, banda de tracking, sangrado de color, ruido y scanlines. |
| **Halftone** | Prisma FX / Stylize | Trama de puntos de imprenta, en una tinta o en color. |
| **Edge Detect** | Prisma FX / Stylize | Bordes, dibujo a tinta o look cartoon. |
| **Vignette** | Prisma FX / Stylize | Viñeta con centro móvil. Es también el ejemplo de la guía para crear efectos. |
| **Color Grade** | Prisma FX / Color | Exposición, balance de blancos, lift/gamma/gain, contraste, saturación y bleach bypass. |
| **Sharpen** | Prisma FX / Blur | Unsharp mask con umbral y modo solo luma. |
| **Film Effect** | Prisma FX / Stylize | Película antigua: gate weave, parpadeo, polvo, rayas, fade y tinte. |
| **Film Look** | Prisma FX / Color | Curva de película, halation alrededor de las luces y fade. |
| **Gradient Map** | Prisma FX / Color | Duotono y tritono: mapea los tonos a dos o tres colores. |
| **Posterize** | Prisma FX / Color | Posterizar en bandas, umbral a dos colores o solarizar. |
| **Skin Smooth** | Prisma FX / Color | Suavizado que conserva los bordes, limitado a tonos de piel. |
| **Mojo** | Prisma FX / Color | Look teal & orange que protege la piel. |
| **Clouds** | Prisma FX / Generate | Ruido fractal animado: nubes, humo, niebla o texturas. |
| **Aurora** | Prisma FX / Generate | Cortinas de aurora boreal que ondulan con el tiempo. |
| **Lightning** | Prisma FX / Generate | Rayo eléctrico entre dos puntos, con ramas, núcleo y glow. |
| **Gradient** | Prisma FX / Generate | Degradados lineal, radial, cónico o de cuatro esquinas. |
| **Grid** | Prisma FX / Generate | Rejilla girable de líneas, con puntos opcionales en los cruces. |
| **Sparkles** | Prisma FX / Generate | Campo de estrellas de cuatro puntas que titilan. |

Los generadores tienen un modo **Combine** (Over, Screen, Add, Multiply, Replace) y opacidad para mezclarse con el clip. Todos trabajan en 8, 16 y 32 bits (float), por tiles y a cualquier escala de render (proxy).

## Instalar

1. Descarga `PrismaFX.ofx.bundle` desde los artefactos de GitHub Actions (o compílalo tú).
2. Cópialo a la carpeta de plugins OFX del sistema:
   - **Windows:** `C:\Program Files\Common Files\OFX\Plugins\`
   - **macOS:** `/Library/OFX/Plugins/`
   - **Linux:** `/usr/OFX/Plugins/`
3. Reinicia Autograph (o el programa que uses). Los efectos aparecen en el grupo **Prisma FX**.

## Compilar

Hace falta CMake 3.20 o superior y un compilador de C++17. Las cabeceras de OpenFX van incluidas en `external/`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

El bundle queda en `build/bundle/PrismaFX.ofx.bundle`. En macOS añade `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` para generar un binario universal.

## Probar sin ningún programa

`ofxtest` es un host OpenFX mínimo de línea de comandos, construido sobre la librería HostSupport oficial. Carga el bundle igual que lo haría Autograph y renderiza una imagen:

```sh
build/ofxtest --bundle-dir build/bundle --list
build/ofxtest --bundle-dir build/bundle --plugin io.github.prismafx.Glow \
    --in entrada.ppm --out salida.ppm --param width=80 --param tint=1,0.6,0.3
ctest --test-dir build --output-on-failure
```

Los tests renderizan cada efecto en 8, 16 y 32 bits y a media resolución. En cada caso comprueban que el render por tiles coincide con el de fotograma completo, lo que detecta efectos que leen fuera de la zona de la imagen que declaran necesitar. Linux necesita `libexpat1-dev` y `python3-numpy` para los tests.

## Añadir un efecto nuevo

Un efecto nuevo es un único archivo `.cpp` en `src/effects/`. Declaras sus parámetros, escribes la función `render` sobre imágenes float RGBA y lo registras con una línea. CMake lo detecta solo, y la capa OFX genera la interfaz y se encarga de bits, tiles y escalas. Tienes la guía paso a paso en [docs/nuevo-efecto.md](docs/nuevo-efecto.md) y el plan de efectos en [docs/hoja-de-ruta.md](docs/hoja-de-ruta.md).

## Estructura

```
src/core/      Imagen float, parámetros, registro de efectos y utilidades (blur, ruido, mezcla)
src/ofx/       Puente genérico a OpenFX (el único código que habla OFX)
src/effects/   Un archivo por efecto, agrupados por categoría
tools/ofxtest/ Host de pruebas
tests/         Imagen de prueba y suite de tests
```

## Licencia

BSD-3-Clause. Las cabeceras de OpenFX en `external/openfx` son © OpenFX contributors, también con licencia BSD-3. Sapphire, Red Giant, Universe, Trapcode, Magic Bullet y After Effects son marcas de sus respectivos dueños. Este proyecto no tiene relación con ellos ni usa su código.
