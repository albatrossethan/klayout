

DESTDIR = $$OUT_PWD/../../..
TARGET = klayout_QtNetwork

include($$PWD/../../../lib.pri)

DEFINES += MAKE_GSI_QTNETWORK_LIBRARY

INCLUDEPATH += $$TL_INC $$GSI_INC $$QTBASIC_INC
DEPENDPATH += $$TL_INC $$GSI_INC $$QTBASIC_INC

LIBS += -L$$DESTDIR -lklayout_tl -lklayout_gsi -lklayout_qtbasic

# because QQbject is used as base class for many classes, we need this:
LIBS += -lklayout_QtCore

SOURCES += \
  gsiDeclQtNetworkAdd.cc

HEADERS += \

include(QtNetwork.pri)

# --- Tahoe / Homebrew Qt5 portability fix -------------------------------
# Some Qt5 builds (e.g. the Homebrew arm64 bottle on macOS Tahoe) are
# configured without OpenSSL and therefore without the "dtls" feature.
# The QDtls headers then hard-fail via QT_REQUIRE_CONFIG(dtls).
# Drop the DTLS bindings in that case; the rest of QtNetwork is fine.
QT_NETWORK_CONFIG_H = $$[QT_INSTALL_HEADERS]/QtNetwork/qtnetwork-config.h
exists($$QT_NETWORK_CONFIG_H) {
  DTLS_LINE = $$cat($$QT_NETWORK_CONFIG_H, lines)
  DTLS_OFF = $$find(DTLS_LINE, "define QT_FEATURE_dtls -1")
  !isEmpty(DTLS_OFF) {
    message("Qt built without DTLS support - excluding QDtls bindings")
    SOURCES -= \
      $$PWD/gsiDeclQDtls.cc \
      $$PWD/gsiDeclQDtlsClientVerifier.cc \
      $$PWD/gsiDeclQDtlsClientVerifier_GeneratorParameters.cc \
      $$PWD/gsiDeclQDtlsError.cc
  }
}

