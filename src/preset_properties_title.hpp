#pragma once

#include <QString>

namespace simplecam {

// Remove only SimpleCam's generated shared-source wrapper. Preserve the host's
// localized title prefix, numeric suffix and literal characters in source names.
inline QString plainPresetPropertiesTitle(QString title)
{
	const QString opening = QStringLiteral("<span style=\"color:#FF6050;\">");
	const QString closing = QStringLiteral("</span>");
	const qsizetype start = title.indexOf(opening);
	if (start < 0) return title;
	const qsizetype contentStart = start + opening.size();
	const qsizetype end = title.indexOf(closing, contentStart);
	if (end < 0) return title;
	QString name = title.mid(contentStart, end - contentStart);
	if (name.endsWith(QStringLiteral(" *"))) name.chop(2);
	// Reverse QString::toHtmlEscaped exactly once; do ampersands last so an
	// operator's literal "&lt;" text does not become a different character.
	name.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
	name.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
	name.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
	name.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
	title.replace(start, end + closing.size() - start, name);
	return title;
}
} // namespace simplecam
