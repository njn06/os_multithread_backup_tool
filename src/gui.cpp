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
#include <string>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Advanced Backup Tool - Encrypted & Compressed");
    window.resize(600, 500);

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
    encryptCheck.setEnabled(false); // Always enabled now
    QCheckBox compressCheck("Compress files");
    compressCheck.setChecked(true);
    compressCheck.setEnabled(false); // Always enabled now
    optionsLayout.addWidget(&threadLabel, 0, 0);
    optionsLayout.addWidget(&threadSpin, 0, 1);
    optionsLayout.addWidget(&encryptCheck, 1, 0);
    optionsLayout.addWidget(&compressCheck, 1, 1);
    optionsGroup.setLayout(&optionsLayout);
    
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
    QPushButton startBtn("Start Backup");
    QPushButton stopBtn("Stop Backup");
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
    mainLayout.addWidget(&decryptGroup);
    mainLayout.addLayout(&buttonLayout);
    mainLayout.addWidget(&progressLabel);
    mainLayout.addWidget(&progressBar);
    mainLayout.addWidget(&logLabel);
    mainLayout.addWidget(&logText);

    QObject::connect(&srcBtn, &QPushButton::clicked, [&](){
        // Start in Windows drive root to make OneDrive visible under /mnt/c/Users/<user>/OneDrive
        QString startDir = QString::fromUtf8("/mnt/c/Users");
        QString dir = QFileDialog::getExistingDirectory(&window, "Select Source Folder", startDir);
        srcEdit.setText(dir);
    });

    QObject::connect(&destBtn, &QPushButton::clicked, [&](){
        QString startDir = QString::fromUtf8("/mnt/c/Users");
        QString dir = QFileDialog::getExistingDirectory(&window, "Select Destination Folder", startDir);
        destEdit.setText(dir);
    });

    QObject::connect(&startBtn, &QPushButton::clicked, [&](){
        QString src = srcEdit.text();
        QString dest = destEdit.text();

        if (src.isEmpty() || dest.isEmpty()) {
            QMessageBox::warning(&window, "Missing input", "Please select both source and destination.");
            return;
        }

        // Resolve CLI path next to GUI binary
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
        progressBar.setRange(0, 0); // Indeterminate
        logText.append("Starting backup...\n");
        
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
        // Create a stop-file in the destination root so the CLI loop exits gracefully
        QString dest = destEdit.text();
        if (dest.isEmpty()) {
            QMessageBox::warning(&window, "No destination", "Please select a destination first.");
            return;
        }

        // Normalize Windows-style path (e.g., C:\...) to WSL (/mnt/c/...)
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
            // Try to create the destination if possible
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
            // Auto-suggest output name
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
        
        // Use CLI to decrypt
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
