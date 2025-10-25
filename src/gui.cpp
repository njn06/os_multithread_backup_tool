//gui.cpp
#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProgressBar>
#include <QTextEdit>
#include <QSpinBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QTimer>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <string>

class BackupScheduler {
public:
    static QString getConfigPath() {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir dir(configDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        return dir.filePath("backup_schedule.json");
    }
    
    static bool saveSchedule(const QString &src, const QString &dest, int interval, int threads) {
        QJsonObject config;
        config["source"] = src;
        config["destination"] = dest;
        config["interval"] = interval;
        config["threads"] = threads;
        config["enabled"] = true;
        config["last_run"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        QJsonDocument doc(config);
        QFile file(getConfigPath());
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write(doc.toJson());
        file.close();
        return true;
    }
    
    static bool loadSchedule(QString &src, QString &dest, int &interval, int &threads, bool &enabled, QDateTime &lastRun) {
        QFile file(getConfigPath());
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) return false;
        
        QJsonObject config = doc.object();
        src = config["source"].toString();
        dest = config["destination"].toString();
        interval = config["interval"].toInt();
        threads = config["threads"].toInt();
        enabled = config["enabled"].toBool();
        lastRun = QDateTime::fromString(config["last_run"].toString(), Qt::ISODate);
        
        return !src.isEmpty() && !dest.isEmpty() && interval > 0;
    }
    
    static bool clearSchedule() {
        QFile file(getConfigPath());
        if (file.exists()) {
            return file.remove();
        }
        return true;
    }
    
    static bool updateLastRun() {
        QString src, dest;
        int interval, threads;
        bool enabled;
        QDateTime lastRun;
        
        if (!loadSchedule(src, dest, interval, threads, enabled, lastRun)) {
            return false;
        }
        
        return saveSchedule(src, dest, interval, threads);
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Advanced Backup Tool - Encrypted & Compressed");
    window.resize(650, 600);

    QVBoxLayout mainLayout;
    
    // Source selection group
    QGroupBox sourceGroup("Source Directory");
    QHBoxLayout sourceLayout;
    QLineEdit srcEdit;
    QPushButton srcBtn("Browse...");
    sourceLayout.addWidget(&srcEdit);
    sourceLayout.addWidget(&srcBtn);
    sourceGroup.setLayout(&sourceLayout);
    
    // Destination selection group
    QGroupBox destGroup("Destination Directory");
    QHBoxLayout destLayout;
    QLineEdit destEdit;
    QPushButton destBtn("Browse...");
    destLayout.addWidget(&destEdit);
    destLayout.addWidget(&destBtn);
    destGroup.setLayout(&destLayout);
    
    // Options group
    QGroupBox optionsGroup("Backup Options");
    QGridLayout optionsLayout;
    QLabel threadLabel("Threads:");
    QSpinBox threadSpin;
    threadSpin.setRange(1, 16);
    threadSpin.setValue(4);
    QCheckBox encryptCheck("Encrypt files");
    encryptCheck.setChecked(true);
    encryptCheck.setEnabled(false);
    QCheckBox compressCheck("Compress files");
    compressCheck.setChecked(true);
    compressCheck.setEnabled(false);
    
    optionsLayout.addWidget(&threadLabel, 0, 0);
    optionsLayout.addWidget(&threadSpin, 0, 1);
    optionsLayout.addWidget(&encryptCheck, 1, 0);
    optionsLayout.addWidget(&compressCheck, 1, 1);
    optionsGroup.setLayout(&optionsLayout);
    
    // Persistent timer group
    QGroupBox timerGroup("Automatic Backup Schedule (Persistent)");
    QGridLayout timerLayout;
    QLabel timerLabel("Backup interval (seconds):");
    QSpinBox timerSpin;
    timerSpin.setRange(60, 86400); // 1 minute to 24 hours
    timerSpin.setValue(300); // Default 5 minutes
    timerSpin.setSingleStep(60);
    
    QPushButton enableScheduleBtn("Enable Persistent Schedule");
    QPushButton disableScheduleBtn("Stop Persistent Schedule");
    QLabel scheduleStatusLabel("Status: Not scheduled");
    
    enableScheduleBtn.setStyleSheet("QPushButton { background-color: #FF9800; color: white; font-weight: bold; padding: 8px; }");
    disableScheduleBtn.setStyleSheet("QPushButton { background-color: #9E9E9E; color: white; font-weight: bold; padding: 8px; }");
    disableScheduleBtn.setEnabled(false);
    scheduleStatusLabel.setStyleSheet("QLabel { color: #666; font-weight: bold; }");
    
    timerLayout.addWidget(&timerLabel, 0, 0);
    timerLayout.addWidget(&timerSpin, 0, 1);
    timerLayout.addWidget(&enableScheduleBtn, 1, 0);
    timerLayout.addWidget(&disableScheduleBtn, 1, 1);
    timerLayout.addWidget(&scheduleStatusLabel, 2, 0, 1, 2);
    
    QLabel timerInfoLabel("Note: Persistent schedule runs in background even when this program is closed.\nBackup checker runs periodically to maintain the schedule.");
    timerInfoLabel.setStyleSheet("QLabel { color: #1976D2; font-size: 10px; }");
    timerInfoLabel.setWordWrap(true);
    timerLayout.addWidget(&timerInfoLabel, 3, 0, 1, 2);
    
    timerGroup.setLayout(&timerLayout);
    
    // Decrypt section
    QGroupBox decryptGroup("Decrypt Files");
    QHBoxLayout decryptLayout;
    QLineEdit decryptInput;
    QLineEdit decryptOutput;
    QPushButton decryptBtn("Decrypt");
    QPushButton decryptBrowseBtn("Browse...");
    decryptInput.setPlaceholderText("Encrypted file (.gz.enc)");
    decryptOutput.setPlaceholderText("Output file");
    decryptBtn.setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; padding: 8px; }");
    decryptLayout.addWidget(&decryptInput);
    decryptLayout.addWidget(&decryptOutput);
    decryptLayout.addWidget(&decryptBrowseBtn);
    decryptLayout.addWidget(&decryptBtn);
    decryptGroup.setLayout(&decryptLayout);
    
    // Control buttons
    QHBoxLayout buttonLayout;
    QPushButton startBtn("Run Backup Now");
    QPushButton stopBtn("Stop Current Backup");
    QPushButton clearBtn("Clear Log");
    startBtn.setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px; }");
    stopBtn.setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; padding: 8px; }");
    stopBtn.setEnabled(false);
    buttonLayout.addWidget(&startBtn);
    buttonLayout.addWidget(&stopBtn);
    buttonLayout.addWidget(&clearBtn);
    
    // Progress and log
    QLabel progressLabel("Progress:");
    QProgressBar progressBar;
    progressBar.setVisible(false);
    
    QLabel logLabel("Backup Log:");
    QTextEdit logText;
    logText.setMaximumHeight(150);
    logText.setReadOnly(true);
    
    // Add to main layout
    mainLayout.addWidget(&sourceGroup);
    mainLayout.addWidget(&destGroup);
    mainLayout.addWidget(&optionsGroup);
    mainLayout.addWidget(&timerGroup);
    mainLayout.addWidget(&decryptGroup);
    mainLayout.addLayout(&buttonLayout);
    mainLayout.addWidget(&progressLabel);
    mainLayout.addWidget(&progressBar);
    mainLayout.addWidget(&logLabel);
    mainLayout.addWidget(&logText);

    // Background timer to check and run scheduled backups
    QTimer *scheduleChecker = new QTimer(&window);
    
    // Function to check if backup should run
    auto checkAndRunScheduledBackup = [&]() {
        QString src, dest;
        int interval, threads;
        bool enabled;
        QDateTime lastRun;
        
        if (!BackupScheduler::loadSchedule(src, dest, interval, threads, enabled, lastRun)) {
            return;
        }
        
        if (!enabled) return;
        
        qint64 secondsSinceLastRun = lastRun.secsTo(QDateTime::currentDateTime());
        if (secondsSinceLastRun >= interval) {
            // Time to run backup
            logText.append(QString("=== Scheduled backup triggered at %1 ===\n")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")));
            
            QString binDir = QCoreApplication::applicationDirPath();
            QString cliPath = QDir(binDir).filePath("AdvancedBackupTool");
            
            if (!QFileInfo::exists(cliPath)) {
                logText.append("ERROR: CLI not found!\n");
                return;
            }
            
            QStringList args;
            args << src << dest << QString::number(threads);
            
            QProcess *proc = new QProcess(&window);
            proc->setProgram(cliPath);
            proc->setArguments(args);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            
            proc->start();
            if (!proc->waitForStarted(5000)) {
                logText.append("ERROR: Could not start scheduled backup!\n");
                proc->deleteLater();
                return;
            }
            
            QObject::connect(proc, &QProcess::readyRead, [proc, &logText](){
                QString output = QString::fromUtf8(proc->readAll());
                logText.append(output);
                logText.moveCursor(QTextCursor::End);
            });
            
            QObject::connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
                            proc, [proc, &logText](int code, QProcess::ExitStatus){
                if (code == 0) {
                    logText.append("=== Scheduled backup completed! ===\n\n");
                    BackupScheduler::updateLastRun();
                } else {
                    logText.append(QString("=== Scheduled backup failed (code %1) ===\n\n").arg(code));
                }
                proc->deleteLater();
            });
        }
    };
    
    // Check every 30 seconds for scheduled backups
    QObject::connect(scheduleChecker, &QTimer::timeout, checkAndRunScheduledBackup);
    scheduleChecker->start(30000); // Check every 30 seconds
    
    // Load existing schedule on startup
    auto updateScheduleStatus = [&]() {
        QString src, dest;
        int interval, threads;
        bool enabled;
        QDateTime lastRun;
        
        if (BackupScheduler::loadSchedule(src, dest, interval, threads, enabled, lastRun)) {
            srcEdit.setText(src);
            destEdit.setText(dest);
            threadSpin.setValue(threads);
            timerSpin.setValue(interval);
            
            if (enabled) {
                enableScheduleBtn.setEnabled(false);
                disableScheduleBtn.setEnabled(true);
                qint64 secsUntilNext = interval - lastRun.secsTo(QDateTime::currentDateTime());
                if (secsUntilNext < 0) secsUntilNext = 0;
                scheduleStatusLabel.setText(QString("Status: Active (next backup in ~%1 seconds)")
                    .arg(secsUntilNext));
                scheduleStatusLabel.setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; }");
            }
        }
    };
    updateScheduleStatus();

    QObject::connect(&srcBtn, &QPushButton::clicked, [&](){
        QString startDir = QString::fromUtf8("/mnt/c/Users");
        QString dir = QFileDialog::getExistingDirectory(&window, "Select Source Folder", startDir);
        if (!dir.isEmpty()) srcEdit.setText(dir);
    });

    QObject::connect(&destBtn, &QPushButton::clicked, [&](){
        QString startDir = QString::fromUtf8("/mnt/c/Users");
        QString dir = QFileDialog::getExistingDirectory(&window, "Select Destination Folder", startDir);
        if (!dir.isEmpty()) destEdit.setText(dir);
    });
    
    // Enable persistent schedule
    QObject::connect(&enableScheduleBtn, &QPushButton::clicked, [&](){
        QString src = srcEdit.text();
        QString dest = destEdit.text();
        
        if (src.isEmpty() || dest.isEmpty()) {
            QMessageBox::warning(&window, "Missing input", "Please select both source and destination.");
            return;
        }
        
        if (timerSpin.value() < 60) {
            QMessageBox::warning(&window, "Invalid interval", "Interval must be at least 60 seconds.");
            return;
        }
        
        if (BackupScheduler::saveSchedule(src, dest, timerSpin.value(), threadSpin.value())) {
            logText.append(QString("Persistent schedule enabled: Every %1 seconds\n").arg(timerSpin.value()));
            logText.append("Schedule will run even when this program is closed.\n");
            
            enableScheduleBtn.setEnabled(false);
            disableScheduleBtn.setEnabled(true);
            scheduleStatusLabel.setText(QString("Status: Active (interval: %1 seconds)")
                .arg(timerSpin.value()));
            scheduleStatusLabel.setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; }");
            
            QMessageBox::information(&window, "Schedule Enabled", 
                QString("Automatic backups will run every %1 seconds.\n\n"
                        "The backup will continue running even if you close this program.\n"
                        "Click 'Stop Persistent Schedule' to disable it.")
                .arg(timerSpin.value()));
        } else {
            QMessageBox::critical(&window, "Failed", "Could not save schedule configuration.");
        }
    });
    
    // Disable persistent schedule
    QObject::connect(&disableScheduleBtn, &QPushButton::clicked, [&](){
        if (BackupScheduler::clearSchedule()) {
            logText.append("Persistent schedule disabled.\n");
            
            enableScheduleBtn.setEnabled(true);
            disableScheduleBtn.setEnabled(false);
            scheduleStatusLabel.setText("Status: Not scheduled");
            scheduleStatusLabel.setStyleSheet("QLabel { color: #666; font-weight: bold; }");
            
            QMessageBox::information(&window, "Schedule Disabled", "Automatic backups have been stopped.");
        } else {
            QMessageBox::critical(&window, "Failed", "Could not disable schedule.");
        }
    });

    // Run backup now button
    QObject::connect(&startBtn, &QPushButton::clicked, [&](){
        QString src = srcEdit.text();
        QString dest = destEdit.text();

        if (src.isEmpty() || dest.isEmpty()) {
            QMessageBox::warning(&window, "Missing input", "Please select both source and destination.");
            return;
        }

        QString binDir = QCoreApplication::applicationDirPath();
        QString cliPath = QDir(binDir).filePath("AdvancedBackupTool");

        if (!QFileInfo::exists(cliPath) || !QFileInfo(cliPath).isExecutable()) {
            QMessageBox::critical(&window, "CLI not found", QString("Could not find CLI at: %1").arg(cliPath));
            return;
        }

        QStringList args;
        args << src << dest << QString::number(threadSpin.value());

        static QProcess proc;
        proc.setProgram(cliPath);
        proc.setArguments(args);
        proc.setProcessChannelMode(QProcess::MergedChannels);
        
        startBtn.setEnabled(false);
        stopBtn.setEnabled(true);
        progressBar.setVisible(true);
        progressBar.setRange(0, 0);
        logText.append("Starting manual backup...\n");
        
        proc.start();
        if (!proc.waitForStarted(5000)) {
            QMessageBox::critical(&window, "Failed to start", QString("Could not start: %1").arg(cliPath));
            startBtn.setEnabled(true);
            stopBtn.setEnabled(false);
            progressBar.setVisible(false);
            return;
        }
        
        QObject::connect(&proc, &QProcess::readyRead, [&](){
            QString output = QString::fromUtf8(proc.readAll());
            logText.append(output);
            logText.moveCursor(QTextCursor::End);
        });
        
        QObject::connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &window, [&](int code, QProcess::ExitStatus){
            startBtn.setEnabled(true);
            stopBtn.setEnabled(false);
            progressBar.setVisible(false);
            
            if (code == 0) {
                logText.append("Backup completed successfully!\n");
                QMessageBox::information(&window, "Done", "Backup finished! Check log.txt for encryption keys.");
            } else {
                QString output = QString::fromUtf8(proc.readAll());
                logText.append(QString("Backup failed with exit code %1\n").arg(code));
                QMessageBox::critical(&window, "Backup failed", QString("Exit code %1\n\n%2").arg(code).arg(output));
            }
        });
    });

    QObject::connect(&stopBtn, &QPushButton::clicked, [&](){
        QString dest = destEdit.text();
        if (dest.isEmpty()) {
            QMessageBox::warning(&window, "No destination", "Please select a destination first.");
            return;
        }

        auto normalizeToWSL = [](const QString &path) -> QString {
            if (path.size() >= 2 && path[1] == QChar(':')) {
                QChar drive = path[0].toLower();
                QString rest = path.mid(2);
                rest.replace("\\", "/");
                if (!rest.startsWith('/')) rest.prepend('/');
                return QString("/mnt/") + drive + rest;
            }
            QString p = path;
            p.replace("\\", "/");
            return p;
        };

        QString destWSL = normalizeToWSL(dest);
        QDir destDir(destWSL);
        if (!destDir.exists()) {
            if (!destDir.mkpath(".")) {
                QMessageBox::critical(&window, "Destination missing", QString("Destination does not exist: %1").arg(destWSL));
                return;
            }
        }

        QString stopPath = destDir.filePath(".abt_stop");
        QFile f(stopPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write("stop");
            f.close();
            logText.append(QString("Created stop file: %1\n").arg(stopPath));
            logText.append("Stop requested. Waiting for backup loop to exit...\n");
        } else {
            logText.append(QString("Failed to create stop file at %1: %2\n").arg(stopPath, f.errorString()));
            QMessageBox::critical(&window, "Failed to create stop file", QString("%1\n%2").arg(stopPath, f.errorString()));
        }
    });
    
    QObject::connect(&clearBtn, &QPushButton::clicked, [&](){
        logText.clear();
    });
    
    // Decrypt functionality
    QObject::connect(&decryptBrowseBtn, &QPushButton::clicked, [&](){
        QString startDir = QString::fromUtf8("/mnt/c/Users");
        QString file = QFileDialog::getOpenFileName(&window, "Select Encrypted File", startDir, "Encrypted Files (*.gz.enc)");
        if (!file.isEmpty()) {
            decryptInput.setText(file);
            QString output = file;
            output.replace(".gz.enc", ".dec");
            decryptOutput.setText(output);
        }
    });
    
    QObject::connect(&decryptBtn, &QPushButton::clicked, [&](){
        QString input = decryptInput.text();
        QString output = decryptOutput.text();
        
        if (input.isEmpty() || output.isEmpty()) {
            QMessageBox::warning(&window, "Missing input", "Please select both input and output files.");
            return;
        }
        
        QString binDir = QCoreApplication::applicationDirPath();
        QString cliPath = QDir(binDir).filePath("AdvancedBackupTool");
        
        QStringList args;
        args << "--decrypt" << input << output << "log.txt";
        
        QProcess proc;
        proc.setProgram(cliPath);
        proc.setArguments(args);
        proc.setProcessChannelMode(QProcess::MergedChannels);
        
        logText.append("Starting decryption...\n");
        proc.start();
        if (!proc.waitForStarted(5000)) {
            QMessageBox::critical(&window, "Failed to start", QString("Could not start: %1").arg(cliPath));
            return;
        }
        
        proc.waitForFinished(-1);
        int exitCode = proc.exitCode();
        QString output_text = QString::fromUtf8(proc.readAll());
        logText.append(output_text);
        
        if (exitCode == 0) {
            QMessageBox::information(&window, "Success", "File decrypted successfully!");
            logText.append("Decryption completed successfully!\n");
        } else {
            QMessageBox::critical(&window, "Decryption failed", QString("Exit code %1\n\n%2").arg(exitCode).arg(output_text));
        }
    });

    window.setLayout(&mainLayout);
    window.show();
    return app.exec();
}