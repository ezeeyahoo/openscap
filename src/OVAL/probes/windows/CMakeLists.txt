if(OPENSCAP_PROBE_WINDOWS_ACCESSTOKEN)
	list(APPEND WINDOWS_PROBES_SOURCES
		"accesstoken_probe.c"
		"accesstoken_probe.h"
	)
endif()
if(OPENSCAP_PROBE_WINDOWS_REGISTRY)
	list(APPEND WINDOWS_PROBES_SOURCES
		"registry_probe.c"
		"registry_probe.h"
	)
endif()
if(OPENSCAP_PROBE_WINDOWS_WMI57)
	list(APPEND WINDOWS_PROBES_SOURCES
		"wmi57_probe.c"
		"wmi57_probe.h"
	)
endif()

add_library(windows_probes_object OBJECT ${WINDOWS_PROBES_SOURCES})
set_oscap_generic_properties(windows_probes_object)
target_include_directories(windows_probes_object PRIVATE ${WINDOWS_PROBES_INCLUDE_DIRECTORIES})
