# CyGameCapture branding: the logo and the application icon.
#
# Header-only on purpose: linking this target only adds this folder to the include path, which is
# where the resource compiler of each binary finds CyGameCaptureBranding.h, CyGameCaptureIcon.rc2
# and the image files they reference. The images are produced by Tools/GenerateBranding.py from
# CyGameCapture_Logo.png and committed, so a build never needs Python.
add_library(CyGameCaptureBranding INTERFACE)
target_include_directories(CyGameCaptureBranding INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
