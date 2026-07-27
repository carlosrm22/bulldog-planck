# Historial de cambios

Todos los cambios relevantes de Bulldog Planck se documentarán en este archivo.

El formato está inspirado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/)
y el proyecto utiliza [versionado semántico](https://semver.org/lang/es/).

## [Sin publicar]

### Añadido

- Integración de `running` como carrera de frente.
- Integración de `failed` como secuencia para tumbarse, descansar y levantarse.
- Integración de `thinking-work` con seis escenas de trabajo.
- Fundido de 350 ms entre escenas de trabajo, sostenidas durante 1.8 segundos.
- Submenú de acciones para revisar, pensar o trabajar, correr y tumbarse.

### Cambiado

- `review` vuelve a presentarse como la animación breve de revisión.
- Planck permanece tumbado unos 12 segundos antes de volver a levantarse.
- Después de cada acción hay una pausa tranquila de entre 5 y 9 segundos.

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
