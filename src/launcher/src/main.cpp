#include "mainDialog.h"
#include "launcherTheme.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

// The experimental Windows archive ships this file next to launcher.exe. Loading
// it here gives a direct launch the same environment as Launch-U59.ps1, while
// ordinary Kyty installations without the file retain their existing behavior.
static bool ApplyBundledPreset() {
	const QString preset_path = QDir(QApplication::applicationDirPath()).filePath("u59-preset.json");
	if (!QFile::exists(preset_path)) {
		return true;
	}

	QFile preset(preset_path);
	if (!preset.open(QIODevice::ReadOnly)) {
		qCritical("Cannot open bundled launcher preset");
		return false;
	}
	QJsonParseError error;
	const QJsonDocument document = QJsonDocument::fromJson(preset.readAll(), &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		qCritical("Invalid bundled launcher preset");
		return false;
	}
	const QJsonObject values = document.object();
	for (auto it = values.begin(); it != values.end(); ++it) {
		if (!it.key().startsWith("KYTY_") && !it.key().startsWith("TRACY_")) {
			qCritical("Invalid bundled launcher preset key");
			return false;
		}
		if (!it.value().isString()) {
			qCritical("Invalid bundled launcher preset value");
			return false;
		}
	}

	for (const QString& key : QProcessEnvironment::systemEnvironment().keys()) {
		if (key.startsWith("KYTY_", Qt::CaseInsensitive) ||
		    key.startsWith("TRACY_", Qt::CaseInsensitive)) {
			qunsetenv(key.toLocal8Bit().constData());
		}
	}
	for (auto it = values.begin(); it != values.end(); ++it) {
		qputenv(it.key().toUtf8().constData(), it.value().toString().toUtf8());
	}
	return true;
}

int main(int argc, char* argv[]) {
	QApplication a(argc, argv);
	if (!ApplyBundledPreset()) {
		return 1;
	}
	LauncherTheme::Initialize(a);

	MainDialog w;

	w.emit Start();

	w.show();

	return QApplication::exec();
}
