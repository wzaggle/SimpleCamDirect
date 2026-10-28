#include "sharing_section.hpp"
#include <QApplication>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <iostream>
int main(int argc, char **argv) {
 QApplication app(argc, argv);
 int count=0, failures=0;
 auto check=[&](bool ok,const char *label){++count;if(!ok){++failures;std::cerr<<"FAIL: "<<label<<'\n';}};
 QWidget host; auto *layout=new QVBoxLayout(&host);
 auto *section=new simplecam::SharingSection(&host);layout->addWidget(section);
 host.resize(520,600);host.show();app.processEvents();
 section->setSceneNames({"Scene 1"});app.processEvents();
 check(section->isHidden(),"one reference hides entire section");
 check(section->fitToBudget(500,480)==0,"unshared adds no height");
 section->setSceneNames({"Scene 1","Scene 2"});
 int two=section->fitToBudget(500,480);app.processEvents();
 check(!section->isHidden(),"two references show section");
 check(section->findChildren<QPlainTextEdit*>().empty() && section->findChildren<QTextEdit*>().empty(),"no edit controls");
 auto *scroll=section->findChild<QScrollArea*>();
 auto *names=qobject_cast<QLabel*>(scroll->widget());
 check(scroll->frameShape()==QFrame::NoFrame,"borderless scene area");
 check(names->textInteractionFlags()==Qt::NoTextInteraction,"scene names not selectable");
 check(names->text()=="Scene 1\nScene 2","one name per line");
 section->setSceneNames({"Scene 1","Scene 2","Scene 3","Scene 4"});
 int four=section->fitToBudget(500,480);
 check(four>two,"height increases with scene count");
 QStringList many;for(int i=0;i<100;i++)many<<QString("Scene %1").arg(i);
 section->setSceneNames(many);int capped=section->fitToBudget(220,480);
 host.resize(520,240);app.processEvents();
 check(capped<=220,"section capped at supplied screen budget");
 check(scroll->verticalScrollBar()->maximum()>0,"long list can scroll");
 section->setSceneNames({"Only scene"});app.processEvents();
 check(section->isHidden(),"collapses after source becomes unshared");
 check(!section->setSceneNames({"Only scene"}),"unchanged polling does not refit dialog");
 std::cout<<count<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
