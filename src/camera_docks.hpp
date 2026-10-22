#pragma once

#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QHBoxLayout;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QSlider;
class QSpinBox;
class QString;
class QToolButton;
class QTimer;

namespace simplecam {

class CameraRuntime;

class CameraControlCard final : public QWidget {
public:
	explicit CameraControlCard(CameraRuntime &runtime, int cameraNumber, bool embedded = false, QWidget *parent = nullptr);
	void setSettingsMode(bool enabled);
	void refreshView();
	void setRuntime(CameraRuntime &runtime, int cameraNumber);
	void setCameraSelectionHandler(std::function<void(int)> handler);

private:
	friend class CameraControlDock;
	enum class Mode { Recall, Set, Rename, Swap, Clear };

	void buildUi();
	void refresh();
	void refreshPageManagement();
	void setMode(Mode mode);
	void handlePresetButton(int slotIndex);
	void flashPresetButton(int slotIndex, const char *origin = "button-action");
	void releasePresetFlashLayoutIfIdle();
	void setStatus(const QString &message, bool error = false, int displayMs = 4000);
	void sendPanTilt(int panDirection, int tiltDirection);
	void sendStopPanTilt();

	CameraRuntime *runtime_ = nullptr;
	QWidget *cameraHeaderWidget_ = nullptr;
	QWidget *groupToolsWidget_ = nullptr;
	bool embedded_ = false;
	int cameraNumber_ = 1;
	Mode mode_ = Mode::Recall;
	QPushButton *pageButton_ = nullptr;
	QLabel *pageDescription_ = nullptr;
	QLineEdit *pageDescriptionEdit_ = nullptr;
	QLabel *pageNumberLabel_ = nullptr;
	QLabel *settingsInstructions_ = nullptr;
	QLabel *settingsPromptLabel_ = nullptr;
	QWidget *navigationWidget_ = nullptr;
	QToolButton *addPageButton_ = nullptr;
	QToolButton *removePageButton_ = nullptr;
	QPushButton *settingsButton_ = nullptr;
	QLabel *cameraTitle_ = nullptr;
	QWidget *cameraSelectorWidget_ = nullptr;
	QPushButton *cameraSelectButtons_[2] = {nullptr, nullptr};
	QLabel *zoomValueLabel_ = nullptr;
	QVector<QPushButton *> presetButtons_;
	QPushButton *setButton_ = nullptr;
	QPushButton *renameButton_ = nullptr;
	QPushButton *swapButton_ = nullptr;
	QPushButton *clearButton_ = nullptr;
	QLabel *statusLabel_ = nullptr;
	QSlider *speedSlider_ = nullptr;
	QSlider *zoomSlider_ = nullptr;
	QPushButton *manualFocusButton_ = nullptr;
	QPushButton *focusPlusButton_ = nullptr;
	QPushButton *focusMinusButton_ = nullptr;
	QPushButton *backlightButton_ = nullptr;
	QPushButton *exposureButton_ = nullptr;
	QPushButton *powerButton_ = nullptr;
	QTimer *homeDoubleClickTimer_ = nullptr;
	QTimer *flashAnimations_[14]{};
	bool presetFlashLayoutFrozen_ = false;
	bool presetFlashLayoutWasEnabled_ = false;
	bool settingsMode_ = false;
	bool groupToolsSuppressed_ = false;
	bool pageDescriptionSuppressed_ = false;
	std::function<void(int)> cameraSelectionHandler_;
	std::function<void(const QString &, bool, int)> statusHandler_;
	bool pageNameDirty_ = false;
	int pendingSwapPage_ = -1;
	int pendingSwapSlot_ = -1;
};

class CameraControlDock final : public QWidget {
public:
	CameraControlDock(CameraRuntime &camera1, CameraRuntime &camera2, QWidget *parent = nullptr);

private:
	void selectCameraForCard(int cardIndex, int cameraIndex);
	void updateSharedGroupIndicator();
	void positionSharedGroupIndicator();
	void resizeDockToWidth(int width);
	void logLayoutDiagnostics(const char *phase, int transition, int resizeRequest) const;
	void resizeEvent(QResizeEvent *event) override;
	CameraControlCard *camera1Card_ = nullptr;
	CameraControlCard *camera2Card_ = nullptr;
	QHBoxLayout *cardsLayout_ = nullptr;
	CameraRuntime *runtimes_[2] = {nullptr, nullptr};
	int cameraAssignments_[2] = {0, 1};
	QPushButton *dualButton_ = nullptr;
	QPushButton *settingsButton_ = nullptr;
	bool settingsMode_ = false;
	int settingsWidth_ = 0;
	QWidget *cameraHeadersWidget_ = nullptr;
	QHBoxLayout *cameraHeadersLayout_ = nullptr;
	QWidget *sharedGroupToolsWidget_ = nullptr;
	QLabel *sharedGroupIndicator_ = nullptr;
	QTimer *sharedGroupMessageTimer_ = nullptr;
	QString sharedGroupMessage_;
	bool sharedGroupMessageError_ = false;
	int singleControlWidth_ = 0;
	int layoutTransitionSequence_ = 0;
	int resizeRequestSequence_ = 0;
};

class CamerasDock final : public QWidget {
public:
	CamerasDock(CameraRuntime &camera1, CameraRuntime &camera2, QWidget *parent = nullptr);

private:
	CameraRuntime *runtimes_[2];
	QComboBox *cameraSelector_ = nullptr;
	QLineEdit *cameraNameEdit_ = nullptr;
	QLineEdit *hostEdit_ = nullptr;
	QSpinBox *portSpin_ = nullptr;
	QSpinBox *addressSpin_ = nullptr;
	QSpinBox *capacitySpin_ = nullptr;
	QLabel *statusLabel_ = nullptr;
};

} // namespace simplecam
