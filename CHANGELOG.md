# Historial de cambios

Todos los cambios relevantes de Bulldog Planck se documentarán en este archivo.

El formato está inspirado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/)
y el proyecto utiliza [versionado semántico](https://semver.org/lang/es/).

## [Sin publicar]

### Añadido

- Integración de `running` como carrera de frente.
- Integración de `failed` como secuencia para tumbarse, descansar y levantarse.
- Integración de `thinking-work` con seis escenas principales y once cuadros de
  entrada, transición y salida.
- Submenú de acciones para revisar, pensar o trabajar, correr y tumbarse.

### Cambiado

- `review` vuelve a presentarse como la animación breve de revisión.
- Planck permanece tumbado unos 18 segundos antes de volver a levantarse.
- Después de cada acción hay una pausa tranquila de entre 3 y 5 segundos.
- El parpadeo durante el reposo es menos frecuente y mantiene el ojo cerrado
  durante menos tiempo.
- El saludo aparece con mayor frecuencia en la rutina espontánea.
- Los ciclos de caminata mantienen una altura uniforme y el sentido derecho es
  el reflejo exacto del izquierdo.
- `look-a` y `look-b` se reproducen en ida y vuelta para evitar saltos al
  reiniciar la secuencia.
- `idle-05` conserva la identidad visual de la postura de reposo.
- `waiting` mantiene una postura sentada estable, con respiración de un píxel y
  un parpadeo de 120 ms.
- La espera sentada aumenta al 14 % de las decisiones espontáneas, dura entre
  6 y 10 segundos y reduce sus parpadeos al 20 % de las vueltas.
- `running` utiliza un trote frontal coherente con dos poses y un rebote máximo
  de dos píxeles.
- El salto distribuye sus 650 ms entre anticipación, ascenso, ápice, descenso y
  aterrizaje para transmitir más peso.
- El saludo sostiene la pata elevada y la revisión incorpora pausas
  intencionales sin cambiar sus duraciones totales.
- `look-a` y `look-b` parten y regresan a una pose neutral, completan siempre
  sus ciclos de 2.78 segundos y cambian directamente entre cuadros para evitar
  destellos de toda la figura.
- El descanso tumbado dura 19.5 segundos sin duplicar pixmaps en memoria
  y cambia directamente entre poses.
- `thinking-work` conserva sus 10.8 segundos y pasa de seis poses con fundido a
  17 cuadros continuos con transiciones físicas breves.
- Se elimina el motor de fundido alfa para evitar que toda la figura destelle o
  pierda opacidad al cambiar de cuadro en cualquier secuencia.
- Las caminatas laterales amplían uniformemente la silueta y ensanchan el lienzo
  un 30 % sólo mientras duran, conservando el centro y la línea del suelo sin
  hacer que Planck crezca visualmente.
- El reposo frontal espontáneo baja al 4 %; caminar queda en 28 %, esperar
  sentado en 14 %, saludar en 12 %, trabajar en 9 %, revisar en 7 %, cada
  mirada en 6 %, correr de frente y tumbarse en 5 %, y saltar en 4 %.

## [0.1.0] - 2026-07-24

### Añadido

- Primera versión pública de Bulldog Planck.
- Ventana transparente y siempre visible.
- Movimiento horizontal con límites por monitor.
- 73 cuadros gráficos en 11 secuencias.
- Comportamientos aleatorios y acciones mediante clic.
- Menú contextual con pausa, tamaño, posición y autoarranque.
- Persistencia de tamaño y posición.
- Instalación local mediante CMake.

[Sin publicar]: https://github.com/carlosrm22/bulldog-planck/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/carlosrm22/bulldog-planck/releases/tag/v0.1.0
