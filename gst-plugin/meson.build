plugin_c_args = ['-DHAVE_CONFIG_H']

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'gst-buffer-manipulator')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_API_VERSION', api_version)
cdata.set_quoted('GST_PACKAGE_NAME', 'GStreamer buffer manipulating Plug-ins')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://gstreamer.freedesktop.org')
configure_file(output : 'config.h', configuration : cdata)

# The buffermanipulator Plugin
 gstbuffermanipulator_sources = [
  'src/gstbuffermanipulator.c',
  'src/gstbuffersplitter.c',
  'src/gstbufferjoiner.c',
  ]

gstbuffermanipulator = library('gstbuffermanipulator',
  gstbuffermanipulator_sources,
  c_args: plugin_c_args,
  dependencies : [gst_dep, gstbase_dep],
  install : true,
  install_dir : plugins_install_dir,
)
