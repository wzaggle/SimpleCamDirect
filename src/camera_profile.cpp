#include "camera_profile.hpp"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace simplecam {
namespace {

void setError(QString *error, const QString &message)
{
	if (error)
		*error = message;
}

QString hexByte(int value)
{
	return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

QString nibble(int value)
{
	return QString::number(value, 16).toUpper();
}

bool validRange(int value, int minimum, int maximum, const QString &token, QString *error)
{
	if (value < minimum || value > maximum) {
		setError(error, QStringLiteral("Token {%1} value %2 is outside %3..%4")
				.arg(token).arg(value).arg(minimum).arg(maximum));
		return false;
	}
	return true;
}

bool expandTokens(QString *text, const ViscaTemplateValues &v, QString *error)
{
	if (!validRange(v.cameraAddress, 1, 7, QStringLiteral("A"), error) ||
	    !validRange(v.panSpeed, 1, 24, QStringLiteral("PS"), error) ||
	    !validRange(v.tiltSpeed, 1, 20, QStringLiteral("TS"), error) ||
	    !validRange(v.panDirection, 1, 3, QStringLiteral("PD"), error) ||
	    !validRange(v.tiltDirection, 1, 3, QStringLiteral("TD"), error) ||
	    !validRange(v.zoomSpeed, 0, 15, QStringLiteral("ZS"), error) ||
	    !validRange(v.zoomPosition, 0, 0xFFFF, QStringLiteral("ZP"), error) ||
	    !validRange(v.physicalPreset, 1, 255, QStringLiteral("PI"), error))
		return false;

	const int protocolPreset = v.physicalPreset - 1;
	const QString zoom = QStringLiteral("0%1 0%2 0%3 0%4")
			     .arg(nibble((v.zoomPosition >> 12) & 0x0F), nibble((v.zoomPosition >> 8) & 0x0F),
				  nibble((v.zoomPosition >> 4) & 0x0F), nibble(v.zoomPosition & 0x0F));

	const QVector<QPair<QString, QString>> replacements = {
		{QStringLiteral("{A}"), nibble(v.cameraAddress)},
		{QStringLiteral("{PS}"), hexByte(v.panSpeed)},
		{QStringLiteral("{TS}"), hexByte(v.tiltSpeed)},
		{QStringLiteral("{PD}"), hexByte(v.panDirection)},
		{QStringLiteral("{TD}"), hexByte(v.tiltDirection)},
		{QStringLiteral("{ZS}"), nibble(v.zoomSpeed)},
		{QStringLiteral("{ZP}"), zoom},
		{QStringLiteral("{PI}"), hexByte(protocolPreset)},
	};
	for (const auto &replacement : replacements)
		text->replace(replacement.first, replacement.second);

	static const QRegularExpression unknownToken(QStringLiteral(R"(\{[^{}]*\})"));
	const auto match = unknownToken.match(*text);
	if (match.hasMatch()) {
		setError(error, QStringLiteral("Unknown or malformed token: %1").arg(match.captured(0)));
		return false;
	}
	return true;
}

} // namespace

bool CameraProfileDocument::load(const QString &path, QString *error)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		setError(error, QStringLiteral("Could not open profile: %1").arg(file.errorString()));
		return false;
	}
	if (!loadBytes(file.readAll(), error))
		return false;
	path_ = path;
	return true;
}

bool CameraProfileDocument::loadBytes(const QByteArray &bytes, QString *error)
{
	QJsonParseError parseError{};
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
		setError(error, QStringLiteral("Invalid JSON profile: %1").arg(parseError.errorString()));
		return false;
	}
	if (!parseRoot(doc.object(), error))
		return false;
	path_.clear();
	return true;
}

bool CameraProfileDocument::parseRoot(const QJsonObject &root, QString *error)
{
	if (root.value(QStringLiteral("schemaVersion")).toInt(-1) != 1) {
		setError(error, QStringLiteral("Only camera profile schemaVersion 1 is supported"));
		return false;
	}
	const QJsonObject identity = root.value(QStringLiteral("identity")).toObject();
	if (identity.value(QStringLiteral("id")).toString().isEmpty() ||
	    identity.value(QStringLiteral("manufacturer")).toString().isEmpty() ||
	    identity.value(QStringLiteral("model")).toString().isEmpty() ||
	    identity.value(QStringLiteral("displayName")).toString().isEmpty()) {
		setError(error, QStringLiteral("Profile identity must include id, manufacturer, model, and displayName"));
		return false;
	}
	const QJsonObject support = root.value(QStringLiteral("support")).toObject();
	const QString supportStatus = support.value(QStringLiteral("status")).toString();
	if (supportStatus != QStringLiteral("draft") && supportStatus != QStringLiteral("protocol-reviewed") &&
	    supportStatus != QStringLiteral("hardware-tested") && supportStatus != QStringLiteral("maintained")) {
		setError(error, QStringLiteral("Profile support.status is missing or unsupported"));
		return false;
	}
	if (!root.value(QStringLiteral("capabilities")).isObject()) {
		setError(error, QStringLiteral("Profile capabilities must be an object"));
		return false;
	}
	const QJsonArray connections = root.value(QStringLiteral("connections")).toArray();
	if (connections.isEmpty()) {
		setError(error, QStringLiteral("Profile has no connection variants"));
		return false;
	}
	QSet<QString> connectionIds;
	for (const QJsonValue &value : connections) {
		if (!value.isObject()) {
			setError(error, QStringLiteral("Each connection variant must be an object"));
			return false;
		}
		const QJsonObject connection = value.toObject();
		const QString connectionId = connection.value(QStringLiteral("id")).toString();
		if (connectionId.isEmpty() || connection.value(QStringLiteral("label")).toString().isEmpty() ||
		    connection.value(QStringLiteral("transport")).toString().isEmpty() ||
		    connection.value(QStringLiteral("framing")).toString().isEmpty() || connectionIds.contains(connectionId)) {
			setError(error, QStringLiteral("Connection variants need unique ids, labels, transport, and framing"));
			return false;
		}
		if (connection.value(QStringLiteral("protocol")).toString() != QStringLiteral("visca")) {
			setError(error, QStringLiteral("The first Profile Test tool supports VISCA profiles only"));
			return false;
		}
		connectionIds.insert(connectionId);
	}
	const QJsonObject commands = root.value(QStringLiteral("commands")).toObject();
	if (commands.isEmpty()) {
		setError(error, QStringLiteral("Profile has no command bindings"));
		return false;
	}
	for (auto it = commands.begin(); it != commands.end(); ++it) {
		if (!it.value().isObject()) {
			setError(error, QStringLiteral("Command '%1' must be an object").arg(it.key()));
			return false;
		}
		const QJsonObject binding = it.value().toObject();
		if (binding.value(QStringLiteral("driverOperation")).toString().isEmpty()) {
			setError(error, QStringLiteral("Command '%1' is missing driverOperation").arg(it.key()));
			return false;
		}
		const bool hasTemplate = binding.value(QStringLiteral("template")).isString();
		const bool hasSequence = binding.value(QStringLiteral("sequence")).isArray();
		if (hasTemplate == hasSequence) {
			setError(error, QStringLiteral("Command '%1' must have exactly one template or sequence").arg(it.key()));
			return false;
		}
		const QJsonValue languageVersionValue = binding.value(QStringLiteral("templateLanguageVersion"));
		const int languageVersion = languageVersionValue.isUndefined() ? 1 : languageVersionValue.toInt(-1);
		if (languageVersion != 1) {
			setError(error, QStringLiteral("Command '%1' uses unsupported template language version %2")
					.arg(it.key()).arg(languageVersion));
			return false;
		}
		if (hasSequence) {
			const QJsonArray sequence = binding.value(QStringLiteral("sequence")).toArray();
			if (sequence.isEmpty()) {
				setError(error, QStringLiteral("Command '%1' has an empty sequence").arg(it.key()));
				return false;
			}
			for (const QJsonValue &step : sequence) {
				if (!step.isString() || step.toString().trimmed().isEmpty()) {
					setError(error, QStringLiteral("Command '%1' sequence steps must be non-empty strings").arg(it.key()));
					return false;
				}
			}
		}
	}
	root_ = root;
	return true;
}

bool CameraProfileDocument::saveAs(const QString &path, QString *error)
{
	QJsonObject savedRoot = root_;
	QJsonObject identity = savedRoot.value(QStringLiteral("identity")).toObject();
	QString displayName = QFileInfo(path).fileName();
	if (displayName.endsWith(QStringLiteral(".camera.cfg"), Qt::CaseInsensitive))
		displayName.chop(QStringLiteral(".camera.cfg").size());
	else
		displayName = QFileInfo(path).completeBaseName();
	QString baseName = displayName;
	baseName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")), QStringLiteral("-"));
	baseName = baseName.trimmed();
	while (baseName.startsWith(QLatin1Char('-')))
		baseName.remove(0, 1);
	while (baseName.endsWith(QLatin1Char('-')))
		baseName.chop(1);
	if (baseName.isEmpty())
		baseName = QStringLiteral("custom-camera");
	identity.insert(QStringLiteral("id"), QStringLiteral("user.%1").arg(baseName.toLower()));
	identity.insert(QStringLiteral("displayName"), displayName);
	savedRoot.insert(QStringLiteral("identity"), identity);
	QJsonObject support = savedRoot.value(QStringLiteral("support")).toObject();
	support.insert(QStringLiteral("status"), QStringLiteral("draft"));
	savedRoot.insert(QStringLiteral("support"), support);
	savedRoot.insert(QStringLiteral("derivedFrom"), id());
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		setError(error, QStringLiteral("Could not save profile: %1").arg(file.errorString()));
		return false;
	}
	const QByteArray serialized = QJsonDocument(savedRoot).toJson(QJsonDocument::Indented);
	if (file.write(serialized) != serialized.size()) {
		setError(error, QStringLiteral("Could not write profile: %1").arg(file.errorString()));
		return false;
	}
	if (!file.commit()) {
		setError(error, QStringLiteral("Could not commit profile: %1").arg(file.errorString()));
		return false;
	}
	root_ = savedRoot;
	path_ = path;
	return true;
}

QString CameraProfileDocument::id() const
{
	return root_.value(QStringLiteral("identity")).toObject().value(QStringLiteral("id")).toString();
}

QString CameraProfileDocument::displayName() const
{
	return root_.value(QStringLiteral("identity")).toObject().value(QStringLiteral("displayName")).toString();
}

int CameraProfileDocument::presetCapacity() const
{
	const int capacity = root_.value(QStringLiteral("capabilities")).toObject()
			     .value(QStringLiteral("limits")).toObject()
			     .value(QStringLiteral("presetCapacity")).toInt(-1);
	return capacity >= 1 && capacity <= 255 ? capacity : -1;
}

int CameraProfileDocument::nativeZoomMaximum() const
{
	const int maximum = root_.value(QStringLiteral("capabilities")).toObject()
			    .value(QStringLiteral("limits")).toObject()
			    .value(QStringLiteral("zoom")).toObject()
			    .value(QStringLiteral("nativeRange")).toObject()
			    .value(QStringLiteral("maximum")).toInt(-1);
	return maximum >= 0 && maximum <= 0xFFFF ? maximum : -1;
}

bool CameraProfileDocument::supportsRawViscaTcp() const
{
	for (const QString &connectionId : connectionIds()) {
		if (connectionTransport(connectionId) == QStringLiteral("tcp") &&
		    connectionFraming(connectionId) == QStringLiteral("raw-visca")) {
			return true;
		}
	}
	return false;
}

QStringList CameraProfileDocument::connectionIds() const
{
	QStringList ids;
	for (const auto value : root_.value(QStringLiteral("connections")).toArray())
		ids.append(value.toObject().value(QStringLiteral("id")).toString());
	return ids;
}

QString CameraProfileDocument::connectionLabel(const QString &id) const
{
	for (const auto value : root_.value(QStringLiteral("connections")).toArray()) {
		const QJsonObject connection = value.toObject();
		if (connection.value(QStringLiteral("id")).toString() == id)
			return connection.value(QStringLiteral("label")).toString();
	}
	return {};
}

QString CameraProfileDocument::connectionTransport(const QString &id) const
{
	for (const auto value : root_.value(QStringLiteral("connections")).toArray()) {
		const QJsonObject connection = value.toObject();
		if (connection.value(QStringLiteral("id")).toString() == id)
			return connection.value(QStringLiteral("transport")).toString();
	}
	return {};
}

QString CameraProfileDocument::connectionFraming(const QString &id) const
{
	for (const auto value : root_.value(QStringLiteral("connections")).toArray()) {
		const QJsonObject connection = value.toObject();
		if (connection.value(QStringLiteral("id")).toString() == id)
			return connection.value(QStringLiteral("framing")).toString();
	}
	return {};
}

QStringList CameraProfileDocument::commandIds() const
{
	return root_.value(QStringLiteral("commands")).toObject().keys();
}

QStringList CameraProfileDocument::commandIdsInTestOrder() const
{
	const QStringList available = commandIds();
	static const QStringList preferredOrder = {
		QStringLiteral("powerInquiry"), QStringLiteral("powerOn"), QStringLiteral("home"), QStringLiteral("movePanTilt"),
		QStringLiteral("stopPanTilt"), QStringLiteral("zoomIn"), QStringLiteral("zoomOut"),
		QStringLiteral("zoomAbsolute"), QStringLiteral("zoomStop"),
		QStringLiteral("focusManual"), QStringLiteral("focusFar"), QStringLiteral("focusNear"),
		QStringLiteral("focusStop"), QStringLiteral("focusAuto"),
		QStringLiteral("whiteBalanceManual"), QStringLiteral("whiteBalanceAuto"),
		QStringLiteral("backlightOn"), QStringLiteral("backlightOff"),
		QStringLiteral("exposureCompOn"), QStringLiteral("exposureCompOff"),
		QStringLiteral("positionInquiry"), QStringLiteral("presetStore"),
		QStringLiteral("presetSpeedSet"), QStringLiteral("presetRecall"),
		QStringLiteral("zoomInquiry"), QStringLiteral("presetClear")
	};

	QStringList ordered;
	for (const QString &id : preferredOrder) {
		if (available.contains(id))
			ordered.append(id);
	}
	QStringList remaining;
	for (const QString &id : available) {
		if (!ordered.contains(id) && id != QStringLiteral("powerOff"))
			remaining.append(id);
	}
	std::sort(remaining.begin(), remaining.end());
	ordered.append(remaining);
	if (available.contains(QStringLiteral("powerOff")))
		ordered.append(QStringLiteral("powerOff"));
	return ordered;
}

CameraProfileCommand CameraProfileDocument::command(const QString &id) const
{
	CameraProfileCommand result;
	result.id = id;
	const QJsonObject binding = root_.value(QStringLiteral("commands")).toObject().value(id).toObject();
	result.driverOperation = binding.value(QStringLiteral("driverOperation")).toString();
	result.destructive = binding.value(QStringLiteral("destructive")).toBool();
	if (binding.value(QStringLiteral("template")).isString())
		result.templates.append(binding.value(QStringLiteral("template")).toString());
	else {
		for (const QJsonValue step : binding.value(QStringLiteral("sequence")).toArray())
			result.templates.append(step.toString());
	}
	return result;
}

bool CameraProfileDocument::setCommandTemplates(const QString &id, const QStringList &templates, QString *error)
{
	if (templates.isEmpty()) {
		setError(error, QStringLiteral("Command template cannot be empty"));
		return false;
	}
	QJsonObject commands = root_.value(QStringLiteral("commands")).toObject();
	if (!commands.contains(id)) {
		setError(error, QStringLiteral("Unknown profile command '%1'").arg(id));
		return false;
	}
	QJsonObject binding = commands.value(id).toObject();
	if (templates.size() == 1 && binding.contains(QStringLiteral("template"))) {
		binding.insert(QStringLiteral("template"), templates.first());
		binding.remove(QStringLiteral("sequence"));
	} else {
		QJsonArray sequence;
		for (const QString &line : templates) {
			if (!line.trimmed().isEmpty())
				sequence.append(line.trimmed());
		}
		binding.insert(QStringLiteral("sequence"), sequence);
		binding.remove(QStringLiteral("template"));
	}
	commands.insert(id, binding);
	root_.insert(QStringLiteral("commands"), commands);
	return true;
}

bool CameraProfileDocument::setDiscoveredZoomEndpoint(int maximum, const QString &digitalZoomMode, QString *error)
{
	if (maximum < 0 || maximum > 0xFFFF ||
	    (digitalZoomMode != QStringLiteral("off") && digitalZoomMode != QStringLiteral("on") &&
	     digitalZoomMode != QStringLiteral("unknown"))) {
		setError(error, QStringLiteral("Discovered zoom endpoint or digital zoom mode is invalid"));
		return false;
	}
	QJsonObject capabilities = root_.value(QStringLiteral("capabilities")).toObject();
	QJsonObject limits = capabilities.value(QStringLiteral("limits")).toObject();
	QJsonObject zoom = limits.value(QStringLiteral("zoom")).toObject();
	QJsonObject nativeRange;
	nativeRange.insert(QStringLiteral("minimum"), 0);
	nativeRange.insert(QStringLiteral("maximum"), maximum);
	nativeRange.insert(QStringLiteral("units"), QStringLiteral("VISCA 16-bit position"));
	nativeRange.insert(QStringLiteral("digitalZoomMode"), digitalZoomMode);
	nativeRange.insert(QStringLiteral("measuredAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
	zoom.insert(QStringLiteral("nativeRange"), nativeRange);
	limits.insert(QStringLiteral("zoom"), zoom);
	capabilities.insert(QStringLiteral("limits"), limits);
	root_.insert(QStringLiteral("capabilities"), capabilities);
	return true;
}

bool renderViscaTemplate(const QString &commandTemplate, const ViscaTemplateValues &values, QByteArray *packet, QString *error)
{
	QString expanded = commandTemplate.trimmed();
	if (expanded.isEmpty()) {
		setError(error, QStringLiteral("Command template is empty"));
		return false;
	}
	if (!expandTokens(&expanded, values, error))
		return false;
	const QStringList bytes = expanded.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
	QByteArray output;
	for (const QString &byteText : bytes) {
		if (byteText.size() != 2) {
			setError(error, QStringLiteral("Expected a two-digit byte, found '%1'").arg(byteText));
			return false;
		}
		bool ok = false;
		const uint value = byteText.toUInt(&ok, 16);
		if (!ok || value > 0xFF) {
			setError(error, QStringLiteral("Invalid hexadecimal byte '%1'").arg(byteText));
			return false;
		}
		output.append(static_cast<char>(value));
	}
	if (output.size() < 3 || static_cast<quint8>(output.back()) != 0xFF) {
		setError(error, QStringLiteral("VISCA packet must end in FF and contain at least three bytes"));
		return false;
	}
	if (packet)
		*packet = output;
	return true;
}

bool renderViscaTemplates(const QStringList &templates, const ViscaTemplateValues &values,
			  QVector<QByteArray> *packets, QString *error)
{
	if (templates.isEmpty()) {
		setError(error, QStringLiteral("Command sequence is empty"));
		return false;
	}
	QVector<QByteArray> rendered;
	for (const QString &commandTemplate : templates) {
		QByteArray packet;
		if (!renderViscaTemplate(commandTemplate, values, &packet, error))
			return false;
		rendered.append(packet);
	}
	if (packets)
		*packets = rendered;
	return true;
}

bool viscaCommandWaitsForCompletion(const QString &commandId)
{
	return commandId != QStringLiteral("movePanTilt") && commandId != QStringLiteral("zoomIn") &&
	       commandId != QStringLiteral("zoomOut") && commandId != QStringLiteral("focusNear") &&
	       commandId != QStringLiteral("focusFar") && commandId != QStringLiteral("powerOff");
}

} // namespace simplecam
