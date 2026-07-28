<p align="center">
  <img src="assets/frames/waving/waving-02.png" width="190" alt="Planck, un bulldog inglés, saludando con la pata">
</p>

<h1 align="center">Bulldog Planck</h1>

<p align="center">
  Una mascota animada que camina, descansa y te acompaña por el escritorio Linux.
</p>

<p align="center">
  <a href="https://github.com/carlosrm22/bulldog-planck/actions/workflows/build.yml"><img src="https://github.com/carlosrm22/bulldog-planck/actions/workflows/build.yml/badge.svg" alt="Compilación"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/licencia-MIT-1D71B8.svg" alt="Licencia MIT"></a>
  <img src="https://img.shields.io/badge/Qt-6.4%2B-41CD52.svg" alt="Qt 6.4 o posterior">
</p>

Planck es un bulldog inglés que vive en una ventana transparente, camina por el
borde inferior de la pantalla y alterna espontáneamente entre diferentes
comportamientos. Puedes saludarlo, hacerlo saltar, moverlo de sitio o simplemente
dejarlo hacer compañía mientras trabajas.

La aplicación nació como una personalización casera inspirada en los pequeños
acompañantes de escritorio y en el plasmoide CatWalk de KDE.

## Características

- Ventana transparente, sin bordes y siempre visible.
- Movimiento real hacia la izquierda y la derecha.
- 79 cuadros PNG con transparencia, distribuidos en 12 secuencias.
- Comportamientos aleatorios: caminar, correr de frente, descansar, mirar,
  esperar, pensar, trabajar, revisar, tumbarse, saludar y saltar.
- Ritmo tranquilo: Planck reposa entre 5 y 9 segundos después de cada acción.
- Parpadeos esporádicos y breves; caminar y saludar son sus gestos más habituales.
- Tres tamaños seleccionables.
- Posición y tamaño persistentes entre ejecuciones.
- Soporte para varios monitores.
- Inicio automático opcional.
- Protección contra instancias duplicadas.
- Sin telemetría, anuncios, cuentas ni conexiones de red.

## Compatibilidad

La versión inicial está desarrollada y probada en:

- Linux.
- KDE Plasma 6.
- Sesión Wayland con XWayland.
- Qt 6.4 o posterior.

Planck utiliza deliberadamente el backend XCB de Qt. Wayland impide que una
ventana convencional cambie libremente su posición; XWayland permite que Planck
camine sin requerir extensiones de KWin ni permisos especiales.

Otros escritorios Linux con X11 o XWayland podrían funcionar, pero todavía no
forman parte de las pruebas oficiales.

## Instalación rápida

Necesitas un compilador de C++ compatible con C++20, CMake, Qt 6 Widgets y
XWayland.

En Fedora:

```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel xorg-x11-server-Xwayland
```

En Debian o Ubuntu:

```bash
sudo apt install build-essential cmake qt6-base-dev xwayland
```

Después:

```bash
git clone https://github.com/carlosrm22/bulldog-planck.git
cd bulldog-planck
./scripts/install.sh
```

Planck aparecerá en el lanzador de aplicaciones. También puedes iniciarlo desde
una terminal:

```bash
planck-pet
```

## Controles

| Acción | Resultado |
| --- | --- |
| Clic izquierdo | Planck saluda |
| Doble clic | Planck salta |
| Arrastrar | Cambia su posición y altura |
| Clic derecho | Abre el menú de controles |

Desde el menú contextual puedes pausarlo, cambiar su tamaño, devolverlo al borde
inferior, activar el inicio con la sesión o salir. El submenú **Acciones**
permite reproducir manualmente cualquiera de sus comportamientos especiales.

## Animaciones incluidas

| Secuencia | Cuadros | Uso actual |
| --- | ---: | --- |
| `idle` | 6 | Reposo automático |
| `running-left` | 8 | Caminar hacia la izquierda |
| `running-right` | 8 | Caminar hacia la derecha, como espejo exacto |
| `look-a` | 8 | Mirar alrededor en ida y vuelta |
| `look-b` | 8 | Mirar y bajar la cabeza en ida y vuelta |
| `waiting` | 6 | Esperar sentado, respirar y parpadear |
| `review` | 6 | Observar o revisar |
| `thinking-work` | 6 | Pensar, investigar y trabajar |
| `waving` | 4 | Saludar |
| `jumping` | 5 | Saltar |
| `running` | 6 | Trotar de frente |
| `failed` | 8 | Tumbarse, descansar y volver a levantarse |

`thinking-work` cambia de escena cada 1.8 segundos y utiliza un fundido de
350 milisegundos. Sus poses —laptop, escritura, pensamiento, investigación,
revisión y tableta— son escenas conceptuales, no cuadros de movimiento continuo.

Los ciclos laterales comparten exactamente las mismas poses reflejadas y
mantienen las patas alineadas con el borde inferior. Las dos secuencias de mirada
regresan por sus propios cuadros para evitar un salto brusco al comenzar de nuevo.

Durante `waiting`, Planck conserva una sola postura sentada, respira con un
desplazamiento de un píxel y cierra los ojos durante 120 milisegundos. Así evita
los saltos de cuerpo y perspectiva que tenía la secuencia original. Esta espera
representa ahora el 10 % de sus decisiones espontáneas.

El trote frontal alterna dos poses coherentes con un rebote vertical máximo de
dos píxeles, manteniendo fija la identidad y el centro horizontal de Planck.

`failed` conserva su nombre de archivo por compatibilidad con el paquete
gráfico, pero dentro de la mascota representa un descanso: Planck se tumba, se
queda quieto unos 12 segundos y vuelve a levantarse.

## Actualizar

Dentro de tu copia del repositorio:

```bash
git pull
./scripts/install.sh
```

## Desinstalar

```bash
./scripts/uninstall.sh
```

La configuración personal se conserva. Si también quieres borrarla:

```bash
rm -f ~/.config/Carlos/PlanckPet.conf
```

## Desarrollo

Compilación local:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
PLANCK_PET_FRAMES="$PWD/assets/frames" ./build/planck-pet
```

El programa precarga todos los cuadros al comenzar. Cada secuencia define su
propio intervalo de animación y la máquina de comportamiento elige el siguiente
estado mediante probabilidades sencillas.

Consulta [CONTRIBUTING.md](CONTRIBUTING.md) antes de enviar cambios.

## Próximos pasos

- Controles para velocidad de caminata.
- Ajustes para frecuencia y duración de comportamientos.
- Nuevos estados: dormir y jugar.
- Selección de monitor y recorrido.
- Paquetes AppImage, RPM y DEB.
- Traducciones y pruebas en otros escritorios.

## Autor y agradecimientos

Creado por [Carlos Alfonso Romero Muñoz](https://itinnitus.com).

Visita [itinnitus.com](https://itinnitus.com) para conocer otros proyectos,
música, libros y trabajos del autor.

## Licencia

El código, el personaje y los recursos gráficos incluidos se distribuyen bajo la
[Licencia MIT](LICENSE).
