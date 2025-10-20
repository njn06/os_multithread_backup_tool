#ifndef BACKUP_H
#define BACKUP_H

#include <string>

void performBackup(const std::string &srcDir, const std::string &destDir, int threadCount = 4);
void requestStopBackup();

#endif
