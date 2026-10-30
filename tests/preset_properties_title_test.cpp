#include "preset_properties_title.hpp"
#include <iostream>

int main()
{
	int checks = 0, failures = 0;
	auto check = [&](const QString &input, const QString &expected, const char *label) {
		++checks;
		if (simplecam::plainPresetPropertiesTitle(input) != expected) {
			++failures;
			std::cerr << "FAIL: " << label << '\n';
		}
	};
	const QString open = QStringLiteral("<span style=\"color:#FF6050;\">");
	const QString close = QStringLiteral("</span>");
	check(QStringLiteral("Properties for 'Move Camera 1 to Preset 1 (2)'"),
		QStringLiteral("Properties for 'Move Camera 1 to Preset 1 (2)'"), "Plain title unchanged");
	check(QStringLiteral("Properties for '") + open + QStringLiteral("Move Camera 1 to Preset 1 (2) *") + close + QLatin1Char('\''),
		QStringLiteral("Properties for 'Move Camera 1 to Preset 1 (2)'"), "Remove generated wrapper and marker");
	check(open + QStringLiteral("Move A &amp; B to &lt;Stage&gt; &quot;Left&quot; *") + close,
		QStringLiteral("Move A & B to <Stage> \"Left\""), "Decode escaped names once");
	check(open + QStringLiteral("Literal &amp;lt; *") + close,
		QStringLiteral("Literal &lt;"), "Preserve literal entity text");
	check(QStringLiteral("Propriétés : ") + open + QStringLiteral("Camera *") + close + QStringLiteral(" (3)"),
		QStringLiteral("Propriétés : Camera (3)"), "Preserve localized prefix and external OBS suffix");
	check(open + QStringLiteral("Camera without closing tag"), open + QStringLiteral("Camera without closing tag"), "Malformed wrapper untouched");
	check(QStringLiteral("<span style=\"color:red;\">Other *</span>"),
		QStringLiteral("<span style=\"color:red;\">Other *</span>"), "Unrelated markup untouched");
	check(open + QStringLiteral("Original * *") + close,
		QStringLiteral("Original *"), "Only remove generated trailing marker");
	std::cout << checks << " checks, " << failures << " failures\n";
	return failures ? 1 : 0;
}
