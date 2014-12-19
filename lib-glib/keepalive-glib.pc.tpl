prefix={{PREFIX}}
exec_prefix={{EXEC_PREFIX}}
libdir={{LIBDIR}}
# Note: Expected usage is: #include <keeaplive-glib/xxx.h>
includedir={{INCLUDEDIR}}

Name: libkeepalive-glib
Description: Nemomobile cpu/display keepalive development files for glib apps
Version: {{VERS}}
Libs: -L${libdir} -lkeepalive-glib
Libs.private: -L${libdir} -lkeepalive-glib
Cflags: -I${includedir}
Requires:
