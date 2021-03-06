project(
  'CafeFM',
  ['cpp', 'c'],
  default_options : [
    'cpp_std=c++1z',
    'buildtype=debugoptimized',
    'warning_level=3',
    'werror=true'
  ]
)

src = [
  'external/nuklear.cc',
  'external/pffft.c',
  'src/audio.cc',
  'src/bindings.cc',
  'src/cafefm.cc',
  'src/control_state.cc',
  'src/controller/controller.cc',
  'src/controller/keyboard.cc',
  'src/controller/gamecontroller.cc',
  'src/controller/joystick.cc',
  'src/controller/microphone.cc',
  'src/controller/midi.cc',
  'src/encoder.cc',
  'src/filter.cc',
  'src/fm.cc',
  'src/helpers.cc',
  'src/instrument.cc',
  'src/instrument_state.cc',
  'src/io.cc',
  'src/looper.cc',
  'src/main.cc',
  'src/mimicker.cc',
  'src/options.cc',
  'src/visualizer.cc',
]

cc = meson.get_compiler('cpp')
m_dep = cc.find_library('m', required : false)
portaudio_dep = dependency('portaudio-2.0')
sdl2_dep = dependency('sdl2')
sdl2_image_dep = dependency('SDL2_image')
glew_dep = dependency('glew')
rtmidi_dep = dependency('rtmidi')
sndfile_dep = dependency('sndfile')
boost_dep = dependency('boost', modules : ['filesystem', 'system'])
incdir = include_directories('src', 'external')

# Of course, Macs count as Unices even though they behave differently here.
regular_unices = ['linux', 'cygwin', 'dragonfly', 'freebsd', 'gnu', 'netbsd'] 
if regular_unices.contains(target_machine.system())
  data_path = join_paths([get_option('prefix'), get_option('datadir'), 'cafefm'])
  add_project_arguments('-DDATA_DIRECTORY="'+data_path+'"', language: 'cpp')
  add_project_arguments('-DUSE_XDG', language: 'cpp')
endif

executable(
  'cafefm',
  src,
  dependencies: [
    portaudio_dep,
    sdl2_dep,
    sdl2_image_dep,
    glew_dep,
    rtmidi_dep,
    sndfile_dep,
    boost_dep,
    m_dep
  ],
  include_directories: [incdir],
  install: true,
)
