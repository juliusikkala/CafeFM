project(
  'CafeFM',
  ['cpp'],
  default_options : ['cpp_std=c++1z', 'buildtype=debugoptimized']
)

src = [
  'src/main.cc',
  'src/audio.cc'
]

cc = meson.get_compiler('cpp')
m_dep = cc.find_library('m', required : false)
portaudio_dep = dependency('portaudio-2.0')
incdir = include_directories('src')

executable(
  'cafefm',
  src,
  dependencies: [
    portaudio_dep,
    m_dep
  ],
  include_directories: [incdir],
  install: true,
)
