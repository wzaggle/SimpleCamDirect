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
		heading_ = new QLabel(QStringLiteral("Used in scenes — changes affect every reference listed"), this);
		heading_->setTextFormat(Qt::PlainText);
		heading_->setWordWrap(true);
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
		copy = new QPushButton(QStringLiteral("Make independent copy"), this);
		copy->setAutoDefault(false);
		layout_->addWidget(heading_);
		layout_->addWidget(scroll_);
		layout_->addWidget(copy);
		hide();
	}

	bool setSceneNames(const QStringList &names)
	{
		if (names == sceneNames_) return false;
		sceneNames_ = names;
		names_->setText(names.join(QLatin1Char('\n')));
		names_->setMinimumSize(names_->sizeHint());
		setVisible(isShared());
		return true;
	}
	bool isShared() const { return sceneNames_.size() > 1; }

	int fitToBudget(int maximumHeight, int availableWidth)
	{
		if (!isShared()) return 0;
		const auto margins = layout_->contentsMargins();
		const int headingHeight = heading_->heightForWidth(std::max(1, availableWidth));
		const int overhead = margins.top() + margins.bottom() + 2 * layout_->spacing() +
			std::max(heading_->sizeHint().height(), headingHeight) + copy->sizeHint().height();
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
	QVBoxLayout *layout_ = nullptr;
	QLabel *heading_ = nullptr;
	QLabel *names_ = nullptr;
	QScrollArea *scroll_ = nullptr;
};
} // namespace simplecam
