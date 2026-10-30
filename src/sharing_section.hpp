#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QVBoxLayout>
#include <algorithm>

namespace simplecam {

// Plain, non-interactive scene names. Scrollbars appear only when the screen
// budget cannot accommodate the content; no edit/list selection behavior.
class SharingSection final : public QWidget {
public:
	explicit SharingSection(QWidget *parent = nullptr) : QWidget(parent)
	{
		layout_ = new QVBoxLayout(this);
		layout_->setContentsMargins(0, 4, 0, 0);
		layout_->setSpacing(4);
		scroll_ = new QScrollArea(this);
		scroll_->setFrameShape(QFrame::NoFrame);
		scroll_->setWidgetResizable(true);
		scroll_->setFocusPolicy(Qt::NoFocus);
		names_ = new QLabel;
		names_->setTextFormat(Qt::PlainText);
		names_->setTextInteractionFlags(Qt::NoTextInteraction);
		names_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		names_->setContentsMargins(0, 0, 0, 0);
		scroll_->setWidget(names_);
		copy = new QPushButton(QStringLiteral("Click here to make this preset independent from these other scenes:"), this);
		copy->setAutoDefault(false);
		layout_->addWidget(copy);
		layout_->addWidget(scroll_);
		hide();
	}

	bool setSceneNames(const QStringList &names, bool shared)
	{
		if (names == sceneNames_ && shared == shared_) return false;
		sceneNames_ = names;
		shared_ = shared;
		names_->setText(names.join(QLatin1Char('\n')));
		names_->setMinimumSize(names_->sizeHint());
		scroll_->setVisible(!names.isEmpty());
		copy->setText(names.isEmpty() && shared
			? QStringLiteral("Click here to make this preset independent from its other references in this scene:")
			: QStringLiteral("Click here to make this preset independent from these other scenes:"));
		setVisible(isShared());
		return true;
	}
	bool isShared() const { return shared_; }

	int fitToBudget(int maximumHeight, int availableWidth)
	{
		if (!isShared()) return 0;
		const auto margins = layout_->contentsMargins();
		const int overhead = margins.top() + margins.bottom() + copy->sizeHint().height() +
			(sceneNames_.isEmpty() ? 0 : layout_->spacing());
		if (sceneNames_.isEmpty()) return overhead;
		const int horizontalBar = names_->sizeHint().width() > availableWidth
			? scroll_->horizontalScrollBar()->sizeHint().height() : 0;
		const int natural = names_->sizeHint().height() + horizontalBar;
		const int height = std::min(natural, std::max(names_->fontMetrics().height(), maximumHeight - overhead));
		scroll_->setFixedHeight(height);
		layout_->invalidate();
		return overhead + height;
	}

	QPushButton *copy = nullptr;
private:
	QStringList sceneNames_;
	bool shared_ = false;
	QVBoxLayout *layout_ = nullptr;
	QLabel *names_ = nullptr;
	QScrollArea *scroll_ = nullptr;
};
} // namespace simplecam
