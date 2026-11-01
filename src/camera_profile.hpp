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

    bool setProfilerSettings(const QString &name, const QString &method, int port, int localPort,
        int gapMs, int resetMs, int timeoutMs, int address, QString *error = nullptr);
    QString profileVersion() const;
    bool setProfileVersion(const QString &version, QString *error=nullptr);
    bool setLocalPortPolicy(bool sameAsCamera, QString *error=nullptr);
    bool setIdentity(const QString &manufacturer,const QString &model,QString *error=nullptr);
    bool newDraft(QString *error = nullptr);
    QString networkMethod() const;
    QJsonObject networkConnection() const;
    bool supportsRawViscaUdp() const;
    bool supportsPanasonicHttp() const;
    bool renderTemplates(const QStringList &, const ViscaTemplateValues &, QVector<QByteArray> *, QString * = nullptr) const;
    bool profileOwnsConnection() const;
	bool setSonyUdpSettings(int port, int localPort, int gapMs, int resetMs, int timeoutMs, QString *error = nullptr);
	QString path() const { return path_; }
	bool isValid() const { return !root_.isEmpty(); }
	QString id() const;
	QString displayName() const;
	int presetCapacity() const;
	int nativeZoomMaximum() const;
	bool supportsRawViscaTcp() const;
    bool supportsSonyViscaUdp() const;
    bool supportsRuntimeConnection() const { return supportsRawViscaTcp() || supportsSonyViscaUdp() || supportsRawViscaUdp() || supportsPanasonicHttp(); }
    QJsonObject sonyUdpConnection() const;
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
    bool explicitDisplayName_ = false;
};

bool renderViscaTemplate(const QString &commandTemplate, const ViscaTemplateValues &values,
			 QByteArray *packet, QString *error = nullptr);
bool renderViscaTemplates(const QStringList &templates, const ViscaTemplateValues &values,
			  QVector<QByteArray> *packets, QString *error = nullptr);
bool viscaCommandWaitsForCompletion(const QString &commandId);

} // namespace simplecam

