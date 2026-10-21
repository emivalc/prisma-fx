# Hoja de ruta

Efectos tipo Sapphire agrupados por familia, en el orden en que tiene sentido hacerlos. Cada uno se apoya en las piezas de los anteriores.

## Hecho (v0.1)
- [x] Glow: bloom multiescala
- [x] Rays: rayos volumétricos
- [x] Prism: aberración cromática espectral
- [x] Film Grain: grano
- [x] Glitch: daño digital
- [x] Vignette: el ejemplo de la guía

## Luz
- [ ] **Glint**: destellos en estrella sobre los brillos (N puntas, longitud, rotación). Reutiliza el pase de brillos y `directionalBlur`.
- [ ] **Edge Rays**: rayos que salen de los bordes del alfa o de un mate.
- [ ] **Lens Flare**: flare paramétrico con elementos (halo, anillos, streak anamórfico) y punto de luz animable.
- [ ] **Light Leaks**: fugas de luz orgánicas con ruido animado.
- [ ] **Glow Rings / Aura**: halos con forma.

## Blur y desenfoque
- [ ] **Blur** de calidad con aspecto y ángulo.
- [ ] **Zoom Blur** y **Radial Blur**.
- [ ] **Defocus / Bokeh**: desenfoque de lente con formas de diafragma (necesitará FFT o scatter por GPU).

## Estilo y película
- [ ] **Film Effect**: grano, gate weave, parpadeo, polvo y rayas.
- [ ] **Halftone** y **Dot Screen**.
- [ ] **VHS / Analog**: chroma bleed, tracking y ruido de cabeza.
- [ ] **Edge Detect / Cartoon**.

## Distorsión
- [ ] **Warp / Distort** con mapa de ruido.
- [ ] **Shake**: temblor de cámara con desenfoque de movimiento.
- [ ] **Kaleidoscope** y **Mirror**.
- [ ] **Lens Distort**: barril y cojín.

## Transiciones (contexto OFX "Transition")
- [ ] Dissolve con glow, swish pan, glitch transition, flash.

## Infraestructura
- [ ] Render por GPU (OpenCL/Metal/CUDA a través de las extensiones GPU de OFX) para los efectos pesados (Rays, Defocus).
- [ ] Overlay en el visor para mover los puntos centrales (`kOfxParamCoordinatesNormalised` + interact).
- [ ] Presets por efecto.
- [ ] Firmado y notarización en macOS para distribuir binarios.
