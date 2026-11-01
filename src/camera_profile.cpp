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
    explicitDisplayName_ = false;
	return true;
}

bool CameraProfileDocument::parseRoot(const QJsonObject &root, QString *error)
{
	if (root.value(QStringLiteral("schemaVersion")).toInt(-1) != 1) {
		setError(error, QStringLiteral("Only camera profile schemaVersion 1 is supported"));
		return false;
	}
    if(root.contains(QStringLiteral("profileVersion"))) {
        CameraProfileDocument versionCheck;
        if(!root.value(QStringLiteral("profileVersion")).isString() || !versionCheck.setProfileVersion(root.value(QStringLiteral("profileVersion")).toString(),error)) return false;
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
	for (const QJsonValue value : connections) {
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
			setError(error, QStringLiteral("Camera Profiler supports VISCA profiles only"));
			return false;
		}
        if (connection.value(QStringLiteral("driver")).toString() == QStringLiteral("sony-visca-udp")) {
            const auto udp = connection.value(QStringLiteral("udp")).toObject();
            const auto response = connection.value(QStringLiteral("response")).toObject();
            auto integer = [](const QJsonValue &v, int low, int high) {
                return v.isDouble() && v.toDouble() == v.toInt(-1) && v.toInt(-1) >= low && v.toInt(-1) <= high;
            };
            if (connection.value(QStringLiteral("transport")).toString() != QStringLiteral("udp") ||
                connection.value(QStringLiteral("framing")).toString() != QStringLiteral("visca-ip-header") ||
                !integer(connection.value(QStringLiteral("defaultPort")),1,65535) ||
                !integer(udp.value(QStringLiteral("localPort")),1,65535) ||
                !integer(udp.value(QStringLiteral("minimumRequestGapMs")),0,2000) ||
                (!udp.value(QStringLiteral("resetSetupMs")).isUndefined() && !integer(udp.value(QStringLiteral("resetSetupMs")),0,2000)) ||
                !integer(response.value(QStringLiteral("timeoutMs")),50,30000)) {
                setError(error, QStringLiteral("Sony UDP connection requires valid framing, port, localPort, minimumRequestGapMs and timeoutMs"));
                return false;
            }
        }
        const auto udpPolicy=connection.value(QStringLiteral("udp")).toObject().value(QStringLiteral("localPortMode"));
        if(!udpPolicy.isUndefined() && udpPolicy.toString()!=QStringLiteral("same-as-camera") && udpPolicy.toString()!=QStringLiteral("fixed")) {
            setError(error,QStringLiteral("Invalid UDP local port policy")); return false;
        }
        if(connection.contains(QStringLiteral("tuning"))) {
            const auto gap=connection.value(QStringLiteral("tuning")).toObject().value(QStringLiteral("minimumRequestGapMs"));
            if(!gap.isDouble() || gap.toDouble()!=gap.toInt(-1) || gap.toInt(-1)<0 || gap.toInt(-1)>2000) {
                setError(error,QStringLiteral("Invalid profile request spacing")); return false;
            }
        }
        const QString driver = connection.value(QStringLiteral("driver")).toString();
        if (driver == QStringLiteral("raw-visca-tcp") || driver == QStringLiteral("raw-visca-udp")) {
            const bool udp = driver == QStringLiteral("raw-visca-udp");
            auto integer = [](const QJsonValue &v, int low, int high) {
                return v.isDouble() && v.toDouble() == v.toInt(-1) && v.toInt(-1)>=low && v.toInt(-1)<=high;
            };
            const auto tuning = connection.value(QStringLiteral("tuning")).toObject();
            if (connection.value(QStringLiteral("transport")).toString() != (udp ? QStringLiteral("udp") : QStringLiteral("tcp")) ||
                connection.value(QStringLiteral("framing")).toString() != QStringLiteral("raw-visca") ||
                !integer(connection.value(QStringLiteral("defaultPort")),1,65535) ||
                !integer(tuning.value(QStringLiteral("minimumRequestGapMs")),0,2000) ||
                !integer(connection.value(QStringLiteral("response")).toObject().value(QStringLiteral("timeoutMs")),50,30000) ||
                (udp && !integer(connection.value(QStringLiteral("udp")).toObject().value(QStringLiteral("localPort")),1,65535))) {
                setError(error,QStringLiteral("Raw VISCA profile needs valid transport/framing, ports, request gap and reply timeout")); return false;
            }
        } else if (!driver.isEmpty() && driver != QStringLiteral("sony-visca-udp")) {
            setError(error,QStringLiteral("Unsupported network driver: %1").arg(driver)); return false;
        }
        const auto addressing = connection.value(QStringLiteral("addressing")).toObject();
        if (addressing.value(QStringLiteral("mode")).toString() == QStringLiteral("profile-fixed")) {
            const auto value = addressing.value(QStringLiteral("default"));
            if (!value.isDouble() || value.toDouble()!=value.toInt() || value.toInt()<1 || value.toInt()>7) {
                setError(error,QStringLiteral("Profile VISCA address must be an integer from 1 to 7")); return false;
            }
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
			for (const QJsonValue step : sequence) {
				if (!step.isString() || step.toString().trimmed().isEmpty()) {
					setError(error, QStringLiteral("Command '%1' sequence steps must be non-empty strings").arg(it.key()));
					return false;
				}
			}
		}
	}
    for (const QJsonValue value : connections) {
        if (value.toObject().value(QStringLiteral("driver")).toString().isEmpty()) continue;
        const auto limits = root.value(QStringLiteral("capabilities")).toObject().value(QStringLiteral("limits")).toObject();
        for (const QString &name : {QStringLiteral("panSpeed"),QStringLiteral("tiltSpeed"),QStringLiteral("zoomSpeed"),QStringLiteral("zoomAbsolute")}) {
            const auto range = limits.value(name).toObject();
            if (range.isEmpty()) continue; // Earlier zoom-only profiles remain readable.
            const int low = range.value(QStringLiteral("minimum")).toInt(-1);
            const int high = range.value(QStringLiteral("maximum")).toInt(-1);
            const int ceiling = name == QStringLiteral("panSpeed") ? 24 : name == QStringLiteral("tiltSpeed") ? 20 :
                name == QStringLiteral("zoomSpeed") ? 15 : 65535;
            if (range.value(QStringLiteral("minimum")).toDouble(-1)!=low || range.value(QStringLiteral("maximum")).toDouble(-1)!=high ||
                low < 0 || high < low || high > ceiling ||
                ((name == QStringLiteral("panSpeed") || name == QStringLiteral("tiltSpeed")) && low < 1)) {
                setError(error,QStringLiteral("Invalid VISCA profile range: %1").arg(name)); return false;
            }
        }
    }
    int sonyConnections = 0;
    bool rawTcp = false;
    for (const QJsonValue value : connections) {
        const auto c = value.toObject();
        if (c.value(QStringLiteral("driver")).toString() == QStringLiteral("sony-visca-udp")) ++sonyConnections;
        if (c.value(QStringLiteral("transport")).toString() == QStringLiteral("tcp") &&
            c.value(QStringLiteral("framing")).toString() == QStringLiteral("raw-visca")) rawTcp = true;
    }
    if (sonyConnections > 1 || (sonyConnections && rawTcp)) {
        setError(error,QStringLiteral("Use one Sony UDP connection in its own profile; mixed TCP/UDP selection is not implemented"));
        return false;
    }
    int namedRaw = 0;
    for (const QJsonValue value : connections) {
        const auto driver = value.toObject().value(QStringLiteral("driver")).toString();
        if (driver == QStringLiteral("raw-visca-tcp") || driver == QStringLiteral("raw-visca-udp")) ++namedRaw;
    }
    if (namedRaw && connections.size()!=1) {
        setError(error,QStringLiteral("Profiler network methods must use separate single-method profiles")); return false;
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
	identity.insert(QStringLiteral("displayName"), explicitDisplayName_ ? this->displayName() : displayName);
	savedRoot.insert(QStringLiteral("identity"), identity);
	QJsonObject support = savedRoot.value(QStringLiteral("support")).toObject();
	support.insert(QStringLiteral("status"), QStringLiteral("draft"));
    support.remove(QStringLiteral("hardwareTest"));
	savedRoot.insert(QStringLiteral("support"), support);
	savedRoot.insert(QStringLiteral("derivedFrom"), id());
    savedRoot.insert(QStringLiteral("profileVersion"),profileVersion());
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

QJsonObject CameraProfileDocument::networkConnection() const
{
    for (const QJsonValue v : root_.value(QStringLiteral("connections")).toArray()) {
        const auto c = v.toObject();
        if (c.value(QStringLiteral("protocol")).toString()!=QStringLiteral("visca")) continue;
        const auto transport=c.value(QStringLiteral("transport")).toString();
        const auto framing=c.value(QStringLiteral("framing")).toString();
        if ((transport==QStringLiteral("tcp") && framing==QStringLiteral("raw-visca")) ||
            (transport==QStringLiteral("udp") && (framing==QStringLiteral("raw-visca") || framing==QStringLiteral("visca-ip-header")))) return c;
    }
    return {};
}
QString CameraProfileDocument::networkMethod() const
{
    const auto c=networkConnection();
    if(c.value(QStringLiteral("transport")).toString()==QStringLiteral("tcp")) return QStringLiteral("raw-visca-tcp");
    if(c.value(QStringLiteral("driver")).toString()==QStringLiteral("sony-visca-udp")) return QStringLiteral("sony-visca-udp");
    if(c.value(QStringLiteral("driver")).toString()==QStringLiteral("raw-visca-udp")) return QStringLiteral("raw-visca-udp");
    return {};
}
bool CameraProfileDocument::supportsRawViscaUdp() const { return networkMethod()==QStringLiteral("raw-visca-udp"); }
bool CameraProfileDocument::profileOwnsConnection() const { return !networkConnection().value(QStringLiteral("driver")).toString().isEmpty(); }

bool CameraProfileDocument::setProfilerSettings(const QString &name, const QString &method, int port, int localPort,
    int gapMs, int resetMs, int timeoutMs, int address, QString *error)
{
    if(name.trimmed().isEmpty() || name.trimmed().size()>160 ||
        (method!=QStringLiteral("raw-visca-tcp") && method!=QStringLiteral("raw-visca-udp") && method!=QStringLiteral("sony-visca-udp")) ||
        port<1 || port>65535 || localPort<1 || localPort>65535 || gapMs<0 || gapMs>2000 ||
        resetMs<0 || resetMs>2000 || timeoutMs<50 || timeoutMs>30000 || address<1 || address>7) {
        setError(error,QStringLiteral("Enter a profile name, supported method and valid port/timing/address values")); return false;
    }
    auto candidate=root_;
    auto identity=candidate.value(QStringLiteral("identity")).toObject();
    identity.insert(QStringLiteral("displayName"),name.trimmed()); candidate.insert(QStringLiteral("identity"),identity);
    const bool sony=method==QStringLiteral("sony-visca-udp"), udp=method!=QStringLiteral("raw-visca-tcp");
    auto c=networkConnection();
    c.insert(QStringLiteral("id"),method); c.insert(QStringLiteral("driver"),method);
    c.insert(QStringLiteral("label"),sony ? QStringLiteral("VISCA / UDP — Sony framing") : udp ? QStringLiteral("VISCA / UDP — Raw") : QStringLiteral("VISCA / TCP — Raw"));
    c.insert(QStringLiteral("protocol"),QStringLiteral("visca"));
    c.insert(QStringLiteral("transport"),udp ? QStringLiteral("udp") : QStringLiteral("tcp"));
    c.insert(QStringLiteral("framing"),sony ? QStringLiteral("visca-ip-header") : QStringLiteral("raw-visca"));
    c.insert(QStringLiteral("defaultPort"),port);
    c.insert(QStringLiteral("addressing"),QJsonObject{{QStringLiteral("mode"),QStringLiteral("profile-fixed")},{QStringLiteral("default"),sony ? 1 : address}});
    c.insert(QStringLiteral("tuning"),QJsonObject{{QStringLiteral("minimumRequestGapMs"),gapMs}});
    if(udp) c.insert(QStringLiteral("udp"),QJsonObject{{QStringLiteral("localPort"),localPort},{QStringLiteral("minimumRequestGapMs"),gapMs},{QStringLiteral("resetSetupMs"),resetMs}});
    else c.remove(QStringLiteral("udp"));
    auto response=c.value(QStringLiteral("response")).toObject(); response.insert(QStringLiteral("timeoutMs"),timeoutMs);
    c.insert(QStringLiteral("response"),response);
    candidate.insert(QStringLiteral("connections"),QJsonArray{c});
    if(!parseRoot(candidate,error)) return false;
    explicitDisplayName_=true;
    return true;
}

QString CameraProfileDocument::profileVersion() const
{
    return root_.value(QStringLiteral("profileVersion")).toString(QStringLiteral("1.0.0"));
}
bool CameraProfileDocument::setProfileVersion(const QString &version,QString *error)
{
    static const QRegularExpression pattern(QStringLiteral(R"(^[0-9]+(?:\.[0-9]+){0,2}(?:-[A-Za-z0-9.-]+)?$)"));
    if(version.size()>32 || !pattern.match(version.trimmed()).hasMatch()) {
        setError(error,QStringLiteral("Use a profile version such as 1.0.0 or 1.0.1-draft")); return false;
    }
    root_.insert(QStringLiteral("profileVersion"),version.trimmed()); return true;
}
bool CameraProfileDocument::setLocalPortPolicy(bool sameAsCamera,QString *error)
{
    if(networkMethod()==QStringLiteral("raw-visca-tcp")) return true;
    auto candidate=root_;
    auto connections=candidate.value(QStringLiteral("connections")).toArray();
    if(connections.size()!=1) { setError(error,QStringLiteral("Local-port policy requires one network method")); return false; }
    auto c=connections[0].toObject(); auto udp=c.value(QStringLiteral("udp")).toObject();
    udp.insert(QStringLiteral("localPortMode"),sameAsCamera ? QStringLiteral("same-as-camera") : QStringLiteral("fixed"));
    if(sameAsCamera) udp.insert(QStringLiteral("localPort"),c.value(QStringLiteral("defaultPort")));
    c.insert(QStringLiteral("udp"),udp); connections[0]=c; candidate.insert(QStringLiteral("connections"),connections);
    return parseRoot(candidate,error);
}

bool CameraProfileDocument::setIdentity(const QString &manufacturer,const QString &model,QString *error)
{
    if(manufacturer.trimmed().isEmpty() || model.trimmed().isEmpty() || manufacturer.size()>120 || model.size()>120) {
        setError(error,QStringLiteral("Enter manufacturer and model")); return false;
    }
    auto identity=root_.value(QStringLiteral("identity")).toObject();
    identity.insert(QStringLiteral("manufacturer"),manufacturer.trimmed()); identity.insert(QStringLiteral("model"),model.trimmed());
    root_.insert(QStringLiteral("identity"),identity); return true;
}
bool CameraProfileDocument::newDraft(QString *error)
{
    auto candidate=root_;
    const QString parent=id();
    auto identity=candidate.value(QStringLiteral("identity")).toObject();
    identity.insert(QStringLiteral("id"),QStringLiteral("user.unsaved-draft"));
    identity.insert(QStringLiteral("displayName"),QStringLiteral("New camera profile"));
    identity.insert(QStringLiteral("manufacturer"),QStringLiteral("Custom"));
    identity.insert(QStringLiteral("model"),QStringLiteral("Unspecified"));
    identity.remove(QStringLiteral("aliases")); identity.remove(QStringLiteral("hardwareRevisions"));
    identity.insert(QStringLiteral("firmware"),QJsonObject{});
    candidate.insert(QStringLiteral("identity"),identity);
    auto support=candidate.value(QStringLiteral("support")).toObject(); support.insert(QStringLiteral("status"),QStringLiteral("draft"));
    support.remove(QStringLiteral("hardwareTest"));
    candidate.insert(QStringLiteral("support"),support); candidate.insert(QStringLiteral("derivedFrom"),parent);
    // Existing command/range definitions are a starting template, not evidence for the new model.
    candidate.remove(QStringLiteral("measurements"));
    auto capabilities=candidate.value(QStringLiteral("capabilities")).toObject();
    auto limits=capabilities.value(QStringLiteral("limits")).toObject();
    auto zoom=limits.value(QStringLiteral("zoom")).toObject(); zoom.remove(QStringLiteral("nativeRange"));
    if(!zoom.isEmpty()) limits.insert(QStringLiteral("zoom"),zoom);
    capabilities.insert(QStringLiteral("limits"),limits);
    auto operations=capabilities.value(QStringLiteral("operations")).toObject();
    for(auto it=operations.begin();it!=operations.end();++it) {
        auto operation=it.value().toObject(); operation.insert(QStringLiteral("reason"),QStringLiteral("Draft copied binding; verify on the target model")); it.value()=operation;
    }
    capabilities.insert(QStringLiteral("operations"),operations);
    candidate.insert(QStringLiteral("capabilities"),capabilities);
    candidate.insert(QStringLiteral("profileVersion"),QStringLiteral("1.0.0"));
    if(!parseRoot(candidate,error)) return false;
    path_.clear(); explicitDisplayName_=true;
    return true;
}

bool CameraProfileDocument::setSonyUdpSettings(int port, int localPort, int gapMs, int resetMs, int timeoutMs, QString *error)
{
    if (!supportsSonyViscaUdp()) {
        setError(error, QStringLiteral("This profile does not use the Sony UDP driver"));
        return false;
    }
    auto candidate = root_;
    auto connections = candidate.value(QStringLiteral("connections")).toArray();
    for (int i = 0; i < connections.size(); ++i) {
        auto connection = connections[i].toObject();
        if (connection.value(QStringLiteral("driver")).toString() != QStringLiteral("sony-visca-udp")) continue;
        connection.insert(QStringLiteral("defaultPort"), port);
        auto udp = connection.value(QStringLiteral("udp")).toObject();
        udp.insert(QStringLiteral("localPort"), localPort);
        udp.insert(QStringLiteral("minimumRequestGapMs"), gapMs);
        udp.insert(QStringLiteral("resetSetupMs"), resetMs);
        connection.insert(QStringLiteral("udp"), udp);
        connection.insert(QStringLiteral("tuning"), QJsonObject{{QStringLiteral("minimumRequestGapMs"),gapMs}});
        auto response = connection.value(QStringLiteral("response")).toObject();
        response.insert(QStringLiteral("timeoutMs"), timeoutMs);
        connection.insert(QStringLiteral("response"), response);
        connections[i] = connection;
    }
    candidate.insert(QStringLiteral("connections"), connections);
    return parseRoot(candidate, error);
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

QJsonObject CameraProfileDocument::sonyUdpConnection() const
{
    for (const QJsonValue value : root_.value(QStringLiteral("connections")).toArray()) {
        const auto c = value.toObject();
        if (c.value(QStringLiteral("protocol")).toString() == QStringLiteral("visca") &&
            c.value(QStringLiteral("transport")).toString() == QStringLiteral("udp") &&
            c.value(QStringLiteral("framing")).toString() == QStringLiteral("visca-ip-header") &&
            c.value(QStringLiteral("driver")).toString() == QStringLiteral("sony-visca-udp"))
            return c;
    }
    return {};
}
bool CameraProfileDocument::supportsSonyViscaUdp() const { return !sonyUdpConnection().isEmpty(); }

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

