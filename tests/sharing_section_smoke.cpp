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
 section->setSceneNames({},false);app.processEvents();
 check(section->isHidden(),"unshared source hides entire section");
 check(section->fitToBudget(500,480)==0,"unshared adds no height");
 section->setSceneNames({"Other scene"},true);app.processEvents();
 check(!section->isHidden(),"one other scene still shows shared section");
 section->setSceneNames({"Scene 1","Scene 2"},true);
 int two=section->fitToBudget(500,480);app.processEvents();
 check(!section->isHidden(),"two references show section");
 check(section->findChildren<QPlainTextEdit*>().empty() && section->findChildren<QTextEdit*>().empty(),"no edit controls");
 auto *scroll=section->findChild<QScrollArea*>();
 check(section->copy->mapTo(section,QPoint(0,0)).y()<scroll->mapTo(section,QPoint(0,0)).y(),"independent copy button above scene names");
 auto *names=qobject_cast<QLabel*>(scroll->widget());
 check(section->findChildren<QLabel*>().size()==1,"separate heading removed");
 check(section->copy->text()=="Click here to make this preset independent from these other scenes:","button is the list heading");
 check(scroll->frameShape()==QFrame::NoFrame,"borderless scene area");
 check(names->textInteractionFlags()==Qt::NoTextInteraction,"scene names not selectable");
 check(names->text()=="Scene 1\nScene 2","one name per line");
 section->setSceneNames({"Scene 1","Scene 2","Scene 3","Scene 4"},true);
 int four=section->fitToBudget(500,480);
 check(four>two,"height increases with scene count");
 QStringList many;for(int i=0;i<100;i++)many<<QString("Scene %1").arg(i);
 section->setSceneNames(many,true);int capped=section->fitToBudget(220,480);
 host.resize(520,240);app.processEvents();
 check(capped<=220,"section capped at supplied screen budget");
 check(scroll->verticalScrollBar()->maximum()>0,"long list can scroll");
 section->setSceneNames({},true);app.processEvents();
 check(!section->isHidden() && scroll->isHidden(),"same-scene-only shared reference retains button without blank list");
 section->setSceneNames({},false);app.processEvents();
 check(section->isHidden(),"collapses after source becomes unshared");
 check(!section->setSceneNames({},false),"unchanged polling does not refit dialog");
 std::cout<<count<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
