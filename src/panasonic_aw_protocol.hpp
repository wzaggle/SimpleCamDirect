#pragma once
#include "camera_profile.hpp"
#include <QUrl>
namespace simplecam {
// Profile templates are command strings, never URLs or executable expressions.
bool renderAwTemplates(const QStringList &, const ViscaTemplateValues &, QVector<QByteArray> *, QString *);
bool awBindingMatches(const QString &id, const QVector<QByteArray> &commands);
bool validateAwCommand(const QByteArray &, QString *error = nullptr);
bool validateAwReply(const QByteArray &command, const QByteArray &reply, QString *error = nullptr);
bool decodeAwPower(const QByteArray &, bool *on);
bool decodeAwZoom(const QByteArray &, int *position);
QByteArray awStopFor(const QByteArray &command);
bool isAwStop(const QByteArray &command);
QUrl awRequestUrl(const QString &host, quint16 port, const QByteArray &command);

}
