#include "camera_profile.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>

#define CHECK(condition)                                                                                              \
	do {                                                                                                            \
		if (!(condition)) {                                                                                     \
			std::cerr << "Check failed at line " << __LINE__ << ": " #condition << std::endl;             \
			return 1;                                                                                         \
		}                                                                                                       \
	} while (false)

using simplecam::CameraProfileDocument;
using simplecam::ViscaTemplateValues;

static QByteArray render(const QString &text, const ViscaTemplateValues &values)
{
	QByteArray packet;
	QString error;
	const bool ok = simplecam::renderViscaTemplate(text, values, &packet, &error);
	if (!ok) {
		std::cerr << error.toStdString() << std::endl;
		return {};
	}
	return packet;
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	ViscaTemplateValues values;
	values.cameraAddress = 1;
	values.panSpeed = 8;
	values.tiltSpeed = 9;
	values.panDirection = 1;
	values.tiltDirection = 3;
	values.zoomSpeed = 8;
	values.zoomPosition = 0x1234;
	values.physicalPreset = 1;

	CHECK(render(QStringLiteral("8{A} 01 04 3F 02 {PI} FF"), values) ==
	       QByteArray::fromHex("8101043f0200ff"));
	CHECK(render(QStringLiteral("8{A} 01 04 47 {ZP} FF"), values) ==
	       QByteArray::fromHex("8101044701020304ff"));
	CHECK(render(QStringLiteral("8{A} 01 04 07 2{ZS} FF"), values) ==
	       QByteArray::fromHex("8101040728ff"));
	CHECK(render(QStringLiteral("8{A} 01 06 01 {PS} {TS} {PD} {TD} FF"), values) ==
	       QByteArray::fromHex("8101060108090103ff"));
	CHECK(render(QStringLiteral("8{A}, 01, 04, 3F, 02, {PI}, FF"), values) ==
	       QByteArray::fromHex("8101043f0200ff"));

	values.physicalPreset = 255;
	CHECK(render(QStringLiteral("8{A} 01 04 3F 02 {PI} FF"), values) ==
	       QByteArray::fromHex("8101043f02feff"));

	QByteArray rejected;
	QString error;
	CHECK(!simplecam::renderViscaTemplate(QStringLiteral("8{NOPE} 01 04 00 02 FF"), values, &rejected, &error));
	CHECK(!error.isEmpty());
	CHECK(!simplecam::renderViscaTemplate(QStringLiteral("81 01 04 ZZ FF"), values, &rejected, &error));
	values.cameraAddress = 8;
	CHECK(!simplecam::renderViscaTemplate(QStringLiteral("8{A} 01 04 00 02 FF"), values, &rejected, &error));
	values.cameraAddress = 1;

	CameraProfileDocument profile;
	CHECK(profile.load(QString::fromLocal8Bit(argv[1]), &error));
	CHECK(profile.id() == QStringLiteral("avipas.av-1280w.visca"));
	CHECK(profile.presetCapacity() == 255);
	CHECK(profile.supportsRawViscaTcp());
	CHECK(profile.command(QStringLiteral("presetStore")).templates.size() == 1);
	CHECK(profile.command(QStringLiteral("powerInquiry")).templates.size() == 1);
	auto renderCommand = [&profile, &values](const QString &id) {
		const QStringList templates = profile.command(id).templates;
		if (templates.size() != 1)
			return QByteArray();
		return render(templates.first(), values);
	};
	CHECK(renderCommand(QStringLiteral("home")) == QByteArray::fromHex("81010604ff"));
	CHECK(renderCommand(QStringLiteral("movePanTilt")) == QByteArray::fromHex("8101060108090103ff"));
	CHECK(renderCommand(QStringLiteral("stopPanTilt")) == QByteArray::fromHex("8101060108090303ff"));
	CHECK(renderCommand(QStringLiteral("zoomIn")) == QByteArray::fromHex("8101040728ff"));
	CHECK(renderCommand(QStringLiteral("zoomOut")) == QByteArray::fromHex("8101040738ff"));
	CHECK(renderCommand(QStringLiteral("zoomStop")) == QByteArray::fromHex("8101040700ff"));
	CHECK(renderCommand(QStringLiteral("focusNear")) == QByteArray::fromHex("8101040803ff"));
	CHECK(renderCommand(QStringLiteral("focusFar")) == QByteArray::fromHex("8101040802ff"));
	CHECK(renderCommand(QStringLiteral("focusStop")) == QByteArray::fromHex("8101040800ff"));
	CHECK(renderCommand(QStringLiteral("focusAuto")) == QByteArray::fromHex("8101043802ff"));
	CHECK(renderCommand(QStringLiteral("focusManual")) == QByteArray::fromHex("8101043803ff"));
	CHECK(renderCommand(QStringLiteral("whiteBalanceAuto")) == QByteArray::fromHex("8101043500ff"));
	CHECK(renderCommand(QStringLiteral("whiteBalanceManual")) == QByteArray::fromHex("8101043505ff"));
	CHECK(renderCommand(QStringLiteral("backlightOn")) == QByteArray::fromHex("8101043302ff"));
	CHECK(renderCommand(QStringLiteral("backlightOff")) == QByteArray::fromHex("8101043303ff"));
	CHECK(renderCommand(QStringLiteral("exposureCompOn")) == QByteArray::fromHex("8101043e02ff"));
	CHECK(renderCommand(QStringLiteral("exposureCompOff")) == QByteArray::fromHex("8101043e03ff"));
	CHECK(renderCommand(QStringLiteral("powerOn")) == QByteArray::fromHex("8101040002ff"));
	CHECK(renderCommand(QStringLiteral("powerOff")) == QByteArray::fromHex("8101040003ff"));
	CHECK(renderCommand(QStringLiteral("zoomInquiry")) == QByteArray::fromHex("81090447ff"));
	CHECK(simplecam::viscaCommandWaitsForCompletion(QStringLiteral("home")));
	CHECK(!simplecam::viscaCommandWaitsForCompletion(QStringLiteral("focusNear")));
	CHECK(!simplecam::viscaCommandWaitsForCompletion(QStringLiteral("powerOff")));
	CHECK(render(QStringLiteral("8{A} 09 04 00 FF"), values) == QByteArray::fromHex("81090400ff"));
	QVector<QByteArray> storePackets;
	CHECK(simplecam::renderViscaTemplates(profile.command(QStringLiteral("presetStore")).templates,
						values, &storePackets, &error));
	CHECK(storePackets.size() == 1);
	CHECK(storePackets.at(0) == QByteArray::fromHex("8101043f01feff"));

	CHECK(!profile.commandIds().contains(QStringLiteral("presetSpeedSet")));
	const QJsonObject speedCapability = profile.root().value(QStringLiteral("capabilities")).toObject()
						.value(QStringLiteral("operations")).toObject()
						.value(QStringLiteral("presetRecallSpeed")).toObject();
	CHECK(!speedCapability.value(QStringLiteral("supported")).toBool());

	QVector<QByteArray> clearPackets;
	CHECK(simplecam::renderViscaTemplates(profile.command(QStringLiteral("presetClear")).templates,
						values, &clearPackets, &error));
	CHECK(clearPackets.size() == 1);
	CHECK(clearPackets.at(0) == QByteArray::fromHex("8101043f00feff"));

	QVector<QByteArray> recallPackets;
	CHECK(simplecam::renderViscaTemplates(profile.command(QStringLiteral("presetRecall")).templates,
						values, &recallPackets, &error));
	CHECK(recallPackets.size() == 1);
	CHECK(recallPackets.at(0) == QByteArray::fromHex("8101043f02feff"));
	CHECK(profile.command(QStringLiteral("zoomInquiry")).templates.size() == 1);

	const QStringList testOrder = profile.commandIdsInTestOrder();
	CHECK(testOrder.first() == QStringLiteral("powerInquiry"));
	CHECK(testOrder.indexOf(QStringLiteral("powerInquiry")) < testOrder.indexOf(QStringLiteral("powerOn")));
	CHECK(testOrder.indexOf(QStringLiteral("focusManual")) < testOrder.indexOf(QStringLiteral("focusFar")));
	CHECK(testOrder.indexOf(QStringLiteral("focusManual")) < testOrder.indexOf(QStringLiteral("focusNear")));
	CHECK(testOrder.indexOf(QStringLiteral("whiteBalanceManual")) < testOrder.indexOf(QStringLiteral("whiteBalanceAuto")));
	CHECK(testOrder.indexOf(QStringLiteral("presetStore")) < testOrder.indexOf(QStringLiteral("presetRecall")));
	CHECK(testOrder.indexOf(QStringLiteral("presetRecall")) < testOrder.indexOf(QStringLiteral("presetClear")));
	CHECK(testOrder.last() == QStringLiteral("powerOff"));
	CHECK(profile.setDiscoveredZoomEndpoint(0x4321, QStringLiteral("on"), &error));

	QTemporaryDir temp;
	CHECK(temp.isValid());
	const QString savedPath = temp.filePath(QStringLiteral("AViPAS 1280 Custom.camera.cfg"));
	CHECK(profile.saveAs(savedPath, &error));
	CameraProfileDocument copy;
	CHECK(copy.load(savedPath, &error));
	CHECK(copy.id() == QStringLiteral("user.avipas-1280-custom"));
	CHECK(copy.displayName() == QStringLiteral("AViPAS 1280 Custom"));
	CHECK(copy.root().value(QStringLiteral("derivedFrom")).toString() == QStringLiteral("avipas.av-1280w.visca"));
	const QJsonObject zoomRange = copy.root().value(QStringLiteral("capabilities")).toObject()
				      .value(QStringLiteral("limits")).toObject()
				      .value(QStringLiteral("zoom")).toObject()
				      .value(QStringLiteral("nativeRange")).toObject();
	CHECK(zoomRange.value(QStringLiteral("maximum")).toInt() == 0x4321);
	CHECK(zoomRange.value(QStringLiteral("digitalZoomMode")).toString() == QStringLiteral("on"));

	QFile profileFile(QString::fromLocal8Bit(argv[1]));
	CHECK(profileFile.open(QIODevice::ReadOnly));
	QJsonObject invalidRoot = QJsonDocument::fromJson(profileFile.readAll()).object();
	QJsonObject commands = invalidRoot.value(QStringLiteral("commands")).toObject();
	QJsonObject home = commands.value(QStringLiteral("home")).toObject();
	home.insert(QStringLiteral("templateLanguageVersion"), 2);
	commands.insert(QStringLiteral("home"), home);
	invalidRoot.insert(QStringLiteral("commands"), commands);
	CameraProfileDocument unsupported;
	CHECK(!unsupported.loadBytes(QJsonDocument(invalidRoot).toJson(), &error));
	return 0;
}
