# Contribuir a Bulldog Planck

¡Gracias por querer mejorar la vida de Planck!

## Antes de comenzar

1. Busca si ya existe un issue relacionado.
2. Para cambios grandes, abre primero una propuesta y describe el comportamiento.
3. Mantén cada pull request enfocado en una sola mejora.

## Preparar el entorno

```bash
git clone https://github.com/carlosrm22/bulldog-planck.git
cd bulldog-planck
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
PLANCK_PET_FRAMES="$PWD/assets/frames" ./build/planck-pet
```

## Reglas para recursos gráficos

- PNG con canal alfa.
- Lienzo de 192 × 208 píxeles.
- El cuerpo debe conservar una posición coherente entre cuadros.
- Nombres ordenables: `estado-01.png`, `estado-02.png`, etc.
- Incluye la procedencia y licencia del recurso.

## Antes de enviar un pull request

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
desktop-file-validate planck-pet.desktop
```

Describe qué cambió, cómo lo probaste y, para cambios visuales, adjunta una
captura o grabación.

## Idioma

La documentación y la conversación principal del proyecto están en español.
Los nombres internos de código pueden permanecer en inglés.
