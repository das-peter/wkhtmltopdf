load(qttest_p4)
QT = core

contains(QT_CONFIG,dbus): {
	SOURCES += tst_qdbuslocalcalls.cpp
	QT += dbus
} else {
	SOURCES += ../qdbusmarshall/dummy.cpp
}

macx:CONFIG += insignificant_test # QTBUG-37469
