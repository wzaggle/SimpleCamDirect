#include "profile_test_tool.hpp"

#include "camera_runtime.hpp"

#include <QApplication>
#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QTcpSocket>
#include <QTimer>

#include <algorithm>
#include <memory>

namespace simplecam {
namespace {

QString hexString(const QByteArray &bytes)
{
	return QString::fromLatin1(bytes.toHex(' ').toUpper());
}

QStringList splitTemplateLine(const QString &text)
{
	QStringList templates = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);
	for (QString &command : templates)
		command = command.trimmed();
	templates.removeAll(QString());
	return templates;
}

bool decodePowerState(const QByteArray &reply, bool *cameraOn)
{
	qsizetype offset = 0;
	while (offset < reply.size()) {
		const qsizetype end = reply.indexOf(static_cast<char>(0xFF), offset);
		if (end < 0)
			break;
		const QByteArray frame = reply.mid(offset, end - offset + 1);
		offset = end + 1;
		if (frame.size() < 4 || (static_cast<quint8>(frame.at(1)) & 0xF0) != 0x50)
			continue;
		const quint8 state = static_cast<quint8>(frame.at(2));
		if (state == 0x02) {
			*cameraOn = true;
			return true;
		}
		if (state == 0x03) {
			*cameraOn = false;
			return true;
		}
	}
	return false;
}

QString prettyName(QString id)
{
	id.replace(QRegularExpression(QStringLiteral("([a-z])([A-Z])")), QStringLiteral("\\1 \\2"));
	id.replace(QLatin1Char('_'), QLatin1Char(' '));
	if (!id.isEmpty())
		id[0] = id.at(0).toUpper();
	return id;
}

bool commandIsInquiry(const QByteArray &packet)
{
	return packet.size() > 1 && static_cast<quint8>(packet.at(1)) == 0x09;
}

QString describeReplies(const QVector<QByteArray> &replies)
{
	QStringList descriptions;
	for (int step = 0; step < replies.size(); ++step) {
		const QByteArray &bytes = replies.at(step);
		qsizetype offset = 0;
		while (offset < bytes.size()) {
			const qsizetype end = bytes.indexOf(static_cast<char>(0xFF), offset);
			if (end < 0)
				break;
			const QByteArray frame = bytes.mid(offset, end - offset + 1);
			QString kind = QStringLiteral("reply");
			if (frame.size() > 1) {
				const quint8 response = static_cast<quint8>(frame.at(1)) & 0xF0;
				if (response == 0x40)
					kind = QStringLiteral("ACK");
				else if (response == 0x50)
					kind = QStringLiteral("completion / inquiry data");
				else if (response == 0x60)
					kind = QStringLiteral("VISCA error");
			}
			descriptions.append(QStringLiteral("step %1 %2: %3").arg(step + 1).arg(kind, hexString(frame)));
			offset = end + 1;
		}
	}
	return descriptions.isEmpty() ? QStringLiteral("No response frame recorded") : descriptions.join(QStringLiteral("; "));
}

} // namespace

CameraProfileTestDialog::CameraProfileTestDialog(const QString &defaultProfilePath, QWidget *parent)
	: QDialog(parent), defaultProfilePath_(defaultProfilePath), profile_(new CameraProfileDocument)
{
	setWindowTitle(QStringLiteral("SimpleCamPTZ - Camera Profile Test"));
	setMinimumSize(980, 680);
	resize(1180, 860);
	buildUi();
	if (!defaultProfilePath_.isEmpty()) {
		profilePath_->setText(defaultProfilePath_);
		loadProfile(defaultProfilePath_);
	}
}

CameraProfileTestDialog::~CameraProfileTestDialog()
{
	delete profile_;
}

bool CameraProfileTestDialog::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::FocusIn) {
		if (qobject_cast<QLineEdit *>(watched) || qobject_cast<QPlainTextEdit *>(watched))
			activeEditor_ = qobject_cast<QWidget *>(watched);
	}
	return QDialog::eventFilter(watched, event);
}

void CameraProfileTestDialog::closeEvent(QCloseEvent *event)
{
	if (ioBusy_ && !zoomDiscoveryActive_) {
		event->ignore();
		setStatus(QStringLiteral("Wait for the camera test to finish before closing."));
		return;
	}
	if (zoomDiscoveryActive_) {
		event->ignore();
		if (!zoomDiscoveryFinishing_) {
			zoomDiscoveryCancelRequested_ = true;
			discoverZoomButton_->setText(QStringLiteral("Canceling before close…"));
			setStatus(QStringLiteral("Stopping zoom and restoring the starting position before closing."));
		}
		return;
	}
	QDialog::closeEvent(event);
}

void CameraProfileTestDialog::buildUi()
{
	auto *root = new QVBoxLayout(this);

	auto *profileBox = new QGroupBox(QStringLiteral("Camera Profile"), this);
	profileControls_ = profileBox;
	auto *profileLayout = new QGridLayout(profileBox);
	profilePath_ = new QLineEdit(profileBox);
	profilePath_->setReadOnly(true);
	auto *browse = new QPushButton(QStringLiteral("Open JSON…"), profileBox);
	auto *reload = new QPushButton(QStringLiteral("Reload"), profileBox);
	auto *saveAsButton = new QPushButton(QStringLiteral("Save As…"), profileBox);
	status_ = new QLabel(QStringLiteral("Load a VISCA profile to begin."), profileBox);
	status_->setWordWrap(true);
	profileLayout->addWidget(new QLabel(QStringLiteral("File"), profileBox), 0, 0);
	profileLayout->addWidget(profilePath_, 0, 1);
	profileLayout->addWidget(browse, 0, 2);
	profileLayout->addWidget(reload, 0, 3);
	profileLayout->addWidget(saveAsButton, 0, 4);
	profileLayout->addWidget(status_, 1, 0, 1, 5);
	profileLayout->setColumnStretch(1, 1);
	root->addWidget(profileBox);

	auto *connectionBox = new QGroupBox(QStringLiteral("Test Connection"), this);
	connectionControls_ = connectionBox;
	auto *connectionLayout = new QGridLayout(connectionBox);
	cameraSlot_ = new QComboBox(connectionBox);
	for (int index = 0; index < CameraSlotCount; ++index)
		cameraSlot_->addItem(QStringLiteral("Camera %1").arg(index + 1), index);
	connectionVariant_ = new QComboBox(connectionBox);
	host_ = new QLineEdit(connectionBox);
	port_ = new QSpinBox(connectionBox);
	port_->setRange(1, 65535);
	port_->setValue(1259);
	address_ = new QSpinBox(connectionBox);
	address_->setRange(1, 7);
	address_->setValue(1);
	testConnectionButton_ = new QPushButton(QStringLiteral("Test TCP"), connectionBox);
	auto *applySlot = new QPushButton(QStringLiteral("Apply to Slot"), connectionBox);
	connectionLayout->addWidget(new QLabel(QStringLiteral("Camera slot"), connectionBox), 0, 0);
	connectionLayout->addWidget(cameraSlot_, 0, 1);
	connectionLayout->addWidget(new QLabel(QStringLiteral("Profile transport"), connectionBox), 0, 2);
	connectionLayout->addWidget(connectionVariant_, 0, 3);
	connectionLayout->addWidget(new QLabel(QStringLiteral("IP address"), connectionBox), 1, 0);
	connectionLayout->addWidget(host_, 1, 1);
	connectionLayout->addWidget(new QLabel(QStringLiteral("Port"), connectionBox), 1, 2);
	connectionLayout->addWidget(port_, 1, 3);
	connectionLayout->addWidget(new QLabel(QStringLiteral("VISCA address"), connectionBox), 1, 4);
	connectionLayout->addWidget(address_, 1, 5);
	connectionLayout->addWidget(testConnectionButton_, 1, 6);
	connectionLayout->addWidget(applySlot, 1, 7);
	connectionLayout->setColumnStretch(1, 1);
	auto *connectionNote = new QLabel(QStringLiteral("Tests use a separate TCP connection. Avoid moving this camera from another SimpleCam panel or its web page during a test."), connectionBox);
	connectionNote->setWordWrap(true);
	connectionLayout->addWidget(connectionNote, 2, 0, 1, 8);
	root->addWidget(connectionBox);

	connect(cameraSlot_, qOverload<int>(&QComboBox::currentIndexChanged), this,
		 [this](int index) { selectSlot(index); });
	connect(testConnectionButton_, &QPushButton::clicked, this, [this]() { testConnection(); });
	connect(applySlot, &QPushButton::clicked, this, [this]() {
		CameraRuntime *runtime = cameraRuntime(cameraSlot_->currentData().toInt());
		if (!runtime) {
			setStatus(QStringLiteral("Selected camera slot is unavailable."), true);
			return;
		}
		if (QMessageBox::question(this, QStringLiteral("Apply Camera Connection"),
					  QStringLiteral("Save these connection values to %1's existing slot settings?")
						  .arg(runtime->cameraName()),
					  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
			return;
		runtime->setHost(host_->text().trimmed());
		runtime->setPort(static_cast<quint16>(port_->value()));
		runtime->setCameraAddress(address_->value());
		setStatus(QStringLiteral("Connection settings applied to %1.").arg(runtime->cameraName()));
	});
	selectSlot(0);

	auto *valueBox = new QGroupBox(QStringLiteral("Token Values for Render / Test"), this);
	valueControls_ = valueBox;
	auto *valueLayout = new QGridLayout(valueBox);
	auto makeSpin = [valueBox](int min, int max, int value) {
		auto *spin = new QSpinBox(valueBox);
		spin->setRange(min, max);
		spin->setValue(value);
		return spin;
	};
	panSpeed_ = makeSpin(1, 24, 8);
	tiltSpeed_ = makeSpin(1, 20, 8);
	panDirection_ = makeSpin(1, 3, 1);
	tiltDirection_ = makeSpin(1, 3, 3);
	zoomSpeed_ = makeSpin(0, 15, 8);
	zoomPosition_ = makeSpin(0, 0xFFFF, 0);
	presetNumber_ = makeSpin(1, 255, 1);
	valueLayout->addWidget(new QLabel(QStringLiteral("{PS} Pan speed"), valueBox), 0, 0);
	valueLayout->addWidget(panSpeed_, 0, 1);
	valueLayout->addWidget(new QLabel(QStringLiteral("{TS} Tilt speed"), valueBox), 0, 2);
	valueLayout->addWidget(tiltSpeed_, 0, 3);
	valueLayout->addWidget(new QLabel(QStringLiteral("{PD} Pan direction"), valueBox), 0, 4);
	valueLayout->addWidget(panDirection_, 0, 5);
	valueLayout->addWidget(new QLabel(QStringLiteral("{TD} Tilt direction"), valueBox), 0, 6);
	valueLayout->addWidget(tiltDirection_, 0, 7);
	valueLayout->addWidget(new QLabel(QStringLiteral("{ZS} Zoom speed"), valueBox), 1, 0);
	valueLayout->addWidget(zoomSpeed_, 1, 1);
	valueLayout->addWidget(new QLabel(QStringLiteral("{ZP} Zoom position"), valueBox), 1, 2);
	valueLayout->addWidget(zoomPosition_, 1, 3);
	valueLayout->addWidget(new QLabel(QStringLiteral("{PI} Preset number (1-based)"), valueBox), 1, 4);
	valueLayout->addWidget(presetNumber_, 1, 5);
	digitalZoomMode_ = new QComboBox(valueBox);
	digitalZoomMode_->addItem(QStringLiteral("Not recorded"), QStringLiteral("unknown"));
	digitalZoomMode_->addItem(QStringLiteral("DZoom off · optical only"), QStringLiteral("off"));
	digitalZoomMode_->addItem(QStringLiteral("DZoom on · optical + digital"), QStringLiteral("on"));
	valueLayout->addWidget(new QLabel(QStringLiteral("Camera DZoom mode (set on camera)"), valueBox), 2, 0, 1, 2);
	valueLayout->addWidget(digitalZoomMode_, 2, 2, 1, 3);
	discoverZoomButton_ = new QPushButton(QStringLiteral("Discover Zoom Range"), valueBox);
	valueLayout->addWidget(discoverZoomButton_, 2, 6, 1, 2);
	connect(discoverZoomButton_, &QPushButton::clicked, this, [this]() { startZoomDiscovery(); });
	root->addWidget(valueBox);

	auto *workArea = new QSplitter(Qt::Horizontal, this);
	auto *commandPanel = new QWidget(workArea);
	auto *commandLayout = new QVBoxLayout(commandPanel);
	commandLayout->setContentsMargins(0, 0, 0, 0);
	commands_ = new QTableWidget(commandPanel);
	commands_->setColumnCount(5);
	commands_->setHorizontalHeaderLabels({QStringLiteral("Action"), QStringLiteral("Profile command string(s)"),
							       QStringLiteral("Render"), QStringLiteral("Send"), QStringLiteral("Result")});
	commands_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	commands_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	commands_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	commands_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	commands_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
	commands_->verticalHeader()->setVisible(false);
	commands_->setSelectionBehavior(QAbstractItemView::SelectItems);
	commands_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	auto *testAllRow = new QHBoxLayout;
	testAllButton_ = new QPushButton(QStringLiteral("Test All"), commandPanel);
	testAllButton_->setEnabled(false);
	testAllButton_->setToolTip(QStringLiteral("Send every command in the loaded profile in order and record each camera reply."));
	auto *testAllNote = new QLabel(QStringLiteral("Sends every listed command. Camera movement and preset/state changes may occur."), commandPanel);
	testAllNote->setWordWrap(true);
	testAllRow->addWidget(testAllButton_);
	testAllRow->addWidget(testAllNote, 1);
	commandLayout->addLayout(testAllRow);
	commandLayout->addWidget(commands_, 1);
	connect(testAllButton_, &QPushButton::clicked, this, [this]() { startTestAll(); });

	auto *tokenBox = new QGroupBox(QStringLiteral("Insert Token"), workArea);
	tokenControls_ = tokenBox;
	auto *tokenLayout = new QVBoxLayout(tokenBox);
	tokenLayout->setContentsMargins(6, 10, 6, 6);
	const QVector<QPair<QString, QString>> tokens = {
		{QStringLiteral("Address"), QStringLiteral("{A}")},
		{QStringLiteral("Pan Speed"), QStringLiteral("{PS}")},
		{QStringLiteral("Tilt Speed"), QStringLiteral("{TS}")},
		{QStringLiteral("Pan Direction"), QStringLiteral("{PD}")},
		{QStringLiteral("Tilt Direction"), QStringLiteral("{TD}")},
		{QStringLiteral("Zoom Speed"), QStringLiteral("{ZS}")},
		{QStringLiteral("Zoom Position"), QStringLiteral("{ZP}")},
		{QStringLiteral("Preset Index"), QStringLiteral("{PI}")},
	};
	for (const auto &token : tokens) {
		auto *button = new QPushButton(token.first, tokenBox);
		button->setToolTip(QStringLiteral("Insert %1").arg(token.second));
		connect(button, &QPushButton::clicked, this, [this, token]() { insertToken(token.second); });
		tokenLayout->addWidget(button);
	}
	tokenLayout->addStretch(1);
	tokenBox->setMinimumWidth(130);
	tokenBox->setMaximumWidth(175);

	auto *outputStack = new QSplitter(Qt::Vertical, workArea);
	auto *previewGroup = new QGroupBox(QStringLiteral("Rendered VISCA Bytes"), outputStack);
	preview_ = new QPlainTextEdit(previewGroup);
	preview_->setReadOnly(true);
	preview_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
	auto *previewLayout = new QVBoxLayout(previewGroup);
	previewLayout->addWidget(preview_);
	auto *traceGroup = new QGroupBox(QStringLiteral("Send / Reply Trace"), outputStack);
	trace_ = new QPlainTextEdit(traceGroup);
	trace_->setReadOnly(true);
	trace_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
	auto *traceLayout = new QVBoxLayout(traceGroup);
	traceLayout->addWidget(trace_);
	outputStack->addWidget(previewGroup);
	outputStack->addWidget(traceGroup);
	outputStack->setStretchFactor(0, 1);
	outputStack->setStretchFactor(1, 2);
	outputStack->setMinimumWidth(245);
	outputStack->setMinimumHeight(180);
	outputStack->setSizes({210, 360});

	workArea->addWidget(commandPanel);
	workArea->addWidget(tokenBox);
	workArea->addWidget(outputStack);
	workArea->setStretchFactor(0, 6);
	workArea->setStretchFactor(1, 0);
	workArea->setStretchFactor(2, 3);
	workArea->setSizes({690, 150, 300});
	root->addWidget(workArea, 1);

	connect(browse, &QPushButton::clicked, this, [this]() {
		const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Camera Profile"),
								 profilePath_->text(), QStringLiteral("Camera profiles (*.json *.camera.cfg);;All files (*)"));
		if (!path.isEmpty())
			loadProfile(path);
	});
	connect(reload, &QPushButton::clicked, this, [this]() { loadProfile(profilePath_->text()); });
	connect(saveAsButton, &QPushButton::clicked, this, [this]() { saveAs(); });
}

void CameraProfileTestDialog::loadProfile(const QString &path)
{
	if (ioBusy_) {
		setStatus(QStringLiteral("Wait for the current camera operation to finish before loading another profile."), true);
		return;
	}
	QString error;
	if (!profile_->load(path, &error)) {
		setStatus(error, true);
		return;
	}
	profilePath_->setText(path);
	connectionVariant_->clear();
	auto *model = new QStandardItemModel(connectionVariant_);
	for (const QString &id : profile_->connectionIds()) {
		const QString transport = profile_->connectionTransport(id);
		const QString framing = profile_->connectionFraming(id);
		const bool available = transport == QStringLiteral("tcp") && framing == QStringLiteral("raw-visca");
		auto *item = new QStandardItem(profile_->connectionLabel(id));
		item->setData(id, Qt::UserRole);
		if (!available) {
			item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
			item->setToolTip(QStringLiteral("This transport is not implemented in the first test-tool pass."));
		}
		model->appendRow(item);
	}
	auto *udpFuture = new QStandardItem(QStringLiteral("UDP · VISCA over IP (future)"));
	udpFuture->setFlags(udpFuture->flags() & ~Qt::ItemIsEnabled);
	udpFuture->setToolTip(QStringLiteral("UDP transport and VISCA-over-IP framing are not implemented yet."));
	model->appendRow(udpFuture);
	connectionVariant_->setModel(model);
	for (int row = 0; row < model->rowCount(); ++row) {
		const QString id = model->index(row, 0).data(Qt::UserRole).toString();
		if (!id.isEmpty() && profile_->connectionTransport(id) == QStringLiteral("tcp") &&
		    profile_->connectionFraming(id) == QStringLiteral("raw-visca")) {
			connectionVariant_->setCurrentIndex(row);
			break;
		}
	}
	populateCommands();
	setStatus(QStringLiteral("Loaded %1 (%2). Basic profile structure is valid for this test harness.")
			  .arg(profile_->displayName(), profile_->id()));
}

void CameraProfileTestDialog::populateCommands()
{
	commands_->setRowCount(0);
	testAllButton_->setEnabled(false);
	if (!profile_)
		return;
	const QStringList ids = profile_->commandIdsInTestOrder();
	testAllButton_->setEnabled(!ids.isEmpty());
	commands_->setRowCount(ids.size());
	for (int row = 0; row < ids.size(); ++row) {
		const CameraProfileCommand command = profile_->command(ids.at(row));
		auto *name = new QTableWidgetItem(prettyName(command.id));
		name->setData(Qt::UserRole, command.id);
		commands_->setItem(row, 0, name);
		name->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
		auto *editor = new QLineEdit(commands_);
		editor->setText(command.templates.join(QStringLiteral(" ; ")));
		editor->setMaxLength(1024);
		editor->setFixedHeight(24);
		editor->setToolTip(QStringLiteral("For a multi-packet sequence, separate VISCA strings with semicolons."));
		editor->installEventFilter(this);
		connect(editor, &QLineEdit::textChanged, this, [this, row]() {
			if (row >= 0 && row < commands_->rowCount()) {
				commands_->item(row, 0)->setData(Qt::UserRole + 1, true);
				setCommandResult(commands_->item(row, 0)->data(Qt::UserRole).toString(), QStringLiteral("—"),
						 QStringLiteral("Command text changed; run it again to refresh this result."));
			}
		});
		commands_->setCellWidget(row, 1, editor);
		auto *render = new QPushButton(QStringLiteral("Render"), commands_);
		auto *send = new QPushButton(QStringLiteral("Send"), commands_);
		render->setFixedHeight(23);
		send->setFixedHeight(23);
		auto *result = new QTableWidgetItem(QStringLiteral("—"));
		result->setTextAlignment(Qt::AlignCenter);
		result->setFlags(Qt::ItemIsEnabled);
		commands_->setItem(row, 4, result);
		connect(render, &QPushButton::clicked, this, [this, row]() {
			commands_->setCurrentCell(row, 0);
			renderCurrentCommand();
		});
		connect(send, &QPushButton::clicked, this, [this, row]() {
			commands_->setCurrentCell(row, 0);
			sendCurrentCommand();
		});
		commands_->setCellWidget(row, 2, render);
		commands_->setCellWidget(row, 3, send);
		commands_->setRowHeight(row, 30);
	}
	if (!ids.isEmpty())
		commands_->setCurrentCell(0, 0);
}

void CameraProfileTestDialog::selectSlot(int index)
{
	Q_UNUSED(index);
	CameraRuntime *runtime = cameraRuntime(cameraSlot_->currentData().toInt());
	if (!runtime)
		return;
	host_->setText(runtime->host());
	port_->setValue(runtime->port() ? runtime->port() : 1259);
	address_->setValue(runtime->cameraAddress());
}

void CameraProfileTestDialog::setStatus(const QString &message, bool error)
{
	status_->setText(message);
	status_->setStyleSheet(error ? QStringLiteral("color:#c43b3b") : QString());
}

ViscaTemplateValues CameraProfileTestDialog::templateValues() const
{
	ViscaTemplateValues values;
	values.cameraAddress = address_->value();
	values.panSpeed = panSpeed_->value();
	values.tiltSpeed = tiltSpeed_->value();
	values.panDirection = panDirection_->value();
	values.tiltDirection = tiltDirection_->value();
	values.zoomSpeed = zoomSpeed_->value();
	values.zoomPosition = zoomPosition_->value();
	values.physicalPreset = presetNumber_->value();
	return values;
}

QStringList CameraProfileTestDialog::selectedTemplates() const
{
	const int row = commands_->currentRow();
	if (row < 0)
		return {};
	const auto *name = commands_->item(row, 0);
	QWidget *editorWidget = commands_->cellWidget(row, 1);
	if (!name || !editorWidget)
		return {};
	if (auto *lineEditor = qobject_cast<QLineEdit *>(editorWidget))
		return splitTemplateLine(lineEditor->text());
	return {};
}

QVector<QByteArray> CameraProfileTestDialog::renderSelected(QString *error) const
{
	QVector<QByteArray> packets;
	if (!renderViscaTemplates(selectedTemplates(), templateValues(), &packets, error))
		return {};
	return packets;
}

void CameraProfileTestDialog::renderCurrentCommand()
{
	QString error;
	const QVector<QByteArray> packets = renderSelected(&error);
	if (packets.isEmpty()) {
		preview_->setPlainText(error);
		setStatus(error, true);
		return;
	}
	QStringList lines;
	for (int i = 0; i < packets.size(); ++i)
		lines.append(QStringLiteral("Step %1: %2").arg(i + 1).arg(hexString(packets.at(i))));
	preview_->setPlainText(lines.join(QLatin1Char('\n')));
	setStatus(QStringLiteral("Rendered %1 VISCA packet(s). Review the bytes, then use Send.").arg(packets.size()));
}

void CameraProfileTestDialog::sendCurrentCommand()
{
	if (ioBusy_) {
		setStatus(QStringLiteral("A camera test is already in progress."), true);
		return;
	}
	if (!profile_) {
		setStatus(QStringLiteral("Load a profile first."), true);
		return;
	}
	const QString variant = connectionVariant_->currentData(Qt::UserRole).toString();
	if (profile_->connectionTransport(variant) != QStringLiteral("tcp") ||
	    profile_->connectionFraming(variant) != QStringLiteral("raw-visca")) {
		setStatus(QStringLiteral("Only raw VISCA over TCP is implemented in this test-tool pass."), true);
		return;
	}
	const QStringList templates = selectedTemplates();
	QVector<QByteArray> packets;
	QString error;
	if (!renderViscaTemplates(templates, templateValues(), &packets, &error)) {
		setStatus(error, true);
		return;
	}
	const int row = commands_->currentRow();
	const QString id = row >= 0 ? commands_->item(row, 0)->data(Qt::UserRole).toString() : QString();
	if (cameraInStandby_ && id == QStringLiteral("powerOff")) {
		setCommandResult(id, QStringLiteral("PASS"), QStringLiteral("Camera is already in standby; no packet sent."));
		setStatus(QStringLiteral("Camera is already in standby."));
		return;
	}
	const CameraProfileCommand binding = profile_->command(id);
	if (binding.destructive) {
		const QString what = id == QStringLiteral("presetStore")
				    ? QStringLiteral("Overwrite camera preset %1?").arg(presetNumber_->value())
				    : QStringLiteral("Send the destructive camera command '%1'?").arg(prettyName(id));
		if (QMessageBox::warning(this, QStringLiteral("Confirm camera command"), what,
					 QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
			return;
	}
	applyEditsToProfile();
	const bool requiresAwakeCamera = id != QStringLiteral("powerOn") && id != QStringLiteral("powerInquiry");
	setCommandResult(id, QStringLiteral("RUN"), requiresAwakeCamera ? QStringLiteral("Checking camera power state")
										 : QStringLiteral("Sending command"));
	QStringList rendered;
	for (int i = 0; i < packets.size(); ++i) {
		rendered.append(QStringLiteral("Step %1 TX: %2").arg(i + 1).arg(hexString(packets.at(i))));
	}
	preview_->setPlainText(rendered.join(QLatin1Char('\n')));
	trace_->appendPlainText(QStringLiteral("%1 · %2 · %3 packet(s)")
				 .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
			      profile_->id(), id));
	ioBusy_ = true;
	commands_->setEnabled(false);
	testConnectionButton_->setEnabled(false);
	QPointer<CameraProfileTestDialog> self(this);
	auto finishRequested = [self, id](bool ok, const QVector<QByteArray> &replies, const QString &message) {
		if (!self)
			return;
		QString resultMessage = message;
		if (id == QStringLiteral("powerOff") && ok)
			self->cameraInStandby_ = true;
		else if (id == QStringLiteral("powerOn") && ok)
			self->cameraInStandby_ = false;
		else if (id == QStringLiteral("powerInquiry") && ok) {
			bool cameraOn = false;
			if (replies.isEmpty() || !decodePowerState(replies.constLast(), &cameraOn)) {
				ok = false;
				resultMessage = QStringLiteral("Power inquiry reply did not contain a recognized On/Standby state.");
				self->trace_->appendPlainText(QStringLiteral("%1 · powerInquiry: FAILED · %2")
							      .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), resultMessage));
			} else {
				self->cameraInStandby_ = !cameraOn;
				self->setCommandResult(id, QStringLiteral("PASS"), cameraOn ? QStringLiteral("Camera reports On")
											 : QStringLiteral("Camera reports Off / Standby"));
				self->setStatus(cameraOn ? QStringLiteral("Camera reports On.")
									 : QStringLiteral("Camera reports Off / Standby."));
				self->trace_->appendPlainText(QStringLiteral("%1 · camera power state: %2")
							      .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
							   cameraOn ? QStringLiteral("On") : QStringLiteral("Off / Standby")));
			}
		}
		if (ok) {
			if (id != QStringLiteral("powerInquiry"))
				self->setCommandResult(id, QStringLiteral("PASS"), describeReplies(replies));
			if (id != QStringLiteral("powerInquiry"))
				self->trace_->appendPlainText(QStringLiteral("%1 · %2: complete")
							      .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), id));
			for (int i = 0; i < replies.size(); ++i) {
				if (!replies.at(i).isEmpty())
					self->trace_->appendPlainText(QStringLiteral("RX %1: %2").arg(i + 1).arg(hexString(replies.at(i))));
			}
			if (id != QStringLiteral("powerInquiry"))
				self->setStatus(QStringLiteral("%1 completed. See trace for camera replies.").arg(prettyName(id)));
		} else {
			self->setCommandResult(id, QStringLiteral("FAIL"), resultMessage);
			self->trace_->appendPlainText(QStringLiteral("%1 · %2: FAILED · %3")
						      .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), id, resultMessage));
			self->setStatus(resultMessage, true);
		}
		self->ioBusy_ = false;
		self->commands_->setEnabled(true);
		self->testConnectionButton_->setEnabled(true);
	};
	auto sendRequested = [self, id, packets, finishRequested]() {
		if (self)
			self->runPacketSequence(packets, id, finishRequested);
	};
	if (!requiresAwakeCamera) {
		sendRequested();
	} else {
		queryCameraPower([self, id, sendRequested, finishRequested](bool ok, bool cameraOn, const QString &error) {
			if (!self)
				return;
			if (!ok) {
				finishRequested(false, {}, QStringLiteral("Could not query camera power state; %1 was not sent. %2")
							  .arg(prettyName(id), error));
				return;
			}
			self->cameraInStandby_ = !cameraOn;
			if (cameraOn) {
				self->trace_->appendPlainText(QStringLiteral("%1 · camera reports On; sending %2")
							      .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), prettyName(id)));
				sendRequested();
				return;
			}
			if (id == QStringLiteral("powerOff")) {
				finishRequested(true, {}, QStringLiteral("Camera is already Off / Standby; no packet sent."));
				return;
			}
			self->trace_->appendPlainText(QStringLiteral("%1 · camera reports Off / Standby; sending Power On before %2")
						      .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), prettyName(id)));
			self->wakeCamera([self, id, sendRequested, finishRequested](bool woke, const QString &wakeError) {
				if (!self)
					return;
				if (!woke) {
					self->cameraInStandby_ = true;
					finishRequested(false, {}, QStringLiteral("Automatic Power On was not verified; %1 was not sent. %2")
								  .arg(prettyName(id), wakeError));
					return;
				}
				self->cameraInStandby_ = false;
				self->trace_->appendPlainText(QStringLiteral("%1 · camera power inquiry confirms On; sending %2")
							      .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), prettyName(id)));
				sendRequested();
			});
		});
	}
}

void CameraProfileTestDialog::startTestAll()
{
	if (ioBusy_) {
		setStatus(QStringLiteral("A camera test is already in progress."), true);
		return;
	}
	if (!profile_ || profile_->commandIds().isEmpty()) {
		setStatus(QStringLiteral("Load a profile with commands first."), true);
		return;
	}
	const QString variant = connectionVariant_->currentData(Qt::UserRole).toString();
	if (profile_->connectionTransport(variant) != QStringLiteral("tcp") ||
	    profile_->connectionFraming(variant) != QStringLiteral("raw-visca")) {
		setStatus(QStringLiteral("Test All currently supports raw VISCA over TCP only."), true);
		return;
	}
	const QString host = host_->text().trimmed();
	if (host.isEmpty()) {
		setStatus(QStringLiteral("Enter an IP address first."), true);
		return;
	}

	const QStringList ids = profile_->commandIdsInTestOrder();
	QStringList destructive;
	for (const QString &id : ids) {
		if (profile_->command(id).destructive)
			destructive.append(prettyName(id));
	}
	QString warning = QStringLiteral("Test All will send all %1 commands to %2:%3, one at a time. The camera may move and change state.")
			  .arg(ids.size()).arg(host).arg(port_->value());
	if (!destructive.isEmpty()) {
		warning += QStringLiteral("\n\nThis profile marks these commands destructive: %1. Preset-related commands use preset %2 and may change or erase it. Power Off may put the camera into standby.")
			   .arg(destructive.join(QStringLiteral(", "))).arg(presetNumber_->value());
	}
	warning += QStringLiteral("\n\nContinue with this test?");
	if (QMessageBox::warning(this, QStringLiteral("Confirm Test All"), warning,
				 QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
		return;

	testAllActive_ = true;
	ioBusy_ = true;
	profileControls_->setEnabled(false);
	connectionControls_->setEnabled(false);
	valueControls_->setEnabled(false);
	tokenControls_->setEnabled(false);
	setCommandInteractionEnabled(false);
	testAllButton_->setEnabled(false);
	testConnectionButton_->setEnabled(false);
	trace_->appendPlainText(QStringLiteral("%1 · TEST ALL START · profile=%2 · endpoint=%3:%4 · commands=%5")
				.arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), profile_->id(), host)
				.arg(port_->value()).arg(ids.size()));
	for (const QString &id : ids)
		setCommandResult(id, QStringLiteral("WAIT"), QStringLiteral("Queued for Test All"));
	setStatus(QStringLiteral("Testing %1 commands on %2:%3…").arg(ids.size()).arg(host).arg(port_->value()));
	runTestAllCommand(0);
}

void CameraProfileTestDialog::runTestAllCommand(int index)
{
	if (!testAllActive_ || !profile_)
		return;
	const QStringList ids = profile_->commandIdsInTestOrder();
	if (index >= ids.size()) {
		finishTestAll();
		return;
	}
	const QString id = ids.at(index);
	for (int row = 0; row < commands_->rowCount(); ++row) {
		const QTableWidgetItem *item = commands_->item(row, 0);
		if (item && item->data(Qt::UserRole).toString() == id) {
			commands_->setCurrentCell(row, 0);
			break;
		}
	}
	QVector<QByteArray> packets;
	QString error;
	if (!renderViscaTemplates(selectedTemplates(), templateValues(), &packets, &error)) {
		setCommandResult(id, QStringLiteral("FAIL"), QStringLiteral("Could not render: %1").arg(error));
		trace_->appendPlainText(QStringLiteral("%1 · %2: FAIL · render error · %3")
					.arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), id, error));
		runTestAllCommand(index + 1);
		return;
	}

	QStringList rendered;
	for (int packetIndex = 0; packetIndex < packets.size(); ++packetIndex)
		rendered.append(QStringLiteral("Step %1 TX: %2").arg(packetIndex + 1).arg(hexString(packets.at(packetIndex))));
	preview_->setPlainText(rendered.join(QLatin1Char('\n')));
	setCommandResult(id, QStringLiteral("RUN"), QStringLiteral("Waiting for VISCA reply"));
	trace_->appendPlainText(QStringLiteral("%1 · TEST %2 · %3 packet(s) · preset=%4")
				.arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), id)
				.arg(packets.size()).arg(presetNumber_->value()));
	runPacketSequence(packets, id,
			  [self = QPointer<CameraProfileTestDialog>(this), index, id](bool ok, const QVector<QByteArray> &replies, const QString &message) {
				  if (!self || !self->testAllActive_)
					  return;
				  bool commandOk = ok;
				  QString detail = ok ? describeReplies(replies) : message;
				  if (ok && id == QStringLiteral("powerOff"))
					  self->cameraInStandby_ = true;
				  else if (ok && id == QStringLiteral("powerOn"))
					  self->cameraInStandby_ = false;
				  else if (ok && id == QStringLiteral("powerInquiry")) {
					  bool cameraOn = false;
					  if (replies.isEmpty() || !decodePowerState(replies.constLast(), &cameraOn)) {
						  commandOk = false;
						  detail = QStringLiteral("Power inquiry reply did not contain a recognized On/Standby state.");
					  } else {
						  self->cameraInStandby_ = !cameraOn;
						  detail = cameraOn ? QStringLiteral("Camera reports On") : QStringLiteral("Camera reports Off / Standby");
					  }
				  }
				  self->setCommandResult(id, commandOk ? QStringLiteral("PASS") : QStringLiteral("FAIL"), detail);
				  self->trace_->appendPlainText(QStringLiteral("%1 · %2: %3 · %4")
								  .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), id,
								       commandOk ? QStringLiteral("PASS") : QStringLiteral("FAIL"), detail));
				  self->runTestAllCommand(index + 1);
			  });
}

void CameraProfileTestDialog::finishTestAll()
{
	if (!testAllActive_)
		return;
	testAllActive_ = false;
	ioBusy_ = false;
	profileControls_->setEnabled(true);
	connectionControls_->setEnabled(true);
	valueControls_->setEnabled(true);
	tokenControls_->setEnabled(true);
	setCommandInteractionEnabled(true);
	testAllButton_->setEnabled(profile_ && !profile_->commandIds().isEmpty());
	testConnectionButton_->setEnabled(true);
	int passed = 0;
	int failed = 0;
	for (int row = 0; row < commands_->rowCount(); ++row) {
		const QTableWidgetItem *result = commands_->item(row, 4);
		if (result && result->text() == QStringLiteral("PASS"))
			++passed;
		else if (result && result->text() == QStringLiteral("FAIL"))
			++failed;
	}
	const QString summary = QStringLiteral("Test All finished: %1 passed, %2 failed. Review Send / Reply Trace for packet bytes and raw replies.")
			       .arg(passed).arg(failed);
	trace_->appendPlainText(QStringLiteral("%1 · TEST ALL END · PASS=%2 FAIL=%3")
				.arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))).arg(passed).arg(failed));
	setStatus(summary, failed > 0);
}

void CameraProfileTestDialog::setCommandResult(const QString &commandId, const QString &result, const QString &details)
{
	for (int row = 0; row < commands_->rowCount(); ++row) {
		const QTableWidgetItem *name = commands_->item(row, 0);
		if (!name || name->data(Qt::UserRole).toString() != commandId)
			continue;
		QTableWidgetItem *item = commands_->item(row, 4);
		if (!item) {
			item = new QTableWidgetItem;
			item->setTextAlignment(Qt::AlignCenter);
			item->setFlags(Qt::ItemIsEnabled);
			commands_->setItem(row, 4, item);
		}
		item->setText(result);
		item->setToolTip(details);
		if (result == QStringLiteral("PASS")) {
			item->setBackground(QColor(QStringLiteral("#BFE6C5")));
			item->setForeground(QColor(QStringLiteral("#173B1D")));
		} else if (result == QStringLiteral("FAIL")) {
			item->setBackground(QColor(QStringLiteral("#F0BDBD")));
			item->setForeground(QColor(QStringLiteral("#5C1515")));
		} else if (result == QStringLiteral("RUN")) {
			item->setBackground(QColor(QStringLiteral("#FFE18A")));
			item->setForeground(QColor(QStringLiteral("#493500")));
		} else if (result == QStringLiteral("WAIT")) {
			item->setBackground(QColor(QStringLiteral("#D7D7D7")));
			item->setForeground(QColor(QStringLiteral("#303030")));
		} else {
			item->setBackground(QBrush());
			item->setForeground(QBrush());
		}
		return;
	}
}

void CameraProfileTestDialog::setCommandInteractionEnabled(bool enabled)
{
	for (int row = 0; row < commands_->rowCount(); ++row) {
		if (QWidget *editor = commands_->cellWidget(row, 1))
			editor->setEnabled(enabled);
		if (auto *render = qobject_cast<QPushButton *>(commands_->cellWidget(row, 2)))
			render->setEnabled(enabled);
		if (auto *send = qobject_cast<QPushButton *>(commands_->cellWidget(row, 3)))
			send->setEnabled(enabled);
	}
}

void CameraProfileTestDialog::testConnection()
{
	if (ioBusy_) {
		setStatus(QStringLiteral("A camera test is already in progress."), true);
		return;
	}
	const QString host = host_->text().trimmed();
	const quint16 port = static_cast<quint16>(port_->value());
	if (host.isEmpty()) {
		setStatus(QStringLiteral("Enter an IP address first."), true);
		return;
	}
	const QString variant = connectionVariant_->currentData(Qt::UserRole).toString();
	if (profile_->connectionTransport(variant) != QStringLiteral("tcp") ||
	    profile_->connectionFraming(variant) != QStringLiteral("raw-visca")) {
		setStatus(QStringLiteral("This profile transport is not available in the first pass."), true);
		return;
	}
	auto *socket = new QTcpSocket(this);
	auto *timer = new QTimer(socket);
	timer->setSingleShot(true);
	auto done = std::make_shared<bool>(false);
	auto finish = [this, socket, timer, done](bool ok, const QString &message) {
		if (*done)
			return;
		*done = true;
		timer->stop();
		socket->abort();
		socket->deleteLater();
		trace_->appendPlainText(QStringLiteral("%1 · connection: %2")
					.arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), message));
		setStatus(message, !ok);
		ioBusy_ = false;
		commands_->setEnabled(true);
		testConnectionButton_->setEnabled(true);
	};
	connect(timer, &QTimer::timeout, socket, [finish]() mutable {
		finish(false, QStringLiteral("TCP connection timed out"));
	});
	connect(socket, &QTcpSocket::connected, socket, [finish, host, port]() mutable {
		finish(true, QStringLiteral("TCP connected to %1:%2; no camera command was sent").arg(host).arg(port));
	});
	connect(socket, &QTcpSocket::errorOccurred, socket, [finish, socket](QAbstractSocket::SocketError) mutable {
		finish(false, QStringLiteral("TCP connection failed: %1").arg(socket->errorString()));
	});
	timer->start(3000);
	ioBusy_ = true;
	commands_->setEnabled(false);
	testConnectionButton_->setEnabled(false);
	socket->connectToHost(host, port);
}

void CameraProfileTestDialog::insertToken(const QString &token)
{
	const int row = commands_->currentRow();
	if (row >= 0)
		activeEditor_ = commands_->cellWidget(row, 1);
	if (!activeEditor_)
		return;
	if (auto *lineEditor = qobject_cast<QLineEdit *>(activeEditor_.data()))
		lineEditor->insert(token);
	else if (auto *multiLineEditor = qobject_cast<QPlainTextEdit *>(activeEditor_.data()))
		multiLineEditor->insertPlainText(token);
	activeEditor_->setFocus();
}

void CameraProfileTestDialog::applyEditsToProfile()
{
	if (!profile_)
		return;
	for (int row = 0; row < commands_->rowCount(); ++row) {
		const QString id = commands_->item(row, 0)->data(Qt::UserRole).toString();
		QWidget *editorWidget = commands_->cellWidget(row, 1);
		if (!editorWidget)
			continue;
		QStringList lines;
		if (auto *lineEditor = qobject_cast<QLineEdit *>(editorWidget))
			lines = splitTemplateLine(lineEditor->text());
		profile_->setCommandTemplates(id, lines);
	}
}

void CameraProfileTestDialog::saveAs()
{
	if (!profile_)
		return;
	applyEditsToProfile();
	const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
				  QStringLiteral("/simplecamdirect/profiles");
	const QString defaultName = profile_->id().section(QLatin1Char('.'), -1) + QStringLiteral(".camera.cfg");
	const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Camera Profile As"),
							 directory + QLatin1Char('/') + defaultName,
							 QStringLiteral("Camera profiles (*.camera.cfg *.json)"));
	if (path.isEmpty())
		return;
	if (QFileInfo(path).canonicalFilePath() == QFileInfo(profile_->path()).canonicalFilePath()) {
		setStatus(QStringLiteral("Save As needs a new file path so the source profile stays unchanged."), true);
		return;
	}
	QDir().mkpath(QFileInfo(path).absolutePath());
	QString error;
	if (!profile_->saveAs(path, &error)) {
		setStatus(error, true);
		return;
	}
	profilePath_->setText(path);
	setStatus(QStringLiteral("Saved edited profile copy: %1").arg(path));
}

void CameraProfileTestDialog::runPacketSequence(
	const QVector<QByteArray> &packets, const QString &commandId,
	std::function<void(bool, const QVector<QByteArray> &, const QString &)> completion)
{
	const QString host = host_->text().trimmed();
	if (host.isEmpty() || packets.isEmpty()) {
		completion(false, {}, QStringLiteral("Missing host or rendered packet"));
		return;
	}
	const bool waitForCompletion = viscaCommandWaitsForCompletion(commandId);
	QPointer<CameraProfileTestDialog> self(this);
	auto replies = std::make_shared<QVector<QByteArray>>();
	auto runner = std::make_shared<std::function<void(int)>>();
	std::weak_ptr<std::function<void(int)>> weakRunner = runner;
	*runner = [self, packets, completion, replies, weakRunner, waitForCompletion](int index) {
		if (!self)
			return;
		if (index >= packets.size()) {
			completion(true, *replies, {});
			return;
		}
		auto keepRunnerAlive = weakRunner.lock();
		if (!keepRunnerAlive) {
			completion(false, *replies, QStringLiteral("VISCA sequence runner expired"));
			return;
		}
		const QByteArray packet = packets.at(index);
		self->sendOnePacket(packet, waitForCompletion, commandIsInquiry(packet),
			[thisWeak = self, index, keepRunnerAlive, replies, completion](bool ok, const QByteArray &reply, const QString &error) {
				if (!thisWeak)
					return;
				replies->append(reply);
				if (!ok) {
					completion(false, *replies, error);
					return;
				}
				(*keepRunnerAlive)(index + 1);
			});
	};
	(*runner)(0);
}

void CameraProfileTestDialog::queryCameraPower(std::function<void(bool, bool, const QString &)> completion)
{
	if (!profile_) {
		completion(false, false, QStringLiteral("No camera profile is loaded."));
		return;
	}
	const CameraProfileCommand binding = profile_->command(QStringLiteral("powerInquiry"));
	QVector<QByteArray> packets;
	QString error;
	if (binding.templates.isEmpty() || !renderViscaTemplates(binding.templates, templateValues(), &packets, &error) ||
	    packets.size() != 1) {
		completion(false, false, error.isEmpty() ? QStringLiteral("Profile has no valid one-packet power inquiry.") : error);
		return;
	}
	trace_->appendPlainText(QStringLiteral("%1 · checking camera power state")
				.arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))));
	sendOnePacket(packets.first(), false, true,
		      [this, completion](bool ok, const QByteArray &reply, const QString &message) {
			      if (!ok) {
				      completion(false, false, message);
				      return;
			      }
			      bool cameraOn = false;
			      if (!decodePowerState(reply, &cameraOn)) {
				      completion(false, false, QStringLiteral("Unrecognized power inquiry reply: %1").arg(hexString(reply)));
				      return;
			      }
			      cameraInStandby_ = !cameraOn;
			      completion(true, cameraOn, {});
		      }, 1200);
}

void CameraProfileTestDialog::waitForCameraOn(int attemptsRemaining,
						      std::function<void(bool, const QString &)> completion)
{
	queryCameraPower([this, attemptsRemaining, completion](bool ok, bool cameraOn, const QString &message) {
		if (ok && cameraOn) {
			completion(true, {});
			return;
		}
		if (attemptsRemaining <= 1) {
			const QString reason = ok ? QStringLiteral("Camera continued to report Off / Standby.") : message;
			completion(false, QStringLiteral("Power inquiry did not verify On after repeated checks. %1").arg(reason));
			return;
		}
		QTimer::singleShot(500, this, [this, attemptsRemaining, completion]() {
			waitForCameraOn(attemptsRemaining - 1, completion);
		});
	});
}

void CameraProfileTestDialog::wakeCamera(std::function<void(bool, const QString &)> completion)
{
	if (!profile_) {
		completion(false, QStringLiteral("No camera profile is loaded."));
		return;
	}
	const CameraProfileCommand binding = profile_->command(QStringLiteral("powerOn"));
	QVector<QByteArray> packets;
	QString error;
	if (binding.templates.isEmpty() || !renderViscaTemplates(binding.templates, templateValues(), &packets, &error) ||
	    packets.size() != 1) {
		completion(false, error.isEmpty() ? QStringLiteral("Profile has no valid one-packet Power On command.") : error);
		return;
	}
	sendOnePacket(packets.first(), false, false,
		      [this, completion](bool ok, const QByteArray &reply, const QString &message) {
			      if (!ok) {
				      completion(false, QStringLiteral("Power On was not acknowledged: %1").arg(message));
				      return;
			      }
			      trace_->appendPlainText(QStringLiteral("%1 · Power On response: %2; polling power inquiry")
						      .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), hexString(reply)));
			      QTimer::singleShot(400, this, [this, completion]() { waitForCameraOn(12, completion); });
		      }, 3000);
}

void CameraProfileTestDialog::sendOnePacket(
	const QByteArray &packet, bool waitForCompletion, bool inquiry,
	std::function<void(bool, const QByteArray &, const QString &)> completion, int timeoutMs)
{
	const QString host = host_->text().trimmed();
	const quint16 port = static_cast<quint16>(port_->value());
	auto *socket = new QTcpSocket(this);
	auto *timer = new QTimer(socket);
	timer->setSingleShot(true);
	auto finished = std::make_shared<bool>(false);
	auto input = std::make_shared<QByteArray>();
	auto replies = std::make_shared<QByteArray>();
	auto finish = [socket, timer, finished, replies, completion](bool ok, const QString &error) mutable {
		if (*finished)
			return;
		*finished = true;
		timer->stop();
		socket->abort();
		socket->deleteLater();
		completion(ok, *replies, error);
	};
	connect(timer, &QTimer::timeout, socket, [finish]() mutable {
		finish(false, QStringLiteral("Timed out waiting for VISCA reply"));
	});
	connect(socket, &QTcpSocket::connected, socket, [socket, packet, timer, timeoutMs]() {
		const qint64 written = socket->write(packet);
		if (written != packet.size()) {
			socket->abort();
			return;
		}
		socket->flush();
		timer->start(timeoutMs);
	});
	connect(socket, &QTcpSocket::readyRead, socket,
		 [socket, input, replies, inquiry, waitForCompletion, finish, this]() mutable {
			input->append(socket->readAll());
			while (true) {
				const qsizetype end = input->indexOf(static_cast<char>(0xFF));
				if (end < 0)
					return;
				const QByteArray frame = input->left(end + 1);
				input->remove(0, end + 1);
				replies->append(frame);
				trace_->appendPlainText(QStringLiteral("%1 · RX: %2")
							.arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), hexString(frame)));
				if (frame.size() < 3)
					continue;
				const quint8 kind = static_cast<quint8>(frame.at(1)) & 0xF0;
				if (kind == 0x60) {
					finish(false, QStringLiteral("Camera rejected command: %1").arg(hexString(frame)));
					return;
				}
				if (inquiry && kind == 0x50) {
					finish(true, {});
					return;
				}
				if (kind == 0x40 && !waitForCompletion) {
					finish(true, {});
					return;
				}
				if (!inquiry && kind == 0x50) {
					finish(true, {});
					return;
				}
			}
		});
	connect(socket, &QTcpSocket::errorOccurred, socket,
		 [socket, finish](QAbstractSocket::SocketError) mutable {
			finish(false, QStringLiteral("VISCA TCP error: %1").arg(socket->errorString()));
		});
	trace_->appendPlainText(QStringLiteral("%1 · TX: %2")
				.arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), hexString(packet)));
	timer->start(timeoutMs);
	socket->connectToHost(host, port);
}

void CameraProfileTestDialog::queryZoom(std::function<void(bool, int, const QString &)> completion)
{
	const CameraProfileCommand command = profile_->command(QStringLiteral("zoomInquiry"));
	QVector<QByteArray> packets;
	QString error;
	if (!renderViscaTemplates(command.templates, templateValues(), &packets, &error) || packets.size() != 1) {
		completion(false, -1, error.isEmpty() ? QStringLiteral("Zoom inquiry must be one packet") : error);
		return;
	}
	sendOnePacket(packets.first(), false, true, [completion](bool ok, const QByteArray &reply, const QString &message) {
		if (!ok) {
			completion(false, -1, message);
			return;
		}
		const qsizetype marker = reply.indexOf(static_cast<char>(0x50));
		if (marker < 0 || reply.size() < marker + 6) {
			completion(false, -1, QStringLiteral("Zoom inquiry reply did not contain four position nibbles"));
			return;
		}
		int position = 0;
		for (int i = 0; i < 4; ++i) {
			const quint8 byte = static_cast<quint8>(reply.at(marker + 1 + i));
			if (byte > 0x0F) {
				completion(false, -1, QStringLiteral("Zoom inquiry returned a non-nibble position byte"));
				return;
			}
			position = (position << 4) | byte;
		}
		completion(true, position, {});
	});
}

void CameraProfileTestDialog::startZoomDiscovery()
{
	if (zoomDiscoveryActive_) {
		if (zoomDiscoveryFinishing_)
			return;
		zoomDiscoveryCancelRequested_ = true;
		discoverZoomButton_->setText(QStringLiteral("Canceling…"));
		return;
	}
	if (ioBusy_) {
		setStatus(QStringLiteral("Wait for the current camera test to finish."), true);
		return;
	}
	if (!profile_ || !profile_->commandIds().contains(QStringLiteral("zoomInquiry")) ||
	    !profile_->commandIds().contains(QStringLiteral("zoomIn")) ||
	    !profile_->commandIds().contains(QStringLiteral("zoomStop"))) {
		setStatus(QStringLiteral("This profile needs Zoom In, Zoom Stop, and Zoom Inquiry to discover its range."), true);
		return;
	}
	if (QMessageBox::warning(this, QStringLiteral("Discover Zoom Range"),
				 QStringLiteral("The camera will move toward telephoto, be queried until its position stops increasing, then return to its starting position when absolute zoom is available. Continue?"),
				 QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
		return;
	zoomDiscoveryActive_ = true;
	ioBusy_ = true;
	zoomDiscoveryCancelRequested_ = false;
	zoomDiscoveryFinishing_ = false;
	zoomDiscoveryStable_ = 0;
	zoomDiscoveryPolls_ = 0;
	zoomDiscoveryMaximum_ = 0;
	zoomDiscoveryNoProgressPolls_ = 0;
	zoomDiscoveryAdvanced_ = false;
	discoverZoomButton_->setText(QStringLiteral("Cancel Zoom Discovery"));
	commands_->setEnabled(false);
	testConnectionButton_->setEnabled(false);
	panSpeed_->setEnabled(false);
	tiltSpeed_->setEnabled(false);
	panDirection_->setEnabled(false);
	tiltDirection_->setEnabled(false);
	zoomSpeed_->setEnabled(false);
	zoomPosition_->setEnabled(false);
	presetNumber_->setEnabled(false);
	digitalZoomMode_->setEnabled(false);
	cameraSlot_->setEnabled(false);
	connectionVariant_->setEnabled(false);
	host_->setEnabled(false);
	port_->setEnabled(false);
	address_->setEnabled(false);
	setStatus(QStringLiteral("Reading starting zoom position…"));
	QPointer<CameraProfileTestDialog> self(this);
	queryZoom([self](bool ok, int position, const QString &message) {
		if (!self || !self->zoomDiscoveryActive_)
			return;
		if (!ok) {
			self->setStatus(message, true);
			self->finishZoomDiscovery(true);
			return;
		}
		self->zoomDiscoveryInitial_ = position;
		self->zoomDiscoveryLast_ = position;
		self->zoomDiscoveryMaximum_ = position;
		if (self->zoomDiscoveryCancelRequested_) {
			self->finishZoomDiscovery(true);
			return;
		}
		const QString startHex = QStringLiteral("%1").arg(position, 4, 16, QLatin1Char('0')).toUpper();
		self->setStatus(QStringLiteral("Starting position 0x%1; moving toward telephoto…").arg(startHex));
		QVector<QByteArray> packets;
		QString renderError;
		if (!renderViscaTemplates(self->profile_->command(QStringLiteral("zoomIn")).templates,
					  self->templateValues(), &packets, &renderError) || packets.isEmpty()) {
			self->setStatus(renderError, true);
			self->finishZoomDiscovery(true);
			return;
		}
		self->sendOnePacket(packets.first(), false, false, [self](bool sent, const QByteArray &, const QString &sendError) {
			if (!self || !self->zoomDiscoveryActive_)
				return;
			if (!sent) {
				self->setStatus(sendError, true);
				self->finishZoomDiscovery(true);
				return;
			}
			self->trace_->appendPlainText(QStringLiteral("Zoom discovery: zoom drive accepted; polling position."));
			QTimer::singleShot(150, self, [self]() { if (self) self->pollZoomDiscovery(); });
		});
	});
}

void CameraProfileTestDialog::pollZoomDiscovery()
{
	if (!zoomDiscoveryActive_)
		return;
	if (zoomDiscoveryCancelRequested_ || zoomDiscoveryPolls_ >= 120) {
		finishZoomDiscovery(zoomDiscoveryCancelRequested_, !zoomDiscoveryCancelRequested_);
		return;
	}
	++zoomDiscoveryPolls_;
	QPointer<CameraProfileTestDialog> self(this);
	queryZoom([self](bool readOk, int value, const QString &readError) {
		if (!self || !self->zoomDiscoveryActive_)
			return;
		if (!readOk) {
			self->setStatus(readError, true);
			self->finishZoomDiscovery(true);
			return;
		}
		if (value > self->zoomDiscoveryMaximum_) {
			self->zoomDiscoveryMaximum_ = value;
			self->zoomDiscoveryStable_ = 0;
			self->zoomDiscoveryAdvanced_ = true;
			self->zoomDiscoveryNoProgressPolls_ = 0;
		} else if (self->zoomDiscoveryAdvanced_ && value <= self->zoomDiscoveryLast_) {
			++self->zoomDiscoveryStable_;
		} else if (self->zoomDiscoveryAdvanced_) {
			self->zoomDiscoveryStable_ = 0;
		} else {
			++self->zoomDiscoveryNoProgressPolls_;
		}
		self->zoomDiscoveryLast_ = value;
		self->setStatus(QStringLiteral("Zoom discovery %1/120 · current 0x%2 · maximum 0x%3 · stable %4/3")
				  .arg(self->zoomDiscoveryPolls_)
				  .arg(value, 4, 16, QLatin1Char('0')).arg(self->zoomDiscoveryMaximum_, 4, 16, QLatin1Char('0'))
				  .arg(self->zoomDiscoveryStable_));
		if (!self->zoomDiscoveryAdvanced_ && self->zoomDiscoveryNoProgressPolls_ >= 20) {
			self->finishZoomDiscovery(false, false, true);
			return;
		}
		if (self->zoomDiscoveryCancelRequested_ || (self->zoomDiscoveryAdvanced_ && self->zoomDiscoveryStable_ >= 3) ||
		    self->zoomDiscoveryPolls_ >= 120) {
			self->finishZoomDiscovery(self->zoomDiscoveryCancelRequested_,
						  !self->zoomDiscoveryCancelRequested_ && self->zoomDiscoveryStable_ < 3);
			return;
		}
		QTimer::singleShot(150, self, [self]() { if (self) self->pollZoomDiscovery(); });
	});
}

void CameraProfileTestDialog::finishZoomDiscovery(bool cancelled, bool timedOut, bool noProgress)
{
	if (!zoomDiscoveryActive_ || zoomDiscoveryFinishing_)
		return;
	zoomDiscoveryFinishing_ = true;
	discoverZoomButton_->setText(QStringLiteral("Stopping zoom…"));
	discoverZoomButton_->setEnabled(false);
	if (!profile_ || !profile_->commandIds().contains(QStringLiteral("zoomStop")))
	{
		completeZoomDiscoveryUi();
		return;
	}
	QVector<QByteArray> stopPackets;
	QString error;
	if (!renderViscaTemplates(profile_->command(QStringLiteral("zoomStop")).templates,
				  templateValues(), &stopPackets, &error) || stopPackets.isEmpty()) {
		setStatus(error, true);
		completeZoomDiscoveryUi();
		return;
	}
	QPointer<CameraProfileTestDialog> self(this);
	sendOnePacket(stopPackets.first(), true, false, [self, cancelled, timedOut, noProgress](bool stopOk, const QByteArray &, const QString &stopError) {
		if (!self)
			return;
		if (!stopOk) {
			self->setStatus(QStringLiteral("Zoom stop failed: %1").arg(stopError), true);
			self->completeZoomDiscoveryUi();
			return;
		}
		self->queryZoom([self, cancelled, timedOut, noProgress](bool readOk, int endpoint, const QString &readError) {
			if (!self)
				return;
			if (!readOk) {
				self->setStatus(QStringLiteral("Stopped movement, but endpoint query failed: %1").arg(readError), true);
				self->completeZoomDiscoveryUi();
				return;
			}
			self->zoomDiscoveryMaximum_ = std::max(self->zoomDiscoveryMaximum_, endpoint);
			const bool completed = !cancelled && !timedOut && !noProgress && self->zoomDiscoveryAdvanced_;
			self->trace_->appendPlainText(QStringLiteral("Zoom discovery %1: endpoint 0x%2; starting position 0x%3.")
					      .arg(cancelled ? QStringLiteral("canceled") : (timedOut ? QStringLiteral("timed out") : (noProgress ? QStringLiteral("no movement observed") : QStringLiteral("complete"))))
					      .arg(endpoint, 4, 16, QLatin1Char('0')).arg(self->zoomDiscoveryInitial_, 4, 16, QLatin1Char('0')));
			if (completed) {
				QString profileError;
				if (!self->profile_->setDiscoveredZoomEndpoint(endpoint,
									 self->digitalZoomMode_->currentData().toString(), &profileError)) {
					self->setStatus(QStringLiteral("Endpoint measured, but profile draft was not updated: %1").arg(profileError), true);
					self->completeZoomDiscoveryUi();
					return;
				}
			}
			const bool hasAbsoluteZoom = self->profile_->commandIds().contains(QStringLiteral("zoomAbsolute"));
			if (self->zoomDiscoveryInitial_ >= 0 && hasAbsoluteZoom) {
				ViscaTemplateValues values = self->templateValues();
				values.zoomPosition = self->zoomDiscoveryInitial_;
				QVector<QByteArray> restore;
				QString restoreError;
				if (renderViscaTemplates(self->profile_->command(QStringLiteral("zoomAbsolute")).templates,
							 values, &restore, &restoreError) && !restore.isEmpty()) {
					self->runPacketSequence(restore, QStringLiteral("zoomAbsolute"), [self, endpoint, cancelled, timedOut, noProgress, completed](bool ok, const QVector<QByteArray> &, const QString &restoreMessage) {
						if (!self)
							return;
						const QString outcome = cancelled ? QStringLiteral("canceled") : (timedOut ? QStringLiteral("timed out") : (noProgress ? QStringLiteral("no movement observed") : QStringLiteral("discovered")));
						self->setStatus(ok ? QStringLiteral("Zoom discovery %1 at 0x%2. Starting position restored%3.")
								   .arg(outcome).arg(endpoint, 4, 16, QLatin1Char('0'))
								   .arg(!completed ? QStringLiteral("; endpoint not saved") : QStringLiteral("; endpoint saved to the unsaved profile draft"))
							   : QStringLiteral("Zoom discovery %1 at 0x%2; restore failed: %3")
								   .arg(outcome).arg(endpoint, 4, 16, QLatin1Char('0')).arg(restoreMessage), !ok);
						self->completeZoomDiscoveryUi();
					});
					return;
				}
				self->trace_->appendPlainText(QStringLiteral("Zoom restore could not render: %1").arg(restoreError));
			}
			const QString restoreNote = self->zoomDiscoveryInitial_ < 0
						    ? QStringLiteral("Starting position could not be read, so there is no restore target")
						    : (hasAbsoluteZoom
							       ? QStringLiteral("An absolute-zoom restore packet could not be rendered")
							       : QStringLiteral("Absolute zoom restore is unavailable"));
			self->setStatus(QStringLiteral("Zoom discovery %1 at 0x%2. Camera remains stopped at its current position. %3%4.")
					.arg(cancelled ? QStringLiteral("canceled") : (timedOut ? QStringLiteral("timed out") : (noProgress ? QStringLiteral("no movement observed") : QStringLiteral("complete"))))
					.arg(endpoint, 4, 16, QLatin1Char('0'))
					.arg(restoreNote)
					.arg(!completed ? QStringLiteral("; endpoint not saved") : QStringLiteral("; endpoint saved to the unsaved profile draft")), false);
			self->completeZoomDiscoveryUi();
		});
	});
}

void CameraProfileTestDialog::completeZoomDiscoveryUi()
{
	zoomDiscoveryActive_ = false;
	zoomDiscoveryFinishing_ = false;
	zoomDiscoveryCancelRequested_ = false;
	discoverZoomButton_->setText(QStringLiteral("Discover Zoom Range"));
	discoverZoomButton_->setEnabled(true);
	commands_->setEnabled(true);
	testConnectionButton_->setEnabled(true);
	panSpeed_->setEnabled(true);
	tiltSpeed_->setEnabled(true);
	panDirection_->setEnabled(true);
	tiltDirection_->setEnabled(true);
	zoomSpeed_->setEnabled(true);
	zoomPosition_->setEnabled(true);
	presetNumber_->setEnabled(true);
	digitalZoomMode_->setEnabled(true);
	cameraSlot_->setEnabled(true);
	connectionVariant_->setEnabled(true);
	host_->setEnabled(true);
	port_->setEnabled(true);
	address_->setEnabled(true);
	ioBusy_ = false;
}

} // namespace simplecam
