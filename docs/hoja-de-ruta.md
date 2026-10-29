# Hoja de ruta

Catálogo de efectos al estilo de Sapphire y de las suites de Red Giant (Universe, Trapcode, Magic Bullet, VFX), agrupados por familia. La columna "Referencia" solo indica qué tipo de efecto comercial cubre; el código es propio.

## Hecho

### v0.1
- [x] **Glow**: bloom multiescala (ref.: S_Glow, Optical Glow)
- [x] **Rays**: rayos volumétricos (ref.: S_Rays, Trapcode Shine)
- [x] **Prism**: aberración cromática espectral (ref.: S_Prism, Chromatic Displacement)
- [x] **Film Grain**: grano (ref.: S_Grain, Magic Bullet Renoiser)
- [x] **Glitch**: daño digital (ref.: S_DigitalDamage, Universe Glitch)
- [x] **Vignette**: el ejemplo de la guía

### Tanda 1
- [x] **Glint**: destellos en estrella sobre los brillos (ref.: S_Glint, Trapcode Starglow)
- [x] **Lens Flare**: flare procedural con halo, streak anamórfico y fantasmas (ref.: S_LensFlare, Knoll Light Factory)
- [x] **Edge Rays**: rayos desde los bordes del alfa o de la luma (ref.: S_EdgeRays)
- [x] **Light Leaks**: fugas de luz animadas (ref.: S_LightLeak, Universe Light Leaks)
- [x] **Blur**: gaussiano con aspecto y ángulo (ref.: S_Blur)
- [x] **Zoom Blur** (ref.: S_ZoomBlur, Universe Radial Blur)
- [x] **Spin Blur** (ref.: S_RadialBlur)
- [x] **Lens Distort**: barril y cojín (ref.: S_LensDistort)
- [x] **Kaleidoscope** y espejo (ref.: S_Kaleido, Universe Kaleidoscope)
- [x] **Shake**: temblor de cámara con desenfoque de movimiento (ref.: S_Shake, Universe Camera Shake)
- [x] **Warp**: distorsión con ruido animado (ref.: S_DistortRGB, Universe Wave Warp)
- [x] **Halftone**: trama de puntos mono o color (ref.: S_Halftone, Universe Halftone)
- [x] **Edge Detect**: bordes, tinta y cartoon (ref.: S_EdgeDetect, S_Cartoon, Universe Toon)
- [x] **VHS**: jitter, banda de tracking, sangrado de color y ruido (ref.: S_TVDamage, Universe VHS)
- [x] **Color Grade**: exposición, balance, lift/gamma/gain, contraste, saturación y bleach bypass (ref.: Magic Bullet Looks/Colorista)

### Tanda 2: película y color
- [x] **Film Effect**: gate weave, parpadeo, polvo, rayas, fade y tinte (ref.: S_FilmEffect, Magic Bullet Film)
- [x] **Film Look**: curva de película, halation y fade (ref.: Magic Bullet Film)
- [x] **Gradient Map**: duotono y tritono (ref.: S_Duotone, S_Tritone)
- [x] **Posterize**: posterizar, umbral a dos colores y solarizar (ref.: S_Posterize, S_Threshold, S_Solarize)
- [x] **Skin Smooth**: suavizado bilateral limitado a tonos de piel (ref.: Magic Bullet Cosmo)
- [x] **Sharpen**: unsharp mask con umbral y modo solo luma (ref.: S_Sharpen)
- [x] **Mojo**: look teal & orange que protege la piel (ref.: Magic Bullet Mojo)

### Tanda 3: luz y desenfoque avanzados
- [x] **Defocus**: desenfoque de lente con diafragma redondo o poligonal y bokeh en las luces (ref.: S_Blur bokeh, Universe Bokeh Blur)
- [x] **Tilt Shift**: banda enfocada con desenfoque progresivo, efecto miniatura (ref.: Universe Tilt Shift)
- [x] **Glow Rings**: anillos de luz alrededor de los brillos, con espectro (ref.: S_GlowRings)
- [x] **Streaks**: glow direccional tipo flare anamórfico (ref.: S_GlowEdges, Universe Glow)
- [x] **Lux**: luz puntual o foco visible en neblina, que el propio clip puede tapar (ref.: Trapcode Lux)

### Tanda 4: generadores
- [x] **Clouds**: ruido fractal animado entre dos colores (ref.: S_Clouds, Fractal Noise)
- [x] **Aurora**: cortinas de aurora boreal animadas (ref.: S_Aurora)
- [x] **Lightning**: rayo eléctrico entre dos puntos con ramas, núcleo y glow (ref.: S_Zap, Universe Electrify)
- [x] **Gradient**: lineal, radial, cónico y cuatro esquinas (ref.: S_Gradient, S_Ramp)
- [x] **Grid**: rejilla girable con puntos en los cruces (ref.: S_Grid)
- [x] **Sparkles**: campo de estrellas que titilan (ref.: S_Sparkles)

### Tanda 5: retro y estilo
- [x] **Mosaic**: pixelado en cuadrados o hexágonos (ref.: S_Mosaic, Universe Pixelate)
- [x] **Tile Scramble**: baraja teselas de la imagen (ref.: S_TileScramble)
- [x] **ASCII**: la imagen redibujada con caracteres según el brillo (ref.: Universe ASCII)
- [x] **CRT**: tubo de televisión con curvatura, máscara de fósforo y scanlines (ref.: Universe Retrograde, S_TVDamage)
- [x] **Ripple**: ondas concéntricas tipo gota en el agua (ref.: S_Ripple)
- [x] **Swirl**: remolino alrededor de un punto (ref.: S_Swirl)
- [x] **Bulge**: burbuja que amplía o pellizca (ref.: S_WarpBubble)

## Pendiente: efectos temporales
- [ ] **Feedback / Echo / Trails** (ref.: S_Feedback, S_Trails, Universe Echo): necesitan leer fotogramas anteriores, así que antes hay que ampliar el puente OFX para pedir el clip en otros tiempos (`clipGetImage` con tiempo distinto y `getFramesNeeded`).

## Transiciones (contexto OFX "Transition")
- [ ] Soporte del contexto Transition en `src/ofx/OfxPlugin.cpp`
- [ ] Dissolve con glow, Swish Pan, Glitch, Flash, Light Leak, Zoom (ref.: S_DissolveGlow, S_Swish, Universe Transitions)

## Fuera de alcance por ahora
- Partículas 3D (Trapcode Particular, Form) y trazos 3D (3D Stroke): necesitan cámara 3D y escena, mejor como proyecto aparte.
- Denoiser con análisis temporal (Magic Bullet Denoiser).

## Port a After Effects
- [ ] Segundo puente `src/ae/` con el SDK de After Effects (descarga aparte: su licencia no permite incluirlo en el repo). Reutiliza los efectos tal cual: parámetros, 8/16/32 bits (SmartFX) y regiones de render.
- [ ] Recursos PiPL por efecto, generados desde `EffectInfo`.
- [ ] CI: `.aex` para Windows y `.plugin` para macOS cuando el SDK esté disponible como secreto del repositorio.

## Infraestructura
- [ ] Render por GPU (OpenCL/Metal/CUDA a través de las extensiones GPU de OFX) para los efectos pesados (Rays, Edge Rays, Defocus).
- [ ] Overlay en el visor para mover los puntos centrales (interact OFX).
- [ ] Presets por efecto.
- [ ] Firmado y notarización en macOS para distribuir binarios.
