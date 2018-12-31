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
  'src/control_state.cc',
  'src/controller/controller.cc',
  'src/controller/keyboard.cc',
  'src/fm.cc',
  'src/helpers.cc',
  'src/instrument.cc',
  'src/io.cc',
  'src/main.cc',
]

cc = meson.get_compiler('cpp')
m_dep = cc.find_library('m', required : false)
portaudio_dep = dependency('portaudio-2.0')
sdl2_dep = dependency('sdl2')
sdl2_image_dep = dependency('SDL2_image')
glew_dep = dependency('glew')
boost_dep = dependency('boost', modules : ['filesystem', 'system'])
incdir = include_directories('src', 'external')

# Of course, Macs count as Unices even though they behave differently here.
regular_unices = ['linux', 'cygwin', 'dragonfly', 'freebsd', 'gnu', 'netbsd'] 
if regular_unices.contains(target_machine.system())
  data_path = join_paths([get_option('prefix'), get_option('datadir'), 'cafefm'])
  add_project_arguments('-DDATA_DIRECTORY="'+data_path+'"', language: 'cpp')
endif

executable(
  'cafefm',
  src,
  dependencies: [
    portaudio_dep,
    sdl2_dep,
    sdl2_image_dep,
    glew_dep,
    boost_dep,
    m_dep
  ],
  include_directories: [incdir],
  install: true,
)
