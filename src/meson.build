project('wayland-app-base', ['c', 'cpp'])

sources = [
    'app.cpp', 'debug.cpp', 'draw.cpp', 'frame.cpp', 'main.cpp',
    'xdg-shell-protocol.c', 'zxdg-decoration-protocol.c' ]

dep_wayland = dependency('wayland')

executable('app', sources, dependencies: [ dep_wayland ])
