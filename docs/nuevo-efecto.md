# Cómo añadir un efecto

Cada efecto vive en un solo archivo y no sabe nada de OpenFX. La capa `src/ofx/OfxPlugin.cpp` lo convierte en plugin automáticamente.

## 1. Crea el archivo

`src/effects/<categoria>/MiEfecto.cpp`:

```cpp
#include <cmath>

#include "core/Effect.h"
#include "core/Ops.h"

namespace prisma {

class Vignette : public Effect {
public:
    const EffectInfo& info() const override {
        static const EffectInfo i{
            "io.github.prismafx.Vignette",   // id: no lo cambies nunca una vez publicado
            "Vignette", "Stylize",            // nombre y submenú
            "Oscurece los bordes del encuadre.", 1, 0,
            {
                ParamSpec::real("amount", "Amount", 0.5, 0, 1),
                ParamSpec::real("softness", "Softness", 0.6, 0, 1),
                ParamSpec::point("center", "Center", 0.5, 0.5),
            }};
        return i;
    }

    void render(const Image& src, Image& dst, const RenderArgs& a) const override {
        const ParamValues& p = *a.params;
        float cx, cy;
        a.pointToPixels("center", cx, cy);
        float radius = 0.5f * std::hypot(float(a.frame.width()), float(a.frame.height()));
        parallelFor(dst.bounds().y1, dst.bounds().y2, [&](int y) {
            for (int x = dst.bounds().x1; x < dst.bounds().x2; ++x) {
                float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy) / radius;
                float k = 1 - p.f("amount") * smoothstep(1 - p.f("softness"), 1, d);
                RGBA c = src.get(x, y);
                dst.at(x, y) = {c.r * k, c.g * k, c.b * k, c.a};
            }
        });
    }
};

PRISMA_REGISTER_EFFECT(Vignette)

}  // namespace prisma
```

## 2. Compila y prueba

```sh
cmake --build build
build/ofxtest --bundle-dir build/bundle --plugin io.github.prismafx.Vignette --in in.ppm --out out.ppm
ctest --test-dir build --output-on-failure
```

No hace falta tocar CMake: cualquier `.cpp` dentro de `src/effects/` entra en el bundle.

## Reglas que hay que respetar

- **Escribe todos los píxeles de `dst`.** `dst` cubre justo la ventana que pide el host.
- **Declara cuánto lees alrededor (`footprint`).** Si un píxel de salida lee la fuente a N píxeles de distancia (blur, desplazamientos…), devuelve `Footprint::pixels(N)` en píxeles a resolución completa. Si puede leer cualquier punto del encuadre (zoom radial, rayos), devuelve `Footprint::wholeFrame()`. Los tests de tiles fallan si te quedas corto.
- **Escala los tamaños con `a.renderScale`.** Los parámetros en píxeles están a resolución completa. Multiplícalos por `renderScale` para que el proxy a ½ o ¼ se vea igual.
- **Las posiciones son normalizadas (0..1).** Usa `a.pointToPixels` para convertirlas.
- **El ruido y lo aleatorio deben ser deterministas.** Usa `hash01` o `valueNoise` con coordenadas canónicas (`(x + 0.5) / renderScale`) y el fotograma, nunca `rand()`. Así cada tile y cada render salen idénticos.
- **Los nombres de parámetros son para siempre.** Los proyectos guardados los usan. Puedes cambiar la etiqueta visible, pero no el `name`.
- Sube `versionMinor` cuando añadas parámetros y `versionMajor` si cambias el resultado de forma incompatible.
