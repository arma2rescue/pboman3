#include <QApplication>
#include <QFile>
#include <QTimer>
#include <Windows.h>
#include "ui/errordialog.h"
#include "ui/mainwindow.h"
#include "util/exception.h"
#include "util/log.h"
#include <QScopedPointer>
#include "model/pbomodel.h"
#include <CLI/CLI.hpp>

#define LOG(...) LOGGER("Main", __VA_ARGS__)

namespace pboman3 {
    class PboApplication : public QApplication {
    public:
        PboApplication(int& argc, char** argv)
            : QApplication(argc, argv) {
        }

        bool notify(QObject* o, QEvent* e) override {
            try {
                return QApplication::notify(o, e);
            } catch (const AppException& ex) {
                LOG(warning, "The developers forgot to handle the exception in place:", ex)
                return true;
            }
        }
    };

    void ActivateCom(const QApplication& a) {
        auto* timer = new QTimer(new QTimer());
        timer->moveToThread(a.thread());
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, [timer]() {
            LOG(info, "Initializing COM")

            const HRESULT hr = CoInitialize(nullptr);

            LOG(info, "COM init status:", hr)

            assert(SUCCEEDED(hr));
            timer->deleteLater();
        });
        timer->start();
    }

    int RunMainWindow(const QApplication& app, const std::string& pboFile) {
        using namespace pboman3;

        ACTIVATE_ASYNC_LOG_SINK

        LOG(info, "Starting the app")

        ActivateCom(app);

        LOG(info, "Display the main window")
        const auto model = QScopedPointer(new PboModel());
        MainWindow w(nullptr, model.get());
        w.show();

        if (!pboFile.empty()) {
            const QString file(pboFile.c_str());
            LOG(info, "Loading the file:", file)
            w.loadFile(file);
        }

        const int exitCode = app.exec();

        LOG(info, "The app exiting with the code:", exitCode)

        return exitCode;
    }


    int RunPackWindow(const QApplication& app, const std::vector<std::string>& folders, const std::string& outDir) {
        qInfo() << "Pack";
        return 0;
    }

    int RunUnpackWindow(const QApplication& app, const std::vector<std::string>& files, const std::string& outDir) {
        qInfo() << "Unpack";
        return 0;
    }
}

int main(int argc, char* argv[]) {
    using namespace CLI;
    using namespace pboman3;

    App cli("PBO Manager V3");
    cli.require_subcommand(0, 1);

    App* cmdOpen = cli.add_subcommand("open", "Open the specified PBO file");
    std::string cmdOpenFilePath;
    auto cmdOpenFile = cmdOpen->add_option("file", cmdOpenFilePath, "The PBO file to open")
                              ->required()
                              ->check(ExistingFile);

    App* cmdPack = cli.add_subcommand("pack", "Pack the specified folders(s) as PBO(s)");
    std::vector<std::string> cmdPackFoldersPath;
    Option* cmdPackFolders = cmdPack->add_option("folders", cmdPackFoldersPath, "The folder(s) to pack")
                                    ->required()
                                    ->check(ExistingDirectory);
    std::string cmdPackOutPath;
    Option* cmdPackOut = cmdPack->add_option("-o,--output-directory", cmdPackOutPath,
                                             "The directory to write the resulting PBO(s)")
                                ->check(ExistingDirectory);

    App* cmdUnpack = cli.add_subcommand("unpack", "Unpack the specified PBO(s)");
    std::vector<std::string> cmdUnpackFilesPath;
    Option* cmdUnpackFiles = cmdUnpack->add_option("files", cmdUnpackFilesPath, "The PBO(s) to unpack")
        ->required()
        ->check(ExistingFile);
    std::string cmdUnpackOutPath;
    Option* cmdUnpackOut = cmdUnpack->add_option("-o,--output-directory", "The directory to write the PBO(s) contents")
                                    ->check(ExistingDirectory);

    CLI11_PARSE(cli, argc, argv)

    int exitCode;
    if (cli.got_subcommand(cmdOpen)) {
        const PboApplication app(argc, argv);
        exitCode = RunMainWindow(app, cmdOpenFilePath);
    } else if (cli.got_subcommand(cmdPack)) {
        const PboApplication app(argc, argv);
        exitCode = RunPackWindow(app, std::vector<std::string>(), "");
    } else if (cli.got_subcommand(cmdUnpack)) {
        const PboApplication app(argc, argv);
        exitCode = RunUnpackWindow(app, std::vector<std::string>(), "");
    } else {
        const PboApplication app(argc, argv);
        exitCode = RunMainWindow(app, "");
    }

    return exitCode;
}
