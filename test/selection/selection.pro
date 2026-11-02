# -*- mode: makefile -*-
#
# qmake .pro file for test/selection: QTest regression tests for the
# selection-safety fixes of the safer-qdirstat fork.
#
# The test links the application sources directly (everything under src/
# except main.cpp), so there is nothing to change in src/src.pro and the
# patch stays rebaseable against upstream.
#
# Build and run from the project toplevel:
#
#     qmake6
#     make
#     QT_QPA_PLATFORM=offscreen make check
#
# or run ./test/selection/test-selection directly (QTest options apply:
# -o results.xml,junitxml, individual test function names, etc.).

!equals( QT_MAJOR_VERSION, 6 ) {
    message( "Trying to use Qt $${QT_VERSION}" )
    error( "This project requires Qt 6!" )
}

TEMPLATE     = app
TARGET       = test-selection
QT          += widgets testlib core5compat
CONFIG      += testcase console
CONFIG      -= app_bundle
LIBS        += -lz

INCLUDEPATH += ../../src
DEPENDPATH  += ../../src

# Same warning switches as src/src.pro: the application sources are compiled
# here as well, and newer GCC (16) reports upstream's DirInfo::locate() hiding
# FileInfo::locate() in every unit otherwise.
QMAKE_CXXFLAGS += -Wno-overloaded-virtual \
                  -Wno-sfinae-incomplete
MOC_DIR      = .moc
OBJECTS_DIR  = .obj
UI_DIR       = .ui
RCC_DIR      = .rcc

# All application sources except main.cpp. An in-tree build of src/ leaves
# generated files (qrc_*.cpp, moc_*.cpp, ui_*.h) next to the sources; those
# are generated here again and must not be picked up twice.
APP_SRC   = $$files( ../../src/*.cpp )
APP_SRC  -= ../../src/main.cpp
APP_SRC  -= $$files( ../../src/qrc_*.cpp )
APP_SRC  -= $$files( ../../src/moc_*.cpp )

APP_HDR   = $$files( ../../src/*.h )
APP_HDR  -= $$files( ../../src/ui_*.h )

SOURCES   = $$APP_SRC test-selection.cpp
HEADERS   = $$APP_HDR
FORMS     = $$files( ../../src/*.ui )
RESOURCES = ../../src/icons.qrc
