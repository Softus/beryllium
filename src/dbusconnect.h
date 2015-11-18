#ifndef DBUSCONNECT_H
#define DBUSCONNECT_H

#include <QObject>
int connectToDbusService(QObject *target, bool systemBus, const QString &service, const QString &path, const QString &interface);

#endif // DBUSCONNECT_H
