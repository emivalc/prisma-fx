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

## Tanda 2: película y color
- [ ] **Film Effect**: gate weave, parpadeo, polvo, pelos y rayas (ref.: S_FilmEffect, Magic Bullet Film)
- [ ] **Film Stock / Look**: curva de película y halation (ref.: Magic Bullet Film)
- [ ] **Duotone / Tritone / Gradient Map** (ref.: S_Duotone, S_Tritone)
- [ ] **Posterize, Threshold, Solarize** (ref.: S_Posterize, S_Threshold)
- [ ] **Skin Smooth**: suavizado que conserva bordes (ref.: Magic Bullet Cosmo)
- [ ] **Sharpen** y **Unsharp** (ref.: S_Sharpen)
- [ ] **Mojo**: separación cálido/frío de tonos de piel (ref.: Magic Bullet Mojo)

## Tanda 3: luz y desenfoque avanzados
- [ ] **Defocus / Bokeh** con formas de diafragma (ref.: S_Blur con bokeh, Universe Bokeh Blur)
- [ ] **Glow Rings / Aura** (ref.: S_GlowRings, S_GlowAura)
- [ ] **Directional Glow / Streaks** (ref.: S_GlowEdges, Universe Glow)
- [ ] **Lux**: luz volumétrica desde un punto (ref.: Trapcode Lux)
- [ ] **Tilt Shift** (ref.: Universe Tilt Shift)

## Tanda 4: generadores
- [ ] **Clouds / Fractal Noise** (ref.: S_Clouds)
- [ ] **Aurora** (ref.: S_Aurora)
- [ ] **Zap / Lightning** con puntos animables (ref.: S_Zap, Universe Electrify)
- [ ] **Gradients y Grid** (ref.: S_Gradient, S_Grid)
- [ ] **Stars / Sparkles**

## Tanda 5: estilo y retro
- [ ] **Mosaic / Pixelate** y **Tile Scramble** (ref.: S_Mosaic, S_TileScramble)
- [ ] **CRT / Retrograde** con curvatura y máscara de fósforo (ref.: Universe Retrograde)
- [ ] **ASCII / Text Mosaic**
- [ ] **Feedback / Echo** temporal (ref.: S_Feedback, S_Trails; necesita leer otros fotogramas)
- [ ] **Warp Bubble / Ripple / Swirl** (ref.: S_WarpBubble, S_Swirl)

## Transiciones (contexto OFX "Transition")
- [ ] Soporte del contexto Transition en `src/ofx/OfxPlugin.cpp`
- [ ] Dissolve con glow, Swish Pan, Glitch, Flash, Light Leak, Zoom (ref.: S_DissolveGlow, S_Swish, Universe Transitions)

## Fuera de alcance por ahora
- Partículas 3D (Trapcode Particular, Form) y trazos 3D (3D Stroke): necesitan cámara 3D y escena, mejor como proyecto aparte.
- Denoiser con análisis temporal (Magic Bullet Denoiser).

## Infraestructura
- [ ] Render por GPU (OpenCL/Metal/CUDA a través de las extensiones GPU de OFX) para los efectos pesados (Rays, Edge Rays, Defocus).
- [ ] Overlay en el visor para mover los puntos centrales (interact OFX).
- [ ] Presets por efecto.
- [ ] Firmado y notarización en macOS para distribuir binarios.
