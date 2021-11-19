#include "taskwindow.h"
#include <QFileDialog>
#include <QLayout>
#include <QPushButton>
#include "ui_taskwindow.h"
#include "model/task/taskwindowmodel.h"
#include "util/exception.h"

namespace pboman3::ui {
    TaskWindow::TaskWindow(QWidget* parent)
        : QMainWindow(parent),
          ui_(new Ui::TaskWindow),
          activeThreadCount_(0),
          log_(nullptr),
          doneText_("Done") {
        ui_->setupUi(this);
    }

    TaskWindow::~TaskWindow() {
        delete ui_;
    }

    void TaskWindow::start(const QSharedPointer<TaskWindowModel>& model) {
        if (model_)
            throw AppException("The tasks have already started");
        model_ = model;

        connect(ui_->buttonBox, &QDialogButtonBox::clicked, this, &TaskWindow::buttonClicked);

        connect(model.get(), &TaskWindowModel::threadStarted, this, &TaskWindow::threadStarted);
        connect(model.get(), &TaskWindowModel::threadThinnking, this, &TaskWindow::threadThinking);
        connect(model.get(), &TaskWindowModel::threadInitialized, this, &TaskWindow::threadInitialized);
        connect(model.get(), &TaskWindowModel::threadProgress, this, &TaskWindow::threadProgress);
        connect(model.get(), &TaskWindowModel::threadCompleted, this, &TaskWindow::threadCompleted);
        connect(model.get(), &TaskWindowModel::threadMessage, this, &TaskWindow::threadMessage);

        model_->start();
    }

    void TaskWindow::threadStarted(ThreadId threadId) {
        setMaximumHeight(QWIDGETSIZE_MAX);
        activeThreadCount_++;

        const auto progress = new ProgressWidget();
        ui_->progressHost->addWidget(progress);
        progressBars_.insert(threadId, progress);

        setMaximumHeight(height());
    }

    void TaskWindow::threadThinking(ThreadId threadId, const QString& text) const {
        const ProgressWidget* progress = progressBars_.value(threadId);
        progress->setIndeterminate(true);
        progress->setText(text);
    }

    void TaskWindow::threadInitialized(ThreadId threadId, const QString& text, qint32 minProgress,
                                       qint32 maxProgress) const {
        const ProgressWidget* progress = progressBars_.value(threadId);
        progress->setMinimum(minProgress);
        progress->setMaximum(maxProgress);
        progress->setValue(minProgress);
        progress->setText(text);
    }

    void TaskWindow::threadProgress(ThreadId threadId, qint32 progress) const {
        const ProgressWidget* progressBar = progressBars_.value(threadId);
        progressBar->setValue(progress);
    }

    void TaskWindow::threadCompleted(ThreadId threadId) {
        const ProgressWidget* progress = progressBars_.value(threadId);
        progress->setIndeterminate(false);
        progress->setText(doneText_);

        activeThreadCount_--;
        if (activeThreadCount_ == 0) {
            ui_->buttonBox->setEnabled(true);
            ui_->buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Close);
        }
    }

    void TaskWindow::threadMessage(ThreadId threadId, const QString& message) {
        if (!log_) {
            setMaximumHeight(QWIDGETSIZE_MAX);
            log_ = new QPlainTextEdit(this);
            log_->setReadOnly(true);
            log_->setMinimumHeight(100);
            log_->setLineWrapMode(QPlainTextEdit::NoWrap);
            log_->setCenterOnScroll(true);
            ui_->progressHost->addWidget(log_);
        }
        log_->appendPlainText(message);
    }

    void TaskWindow::buttonClicked(QAbstractButton* button) {
        if (button == dynamic_cast<QAbstractButton*>(ui_->buttonBox->button(QDialogButtonBox::Cancel))) {
            ui_->buttonBox->setEnabled(false);
            doneText_ = "Cancelled";
            model_->stop();
        } else if (button == dynamic_cast<QAbstractButton*>(ui_->buttonBox->button(QDialogButtonBox::Close))) {
            close();
        }
    }
}
