#include "preset_source.hpp"

#include "camera_runtime.hpp"
#include "source_references.hpp"
#include "sharing_section.hpp"
#include "preset_properties_title.hpp"
#include "preset_name_allocator.hpp"

#include <plugin-support.h>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/base.h>

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QLayout>
#include <QMargins>
#include <QSizePolicy>
#include <QPointer>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QVariant>
#include <QMetaObject>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QScreen>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <memory>
#include <QString>
#include <vector>

namespace simplecam {
namespace {

constexpr char kSourceId[] = "simplecamdirect_preset";
constexpr int kProgramMode = 0;
constexpr int kPreviewMode = 1;
constexpr int kBothMode = 2;
constexpr int kAllModes = 3;

int cameraIndexFromValue(const char *value)
{
	const QString camera = QString::fromUtf8(value ? value : "1").trimmed();
	bool ok = false;
	int numeric = camera.toInt(&ok);
	if (!ok) {
		const int separator = camera.lastIndexOf(QLatin1Char(' '));
		if (separator >= 0)
			numeric = camera.mid(separator + 1).toInt(&ok);
	}
	return ok && numeric >= 1 && numeric <= CameraSlotCount ? numeric - 1 : 0;
}

std::mutex liveSourcesMutex;
std::vector<struct PresetSourceData *> liveSources;

QString makeDisplayName(const struct PresetSourceData &data);

const char *sourceText(const char *key, const char *fallback)
{
	const char *translated = obs_module_text(key);
	return translated && std::strcmp(translated, key) != 0 ? translated : fallback;
}

// OBS has no public API for a source-specific Properties size. Match only our
// source's camera-combo tooltip inside the known host Properties dialog. No
// private OBS headers/ABI, global settings or fixed-size constraints are used.
constexpr char kPropertiesMarker[] = "Select the camera for this SimpleCamPTZ preset source.";
constexpr char kCopyNumber[] = "simplecam_independent_number";

QString presetRequestBase(obs_data_t *settings);
PresetNameReservations presetNameReservations(const QString &base, obs_source_t *excluded);

using SourceRef = std::shared_ptr<obs_source_t>;
using SettingsRef = std::shared_ptr<obs_data_t>;
SourceRef ownSource(obs_source_t *source) { return SourceRef(source, obs_source_release); }

SourceRef editingScene()
{
	obs_source_t *scene = obs_frontend_preview_program_mode_active()
		? obs_frontend_get_current_preview_scene() : obs_frontend_get_current_scene();
	return ownSource(scene);
}

void completeIndependentCopy(const SourceRef &scene, const SourceRef &original, int64_t itemId,
			     const SettingsRef &settings)
{
	// Do not switch scenes or search for a replacement target after the dialog
	// closes. A deleted/replaced target cancels this operation without mutation.
	obs_source_t *registeredScene = obs_get_source_by_uuid(obs_source_get_uuid(scene.get()));
	const bool sceneStillExists = registeredScene == scene.get();
	obs_source_release(registeredScene);
	if (!sceneStillExists || obs_source_removed(original.get())) return;
	const QString base = presetRequestBase(settings.get());
	int number;
	{
		std::lock_guard<std::mutex> lock(liveSourcesMutex);
		// The original identity continues to occupy its number after detaching.
		number = presetNameReservations(base, nullptr).lowestUnused(true);
	}
	if (number < 0) return;
	const QString name = QStringLiteral("%1 (%2)").arg(base).arg(number);
	obs_data_set_int(settings.get(), kCopyNumber, number);
	const QByteArray utf8 = name.toUtf8();
	obs_source_t *copy = replaceWithIndependentSource(obs_scene_from_source(scene.get()), itemId,
		original.get(), settings.get(), utf8.constData());
	if (!copy) {
		QMessageBox::warning(nullptr, QStringLiteral("SimpleCamPTZ"),
			QStringLiteral("The original scene item changed or the copy could not be created. No reference was removed."));
		return;
	}
	refreshPresetSourceNames();
	obs_frontend_save();
	obs_log(LOG_INFO, "[SimpleCamPTZ IndependentSource] replaced item in scene=%s new_source=%s",
		obs_source_get_name(scene.get()), obs_source_get_name(copy));
	obs_frontend_open_source_properties(copy);
	obs_source_release(copy);
}

class CompactPresetProperties final : public QObject {
public:
	explicit CompactPresetProperties(QObject *parent) : QObject(parent) {}
	~CompactPresetProperties() override
	{
		// Host Properties dialogs may outlive plugin UI shutdown. Remove our
		// timers/connections while this module's code is still loaded.
		for (const auto &widget : sharingWidgets_) delete widget.data();
	}
protected:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		if (event->type() == QEvent::WindowTitleChange && watched->inherits("OBSBasicProperties")) {
			if (auto *dialog = qobject_cast<QDialog *>(watched))
				if (dialog->property("simplecamptzPlainPropertiesTitle").toBool()) cleanTitle(dialog);
			return false;
		}
		if (event->type() != QEvent::Show || !watched->inherits("OBSBasicProperties"))
			return false;
		auto *dialog = qobject_cast<QDialog *>(watched);
		if (!dialog || dialog->property("simplecamptzPropertiesProbeStarted").toBool())
			return false;
		dialog->setProperty("simplecamptzPropertiesProbeStarted", true);
		// Capture editor context at Show time, before another scene can be selected.
		const SourceRef scene = editingScene();
		QPointer<QDialog> guard(dialog);
		QTimer::singleShot(0, this, [this, guard, scene]() { probe(guard, 0, scene); });
		return false;
	}
private:
	bool cleaningTitle_ = false;
	void cleanTitle(QDialog *dialog)
	{
		if (cleaningTitle_) return;
		const QString plain = plainPresetPropertiesTitle(dialog->windowTitle());
		if (plain == dialog->windowTitle()) return;
		cleaningTitle_ = true;
		dialog->setWindowTitle(plain);
		cleaningTitle_ = false;
	}
	std::vector<QPointer<QWidget>> sharingWidgets_;
	void probe(QPointer<QDialog> guard, int attempt, const SourceRef &scene)
	{
		if (!guard || !guard->isVisible()) return;
		QString sourceUuid;
		for (QComboBox *combo : guard->findChildren<QComboBox *>())
			if (combo->toolTip().startsWith(QString::fromUtf8(kPropertiesMarker) + QLatin1Char('\n')))
				sourceUuid = combo->toolTip().section(QLatin1Char('\n'), 1, 1);
		auto *container = guard->findChild<QWidget *>(QStringLiteral("PropertiesContainer"));
		auto *splitter = guard->findChild<QSplitter *>(QStringLiteral("windowSplitter"));
		if (sourceUuid.isEmpty() || !container || !container->layout() || !splitter) {
			if (attempt < 19)
				QTimer::singleShot(25, this, [this, guard, attempt, scene]() { probe(guard, attempt + 1, scene); });
			return;
		}
		SourceRef original = ownSource(obs_get_source_by_uuid(sourceUuid.toUtf8().constData()));
		if (!original || std::strcmp(obs_source_get_id(original.get()), kSourceId) != 0) return;
		guard->setProperty("simplecamptzPlainPropertiesTitle", true);
		cleanTitle(guard.data());
		addSharingControls(guard, container, scene, original);
	}

	void fitProperties(QPointer<QDialog> guard, QPointer<QWidget> container,
			   QPointer<SharingSection> sharing)
	{
		if (!guard || !container || !sharing || !guard->isVisible()) return;
		auto *splitter = guard->findChild<QSplitter *>(QStringLiteral("windowSplitter"));
		if (!splitter || !container->layout()) return;
		QScreen *screen = guard->screen();
		// Set the requested opening width once. Later scene-list updates must
		// not undo an operator's manual resize, and unshared dialogs keep the
		// width supplied by OBS.
		if (!guard->property("simplecamptzInitialWidthApplied").toBool()) {
			guard->setProperty("simplecamptzInitialWidthApplied", true);
			if (sharing->isShared()) {
				const int frameWidth = std::max(0, guard->frameGeometry().width() - guard->width());
				const int widthLimit = screen ? std::max(1, screen->availableGeometry().width() - frameWidth - 24) : 610;
				guard->resize(std::min(610, widthLimit), guard->height());
				if (guard->layout()) guard->layout()->activate();
			}
		}
		splitter->setMinimumHeight(0);
		if (QWidget *preview = guard->findChild<QWidget *>(QStringLiteral("previewFrame"))) preview->hide();
		// Measure the established compact form without the optional section.
		sharing->hide();
		container->layout()->invalidate();
		const int baseFormHeight = container->layout()->sizeHint().height();
		auto *buttons = guard->findChild<QDialogButtonBox *>();
		const int footer = buttons ? buttons->sizeHint().height() : 36;
		const int baseHeight = std::max(280, baseFormHeight + footer + 56);
		const int frameHeight = std::max(0, guard->frameGeometry().height() - guard->height());
		const int screenLimit = screen ? std::max(1, screen->availableGeometry().height() - frameHeight - 24) : 900;
		sharing->setVisible(sharing->isShared());
		const int sectionHeight = sharing->fitToBudget(std::max(0, screenLimit - baseHeight - 12),
			std::max(1, container->contentsRect().width()));
		container->layout()->invalidate();
		const int formHeight = container->layout()->sizeHint().height();
		for (QWidget *parent = container->parentWidget(); parent && parent != guard.data(); parent = parent->parentWidget()) {
			if (auto *view = qobject_cast<QScrollArea *>(parent)) {
				// Let the existing outer form scroll on a very short display.
				view->setMinimumHeight(std::max(0, std::min(formHeight + 12, screenLimit - footer - 56)));
				break;
			}
		}
		if (guard->layout()) {
			guard->layout()->invalidate();
			guard->layout()->activate();
		}
		const int target = std::min(screenLimit, std::max(guard->minimumSizeHint().height(),
			baseHeight + sectionHeight + (sharing->isShared() ? 12 : 0)));
		guard->resize(guard->width(), target);
		if (screen) {
			const QRect available = screen->availableGeometry().adjusted(12, 12, -12, -12);
			const QRect frame = guard->frameGeometry();
			const int left = std::clamp(frame.left(), available.left(),
				std::max(available.left(), available.right() - frame.width() + 1));
			const int top = std::clamp(frame.top(), available.top(),
				std::max(available.top(), available.bottom() - frame.height() + 1));
			if (left != frame.left() || top != frame.top())
				guard->move(guard->pos() + QPoint(left - frame.left(), top - frame.top()));
		}
	}

	void addSharingControls(QPointer<QDialog> dialog, QWidget *container,
				const SourceRef &scene, const SourceRef &original)
	{
		auto *group = new SharingSection(container);
		sharingWidgets_.erase(std::remove_if(sharingWidgets_.begin(), sharingWidgets_.end(),
			[](const QPointer<QWidget> &widget) { return widget.isNull(); }), sharingWidgets_.end());
		sharingWidgets_.push_back(group);
		auto *copy = group->copy;
		container->layout()->addWidget(group);
		obs_sceneitem_t *target = independentCopyTarget(scene ? obs_scene_from_source(scene.get()) : nullptr, original.get());
		const int64_t itemId = target ? obs_sceneitem_get_id(target) : -1;
		QPointer<SharingSection> sharing(group);
		QPointer<QWidget> containerGuard(container);
		auto refresh = [this, dialog, containerGuard, sharing, copy, scene, original, itemId]() {
			QStringList paths;
			const auto references = sourceReferencePaths(original.get());
			const std::string editingSceneUuid = scene ? obs_source_get_uuid(scene.get()) : "";
			for (const auto &reference : references) {
				if (reference.rootSceneUuid == editingSceneUuid) continue;
				QString path = QString::fromUtf8(reference.path.c_str());
				if (reference.indirect) path += QStringLiteral(" (via nested scene/group)");
				paths.push_back(path);
			}
			paths.sort(Qt::CaseInsensitive);
			paths.removeDuplicates();
			if (sharing->setSceneNames(paths, references.size() > 1))
				QTimer::singleShot(0, this, [this, dialog, containerGuard, sharing]() {
					fitProperties(dialog, containerGuard, sharing);
				});
			obs_sceneitem_t *item = scene ? obs_scene_find_sceneitem_by_id(obs_scene_from_source(scene.get()), itemId) : nullptr;
			const bool valid = item && obs_sceneitem_get_source(item) == original.get() &&
				sourceReferencePaths(scene.get()).empty();
			copy->setEnabled(valid);
			copy->setToolTip(valid
				? QStringLiteral("Replace only this scene's reference, then edit the independent copy. Other references keep their original settings.")
				: QStringLiteral("Select one direct preset item in a scene that is not itself reused inside other scenes. Detach shared containing scenes first."));
		};
		refresh();
		// Wait for OBS and Qt to finish constructing/polishing the form before
		// its initial measurement. Unchanged polling never overrides user size.
		QTimer::singleShot(0, this, [this, dialog, containerGuard, sharing]() {
			fitProperties(dialog, containerGuard, sharing);
		});
		auto *timer = new QTimer(group);
		QObject::connect(timer, &QTimer::timeout, group, refresh);
		timer->start(1000);
		QObject::connect(copy, &QPushButton::clicked, this, [this, dialog, scene, original, itemId]() {
			if (!dialog || !scene || itemId < 0) return;
			auto *box = dialog->findChild<QDialogButtonBox *>();
			auto *cancel = box ? box->button(QDialogButtonBox::Cancel) : nullptr;
			if (!cancel) return;
			// Snapshot the visible edits, then use OBS's real Cancel path to
			// restore the shared source. No private OBS ABI or direct done().
			SettingsRef settings(obs_data_create(), obs_data_release);
			obs_data_t *current = obs_source_get_settings(original.get());
			obs_data_apply(settings.get(), current);
			obs_data_release(current);
			QObject::connect(dialog, &QDialog::finished, this,
				[this, scene, original, itemId, settings](int) {
					QTimer::singleShot(0, this, [scene, original, itemId, settings]() {
						completeIndependentCopy(scene, original, itemId, settings);
					});
				});
			cancel->click();
		});
	}
};
QPointer<CompactPresetProperties> propertiesFilter;

size_t countSceneReferences(obs_source_t *target)
{
	return sourceReferencePaths(target).size();
}

struct PresetSourceData {
	obs_source_t *source = nullptr;
	bool initializing = false;
	int logicalPreset = 1;
	int triggerMode = kProgramMode;
	int cameraIndex = 0;
	int independentNumber = 0;
	bool newIdentity = true;
	bool initialNamePending = true;
	QString cameraName;
	QString presetLabel;
};

void refreshSourceName(PresetSourceData *data, bool referencedMoreThanOnce = false, bool allocateIdentity = false)
{
	if (!data || !data->source)
		return;
	if (CameraRuntime *runtime = cameraRuntime(data->cameraIndex)) {
		data->cameraName = runtime->cameraName();
		data->presetLabel = runtime->presets().presetLabel(runtime->activePage(), data->logicalPreset - 1);
	}
	QString display = makeDisplayName(*data);
	// Loaded sources retain their identity. A new unnumbered source may still
	// need a suffix after the operator selects its camera/preset in Properties.
	if (allocateIdentity && data->newIdentity && (data->initialNamePending || data->independentNumber == 0)) {
		const auto reserved = presetNameReservations(display, data->source);
		if (reserved.occupied(data->independentNumber)) {
			const int number = reserved.lowestUnused(true);
			if (number < 0) return;
			data->independentNumber = number;
			obs_data_t *settings = obs_source_get_settings(data->source);
			obs_data_set_int(settings, kCopyNumber, number);
			obs_data_release(settings);
		}
	}
	if (allocateIdentity) data->initialNamePending = false;
	if (data->independentNumber > 0)
		display += QStringLiteral(" (%1)").arg(data->independentNumber);
	if (referencedMoreThanOnce)
		display = QStringLiteral("<span style=\"color:#FF6050;\">%1 *</span>").arg(display.toHtmlEscaped());
	const QByteArray displayName = display.toUtf8();
	const char *currentName = obs_source_get_name(data->source);
	if (!currentName || display != QString::fromUtf8(currentName))
		obs_source_set_name(data->source, displayName.constData());
}

struct DeferredSourceRename {
	obs_source_t *source = nullptr;
	PresetSourceData *data = nullptr;
};

void scheduleSourceNameRefreshAfterCreate(PresetSourceData *data)
{
	auto *rename = new DeferredSourceRename{obs_source_get_ref(data->source), data};
	auto *application = QCoreApplication::instance();
	if (!application) {
		obs_source_release(rename->source);
		delete rename;
		return;
	}

	// OBS's OBS_TASK_UI handler uses Qt::AutoConnection, which executes
	// immediately when called from OBS's UI thread. Force a queued Qt event so
	// the source-creation callback and OBS name registration have both finished.
	const bool queued = QMetaObject::invokeMethod(
		application,
		[rename]() {
			{
				std::lock_guard<std::mutex> lock(liveSourcesMutex);
				refreshSourceName(rename->data, countSceneReferences(rename->source) > 1, true);
			}
			obs_source_release(rename->source);
			delete rename;
		},
		Qt::QueuedConnection);
	if (!queued) {
		obs_source_release(rename->source);
		delete rename;
	}
}

void sourceUpdate(void *opaque, obs_data_t *settings);

void *sourceCreate(obs_data_t *settings, obs_source_t *source)
{
	auto *data = new PresetSourceData;
	data->source = source;
	data->initializing = true;
	sourceUpdate(data, settings);
	data->initializing = false;
	{
		std::lock_guard<std::mutex> lock(liveSourcesMutex);
		liveSources.push_back(data);
	}
	// OBS invokes create() before the source has been inserted into its name
	// registry. Renaming here can crash in obs_source_set_name.
	scheduleSourceNameRefreshAfterCreate(data);
	return data;
}

QString makeDisplayName(const PresetSourceData &data)
{
	const QString camera = data.cameraName.trimmed().isEmpty() ? QStringLiteral("Camera 1") : data.cameraName;
	const QString preset = data.presetLabel.trimmed().isEmpty()
			       ? QStringLiteral("Preset %1").arg(data.logicalPreset)
			       : data.presetLabel;
	return QStringLiteral("Move %1 to %2").arg(camera, preset);
}

QString presetRequestBase(obs_data_t *settings)
{
	PresetSourceData data;
	data.cameraIndex = cameraIndexFromValue(obs_data_get_string(settings, "camera"));
	data.logicalPreset = static_cast<int>(std::clamp<std::int64_t>(obs_data_get_int(settings, "preset"), 1, 14));
	if (CameraRuntime *runtime = cameraRuntime(data.cameraIndex)) {
		data.cameraName = runtime->cameraName();
		data.presetLabel = runtime->presets().presetLabel(runtime->activePage(), data.logicalPreset - 1);
	} else {
		data.cameraName = QStringLiteral("Camera %1").arg(data.cameraIndex + 1);
	}
	return makeDisplayName(data);
}

// Caller holds liveSourcesMutex. One live entry is one source identity,
// irrespective of how many scene references use it. Reserve pending sources
// before their queued rename, plus actual names (including legacy suffixes).
PresetNameReservations presetNameReservations(const QString &base, obs_source_t *excluded)
{
	PresetNameReservations reserved(base.toStdString());
	for (auto *data : liveSources) {
		if (data->source == excluded || obs_source_removed(data->source)) continue;
		reserved.reserveIdentity(makeDisplayName(*data).toStdString(), data->independentNumber);
		reserved.reserveName(obs_source_get_name(data->source));
	}
	struct Scan { PresetNameReservations *reserved; obs_source_t *excluded; } scan{&reserved, excluded};
	obs_enum_sources([](void *opaque, obs_source_t *source) {
		auto &scan = *static_cast<Scan *>(opaque);
		if (source != scan.excluded && !obs_source_removed(source))
			scan.reserved->reserveName(obs_source_get_name(source));
		return true;
	}, &scan);
	return reserved;
}

void sourceLoad(void *opaque, obs_data_t *settings)
{
	std::lock_guard<std::mutex> lock(liveSourcesMutex);
	auto *data = static_cast<PresetSourceData *>(opaque);
	data->newIdentity = false;
	// Preserve legacy OBS-assigned numbers, including suffixes outside orange
	// markup, rather than losing them during automatic source-name refresh.
	if (data->independentNumber == 0) {
		const std::string name = plainPresetSourceName(obs_source_get_name(data->source));
		const size_t suffix = name.rfind(" (");
		const int number = suffix == std::string::npos ? -1 : presetSourceNumber(name.substr(0, suffix), name);
		if (number > 0) {
			data->independentNumber = number;
			obs_data_set_int(settings, kCopyNumber, number);
		}
	}
}

void sourceUpdate(void *opaque, obs_data_t *settings)
{
	auto *data = static_cast<PresetSourceData *>(opaque);
	if (!data)
		return;
	std::lock_guard<std::mutex> lock(liveSourcesMutex);
	data->logicalPreset = static_cast<int>(std::clamp<std::int64_t>(obs_data_get_int(settings, "preset"), 1, 14));
	data->independentNumber = static_cast<int>(std::clamp<std::int64_t>(obs_data_get_int(settings, kCopyNumber), 0, 99999));
	data->triggerMode = static_cast<int>(std::clamp<std::int64_t>(obs_data_get_int(settings, "trigger_mode"), 0, 2));
	const char *camera = obs_data_get_string(settings, "camera");
	data->cameraIndex = cameraIndexFromValue(camera);
	if (CameraRuntime *runtime = cameraRuntime(data->cameraIndex)) {
		data->cameraName = runtime->cameraName();
		const int page = runtime->activePage();
		data->presetLabel = runtime->presets().presetLabel(page, data->logicalPreset - 1);
	} else {
		data->cameraName = QStringLiteral("Camera %1").arg(data->cameraIndex + 1);
		data->presetLabel.clear();
	}
	if (!data->initializing)
		refreshSourceName(data, countSceneReferences(data->source) > 1, true);
}

void sourceDestroy(void *opaque)
{
	auto *data = static_cast<PresetSourceData *>(opaque);
	{
		std::lock_guard<std::mutex> lock(liveSourcesMutex);
		const auto found = std::find(liveSources.begin(), liveSources.end(), data);
		if (found != liveSources.end())
			liveSources.erase(found);
	}
	delete data;
}

const char *sourceName(void *)
{
	return sourceText("SimpleCamPTZ.PresetSource", "SimpleCam Preset");
}

void sourceDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "camera", "Camera 1");
	obs_data_set_default_int(settings, "preset", 1);
	obs_data_set_default_int(settings, "trigger_mode", kProgramMode);
	obs_data_set_default_int(settings, "delay_seconds", 0);
}

void populatePresetList(obs_property_t *preset, int cameraIndex)
{
	obs_property_list_clear(preset);
	const CameraRuntime *runtime = cameraRuntime(cameraIndex);
	const int activePage = runtime ? runtime->activePage() : 0;
	for (int i = 0; i < PresetCatalog::SlotsPerPage; ++i) {
		const bool usable = !runtime || runtime->hasUsablePreset(activePage, i);
		QString label = runtime ? runtime->presets().presetLabel(activePage, i) : QString();
		if (!usable)
			label = QStringLiteral("---");
		else if (label.trimmed().isEmpty())
			label = QStringLiteral("Preset %1").arg(i + 1);
		const QByteArray utf8 = label.toUtf8();
		const size_t item = obs_property_list_add_int(preset, utf8.constData(), i + 1);
		if (!usable)
			obs_property_list_item_disable(preset, item, true);
	}
}

bool cameraSelectionChanged(obs_properties_t *properties, obs_property_t *, obs_data_t *settings)
{
	const int cameraIndex = cameraIndexFromValue(obs_data_get_string(settings, "camera"));
	populatePresetList(obs_properties_get(properties, "preset"), cameraIndex);
	return true;
}

obs_properties_t *sourceProperties(void *opaque)
{
	const auto *sourceData = static_cast<const PresetSourceData *>(opaque);
	const int currentCamera = sourceData ? sourceData->cameraIndex : 0;
	auto *properties = obs_properties_create();
	auto *camera = obs_properties_add_list(properties, "camera", sourceText("SimpleCamPTZ.Camera", "Camera"),
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	for (int index = 0; index < CameraSlotCount; ++index) {
		const CameraRuntime *runtime = cameraRuntime(index);
		const QByteArray label = (runtime ? runtime->cameraName() + (runtime->isEnabled() ? QString() : QStringLiteral(" (disabled)")) : QStringLiteral("Camera %1").arg(index + 1)).toUtf8();
		const QByteArray value = QStringLiteral("Camera %1").arg(index + 1).toUtf8();
		obs_property_list_add_string(camera, label.constData(), value.constData());
	}
	obs_property_set_modified_callback(camera, cameraSelectionChanged);
	const QByteArray marker = (QString::fromUtf8(kPropertiesMarker) + QLatin1Char('\n') +
		QString::fromUtf8(sourceData ? obs_source_get_uuid(sourceData->source) : "")).toUtf8();
	obs_property_set_long_description(camera, marker.constData());

	auto *preset = obs_properties_add_list(properties, "preset", sourceText("SimpleCamPTZ.Preset", "Preset"),
						OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	populatePresetList(preset, currentCamera);

	auto *mode = obs_properties_add_list(properties, "trigger_mode", sourceText("SimpleCamPTZ.Trigger", "Trigger"),
						OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mode, sourceText("SimpleCamPTZ.Program", "Program"), kProgramMode);
	obs_property_list_add_int(mode, sourceText("SimpleCamPTZ.Preview", "Preview"), kPreviewMode);
	obs_property_list_add_int(mode, sourceText("SimpleCamPTZ.Both", "Both"), kBothMode);
	obs_properties_add_int_slider(properties, "delay_seconds", sourceText("SimpleCamPTZ.Delay", "Delay (seconds)"), 0, 600, 1);
	return properties;
}

struct DispatchContext {
	int triggerMode;
};

bool dispatchSceneItem(obs_scene_t *scene, obs_sceneitem_t *item, void *opaque)
{
	auto *context = static_cast<DispatchContext *>(opaque);
	if (!obs_sceneitem_visible(item))
		return true;
	if (obs_sceneitem_is_group(item)) {
		if (obs_scene_t *groupScene = obs_sceneitem_group_get_scene(item))
			obs_scene_enum_items(groupScene, dispatchSceneItem, context);
		return true;
	}

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source || std::strcmp(obs_source_get_id(source), kSourceId) != 0)
		return true;
	obs_data_t *settings = obs_source_get_settings(source);
	const int mode = static_cast<int>(obs_data_get_int(settings, "trigger_mode"));
	const bool matches = context->triggerMode == kAllModes || mode == kBothMode || mode == context->triggerMode;
	const int preset = static_cast<int>(obs_data_get_int(settings, "preset"));
	const int delay = static_cast<int>(obs_data_get_int(settings, "delay_seconds")) * 1000;
	const int cameraIndex = cameraIndexFromValue(obs_data_get_string(settings, "camera"));
	obs_data_release(settings);
	if (matches) {
		if (CameraRuntime *runtime = cameraRuntime(cameraIndex)) {
			if (cameraIndex == 0 && runtime->activePage() == 0 && preset == 1) {
				obs_source_t *sceneSource = obs_scene_get_source(scene);
				const char *sceneName = sceneSource ? obs_source_get_name(sceneSource) : "<unknown>";
				const char *presetSourceName = obs_source_get_name(source);
				obs_log(LOG_INFO,
					"[SimpleCamPTZ PresetFadeDiag] stage=obs-source-request camera=1 group=1 button=1 "
					"scene=\"%s\" source=\"%s\" delay_ms=%d trigger_mode=%d",
					sceneName ? sceneName : "<unknown>", presetSourceName ? presetSourceName : "<unknown>", delay, mode);
			}
			runtime->recallLogicalPreset(preset, delay, [preset](bool ok, const QString &message) {
				if (!ok)
					obs_log(LOG_WARNING, "SimpleCamPTZ preset %d failed: %s", preset, message.toUtf8().constData());
			}, CameraRuntime::PresetRecallOrigin::ObsSource);
		}
	}
	return true;
}

} // namespace

void registerPresetSource()
{
	if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance())) {
		propertiesFilter = new CompactPresetProperties(app);
		app->installEventFilter(propertiesFilter.data());
	}
	static obs_source_info info{};
	info.id = kSourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = 0;
	info.get_name = sourceName;
	info.create = sourceCreate;
	info.destroy = sourceDestroy;
	info.update = sourceUpdate;
	info.load = sourceLoad;
	info.get_defaults = sourceDefaults;
	info.get_properties = sourceProperties;
	obs_register_source(&info);
}

void shutdownPresetSourceUi()
{
	// Cancel queued dialog work before unloading this module's code.
	delete propertiesFilter.data();
	propertiesFilter = nullptr;
}

void dispatchScenePresets(obs_source_t *sceneSource, int triggerMode)
{
	if (!sceneSource)
		return;
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	if (!scene)
		return;
	DispatchContext context{triggerMode};
	obs_scene_enum_items(scene, dispatchSceneItem, &context);
}

void refreshPresetSourceNames()
{
	std::vector<std::pair<PresetSourceData *, bool>> refreshList;
	std::lock_guard<std::mutex> lock(liveSourcesMutex);
	refreshList.reserve(liveSources.size());
	for (auto *data : liveSources)
		refreshList.emplace_back(data, countSceneReferences(data->source) > 1);
	for (const auto &entry : refreshList)
		refreshSourceName(entry.first, entry.second);
}

} // namespace simplecam
