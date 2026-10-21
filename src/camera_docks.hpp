#pragma once

#include <QWidget>

#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QToolButton;
class QTimer;

namespace simplecam {

class CameraRuntime;

class CameraControlDock final : public QWidget {
public:
	explicit CameraControlDock(CameraRuntime &runtime, QWidget *parent = nullptr);

private:
	enum class Mode { Recall, Set, Rename, Swap, Clear };

	void buildUi();
	void refresh();
	void refreshPageManagement();
	void setMode(Mode mode);
	void handlePresetButton(int slotIndex);
	void flashPresetButton(int slotIndex);
	void setStatus(const QString &message, bool error = false);
	void sendPanTilt(int panDirection, int tiltDirection);
	void sendStopPanTilt();

	CameraRuntime &runtime_;
	Mode mode_ = Mode::Recall;
	QPushButton *pageButton_ = nullptr;
	QLabel *pageDescription_ = nullptr;
	QLineEdit *pageDescriptionEdit_ = nullptr;
	QToolButton *addPageButton_ = nullptr;
	QToolButton *removePageButton_ = nullptr;
	QPushButton *settingsButton_ = nullptr;
	QVector<QPushButton *> presetButtons_;
	QPushButton *setButton_ = nullptr;
	QPushButton *renameButton_ = nullptr;
	QPushButton *swapButton_ = nullptr;
	QPushButton *clearButton_ = nullptr;
	QLabel *statusLabel_ = nullptr;
	QSlider *speedSlider_ = nullptr;
	QCheckBox *backlightCheck_ = nullptr;
	QTimer *homeDoubleClickTimer_ = nullptr;
	QTimer *flashTimers_[14]{};
	bool settingsMode_ = false;
	bool pageNameDirty_ = false;
	int pendingSwapPage_ = -1;
	int pendingSwapSlot_ = -1;
};

class CamerasDock final : public QWidget {
public:
	explicit CamerasDock(CameraRuntime &runtime, QWidget *parent = nullptr);

private:
	CameraRuntime &runtime_;
	QLineEdit *cameraNameEdit_ = nullptr;
	QLineEdit *hostEdit_ = nullptr;
	QSpinBox *portSpin_ = nullptr;
	QSpinBox *addressSpin_ = nullptr;
	QSpinBox *capacitySpin_ = nullptr;
	QLabel *statusLabel_ = nullptr;
};

} // namespace simplecam
