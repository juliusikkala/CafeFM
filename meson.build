project(
  'CafeFM',
  ['cpp'],
  default_options : ['cpp_std=c++1z', 'buildtype=debugoptimized']
)

src = [
  'external/nuklear.cc',
  'src/audio.cc',
  'src/bindings.cc',
  'src/cafefm.cc',
  'src/control_context.cc',
  'src/controller/controller.cc',
  'src/controller/keyboard.cc',
  'src/fm.cc',
  'src/instrument.cc',
  'src/main.cc',
]

cc = meson.get_compiler('cpp')
m_dep = cc.find_library('m', required : false)
portaudio_dep = dependency('portaudio-2.0')
sdl2_dep = dependency('sdl2')
sdl2_image_dep = dependency('SDL2_image')
glew_dep = dependency('glew')
incdir = include_directories('src', 'external')

executable(
  'cafefm',
  src,
  dependencies: [
    portaudio_dep,
    sdl2_dep,
    sdl2_image_dep,
    glew_dep,
    m_dep
  ],
  include_directories: [incdir],
  install: true,
)
