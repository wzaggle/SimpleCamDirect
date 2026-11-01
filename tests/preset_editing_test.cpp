#include "camera_docks.hpp"
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <cstdlib>

// This isolated card test does not create OBS scenes/sources. Source dispatch
// is covered by the shared runtime recall tests using ObsSource origin.
namespace simplecam {
void refreshPresetSourceNames() {}
// The full dock's OBS callbacks are not exercised by this isolated card test.
// MSVC retains their references even when unused sections are discarded.
obs_source_t *currentProgramSceneSource(bool) { return nullptr; }
std::uint64_t currentProgramTransitionGeneration() { return 0; }
class PresetEditingTest {
    static inline int checks=0;
    static void check(bool ok,const char *label) {
        ++checks; if(!ok) { std::fprintf(stderr,"FAIL: %s\n",label); std::exit(1); }
    }
public:
    static int run(const QString &profile) {
        QTemporaryDir dir;
        CameraRuntime runtime(dir.filePath("slot.ini"),QStringLiteral("Camera"),profile);
        runtime.presets().clearPageLabels(0);
        setCameraRuntime(0,&runtime);
        CameraControlCard card(runtime,1);
        card.show(); QCoreApplication::processEvents();
        check(!card.presetButtons_[0]->isEnabled(),"blank mapped button disabled in Recall mode");
        card.renameButton_->click();
        check(card.presetButtons_[0]->isEnabled() && card.presetButtons_[1]->isEnabled(),"blank buttons enabled for Rename");
        card.presetButtons_[0]->click();
        card.presetNameEditor_->setText(QStringLiteral("First new label"));
        card.presetButtons_[1]->click();
        check(runtime.presets().presetLabel(0,0)==QStringLiteral("First new label"),"clicking next saves previous edit without Enter");
        check(card.mode_==CameraControlCard::Mode::Rename && card.renameSlot_==1 && card.presetButtons_[2]->isEnabled(),"Rename mode remains active on next blank button");
        card.presetNameEditor_->setText(QStringLiteral("Second new label"));
        card.presetButtons_[2]->click();
        check(runtime.presets().presetLabel(0,1)==QStringLiteral("Second new label"),"multiple successive renames persist");
        card.presetNameEditor_->setText(QStringLiteral("Third new label"));
        QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
        QCoreApplication::sendEvent(card.presetNameEditor_,&enter);
        check(runtime.presets().presetLabel(0,2)==QStringLiteral("Third new label") && card.mode_==CameraControlCard::Mode::Recall,"Enter commits and exits Rename mode");
        check(card.presetButtons_[0]->isEnabled() && !card.presetButtons_[3]->isEnabled(),"only named presets enabled after Rename exit");
        card.renameButton_->click(); card.presetButtons_[0]->click();
        card.presetNameEditor_->setText(QStringLiteral("Discard with Escape"));
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
        QCoreApplication::sendEvent(card.presetNameEditor_,&escape);
        check(runtime.presets().presetLabel(0,0)==QStringLiteral("First new label") && card.mode_==CameraControlCard::Mode::Recall,"Escape preserves prior saved label");
        card.renameButton_->click(); card.presetButtons_[0]->click(); card.presetNameEditor_->setText(QStringLiteral("   "));
        card.presetButtons_[1]->click();
        check(runtime.presets().presetLabel(0,0).isEmpty() && !runtime.canRecallPreset(0,0),"blanking label while advancing immediately blocks recall");
        card.presetNameEditor_->setText(QStringLiteral("Keep second"));
        QCoreApplication::sendEvent(card.presetNameEditor_,&enter);
        check(!card.presetButtons_[0]->isEnabled() && card.presetButtons_[1]->isEnabled(),"blank label stays disabled after commit");
        card.setButton_->click();
        check(card.mode_==CameraControlCard::Mode::Set && card.presetButtons_[0]->isEnabled(),"Set mode still permits blank preset button");
        card.setMode(CameraControlCard::Mode::Recall);
        runtime.presets().setLabel(0,0,QStringLiteral("Will clear")); card.refreshView();
        card.setSettingsMode(true); card.clearButton_->click(); card.presetButtons_[0]->click();
        check(runtime.presets().presetLabel(0,0).isEmpty() && card.mode_==CameraControlCard::Mode::Recall && !card.presetButtons_[0]->isEnabled(),"Clear immediately disables normal recall");
        card.setSettingsMode(false); card.renameButton_->click(); card.presetButtons_[0]->click();
        card.presetNameEditor_->setText(QStringLiteral("Re-enabled")); QCoreApplication::sendEvent(card.presetNameEditor_,&enter);
        check(card.presetButtons_[0]->isEnabled() && runtime.canRecallPreset(0,0),"naming a blank button re-enables recall");
        // Finish any animation before card teardown; no camera IO was invoked.
        card.hide(); setCameraRuntime(0,nullptr);
        std::printf("Preset editing: %d checks passed\n",checks);
        return 0;
    }
};
}
int main(int argc,char **argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    if(argc!=2) return 2;
    return simplecam::PresetEditingTest::run(QString::fromLocal8Bit(argv[1]));
}
