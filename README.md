# GStreamer buffer splitter repository

This git module contains a plugin that includes an element for splitting
buffers in GStreamer.

Example:
```
gst-launch-1.0 --gst-plugin-path=builddir/gst-plugin filesrc location=input.h264 ! \
    buffersplitter ! video/x-h264,alignment=nal,framerate=30/1 ! avdec_h264 ! xvimagesink
```

or if your stream includes Access Unit Delimiters, you could use:
```
gst-launch-1.0 --gst-plugin-path=builddir/gst-plugin filesrc location=input.h264 ! \
    buffersplitter delimiter=0000000109 ! video/x-h264,framerate=30/1,stream-format=byte-stream ! avdec_h264 ! xvimagesink
```

or for a more complex scenario, use both at the same time:
```
gst-launch-1.0 --gst-plugin-path=builddir/gst-plugin filesrc location=input.h264 ! \
    video/x-h264,framerate=30/1,stream-format=byte-stream ! tee \
              ! queue ! buffersplitter delimiter=0000000109 ! avdec_h264 ! xvimagesink \
        tee0. ! queue ! buffersplitter ! video/x-h264,alignment=nal ! avdec_h264 ! xvimagesink
```

Although these examples use h264 streams, the splitter can be used on any format that can be split
on a fixed sequence of bytes.

## License

This code is provided under a MIT license [MIT], which basically means "do
with it as you wish, but don't blame us if it doesn't work". You can use
this code for any project as you wish, under any license as you wish. We
recommend the use of the LGPL [LGPL] license for applications and plugins,
given the minefield of patents the multimedia is nowadays. See our website
for details [Licensing].

## Usage

Configure and build all examples (application and plugins) as such:

    meson builddir
    ninja -C builddir

See <https://mesonbuild.com/Quick-guide.html> on how to install the Meson
build system and ninja.

Once the plugin is built you can either install system-wide it with `sudo ninja
-C builddir install` (however, this will by default go into the `/usr/local`
prefix where it won't be picked up by a `GStreamer` installed from packages, so
you would need to set the `GST_PLUGIN_PATH` environment variable to include or
point to `/usr/local/lib/gstreamer-1.0/` for your plugin to be found by a
from-package `GStreamer`).

Alternatively, you will find your plugin binary in `builddir/gst-plugins/src/`
as `libgstplugin.so` or similar (the extension may vary), so you can also set
the `GST_PLUGIN_PATH` environment variable to the `builddir/gst-plugins/src/`
directory (best to specify an absolute path though).

You can also check if it has been built correctly with:

    gst-inspect-1.0 builddir/gst-plugin/libgstbuffersplitter.so


[MIT]: http://www.opensource.org/licenses/mit-license.php or COPYING.MIT
[LGPL]: http://www.opensource.org/licenses/lgpl-license.php or COPYING.LIB
[Licensing]: https://gstreamer.freedesktop.org/documentation/application-development/appendix/licensing.html
