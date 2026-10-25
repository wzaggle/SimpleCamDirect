#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace simplecam {

struct CameraProfileCommand {
	QString id;
	QString driverOperation;
	QStringList templates;
	bool destructive = false;
};

struct ViscaTemplateValues {
	int cameraAddress = 1;
	int panSpeed = 8;
	int tiltSpeed = 8;
	int panDirection = 1;
	int tiltDirection = 3;
	int zoomSpeed = 8;
	int zoomPosition = 0;
	int physicalPreset = 1;
};

class CameraProfileDocument final {
public:
	bool load(const QString &path, QString *error = nullptr);
	bool loadBytes(const QByteArray &bytes, QString *error = nullptr);
	bool saveAs(const QString &path, QString *error = nullptr);

	QString path() const { return path_; }
	bool isValid() const { return !root_.isEmpty(); }
	QString id() const;
	QString displayName() const;
	int presetCapacity() const;
	int nativeZoomMaximum() const;
	bool supportsRawViscaTcp() const;
	QJsonObject root() const { return root_; }
	QStringList connectionIds() const;
	QString connectionLabel(const QString &id) const;
	QString connectionTransport(const QString &id) const;
	QString connectionFraming(const QString &id) const;
	QStringList commandIds() const;
	QStringList commandIdsInTestOrder() const;
	CameraProfileCommand command(const QString &id) const;
	bool setCommandTemplates(const QString &id, const QStringList &templates, QString *error = nullptr);
	bool setDiscoveredZoomEndpoint(int maximum, const QString &digitalZoomMode, QString *error = nullptr);

private:
	bool parseRoot(const QJsonObject &root, QString *error);
	QJsonObject root_;
	QString path_;
};

bool renderViscaTemplate(const QString &commandTemplate, const ViscaTemplateValues &values,
			 QByteArray *packet, QString *error = nullptr);
bool renderViscaTemplates(const QStringList &templates, const ViscaTemplateValues &values,
			  QVector<QByteArray> *packets, QString *error = nullptr);
bool viscaCommandWaitsForCompletion(const QString &commandId);

} // namespace simplecam
