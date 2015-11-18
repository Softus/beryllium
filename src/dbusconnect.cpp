#include "dbusconnect.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDebug>
#include <QMetaMethod>
#include <QStringList>

int connectToDbusService(QObject *target, bool systemBus, const QString &service, const QString &path, const QString &interface)
{
    auto succeeded = 0;
    auto bus = systemBus? QDBusConnection::systemBus(): QDBusConnection::sessionBus();

    // Activate the service
    //
    auto obj = (new QDBusInterface(service, path, interface, bus, target))->metaObject();

    for (int i = 0; i < obj->methodCount(); ++i)
    {
        auto method = obj->method(i);
        if (0 == (method.attributes() & QMetaMethod::Scriptable) || method.methodType() != QMetaMethod::Signal)
        {
            // Not a connectible signal
            //
            continue;
        }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        auto sign = QString::fromUtf8(method.methodSignature());
        auto name = method.name();
#else
        auto sign = QString::fromUtf8(method.signature());
        auto name = sign.split("(").front();
#endif

        if (bus.connect(service, path, interface, name,  target, QString("1").append(sign).toUtf8()))
        {
            qDebug() << "Connected to signal" << sign;
            ++succeeded;
        }
        else
        {
            qDebug() << "Failed to connect to signal" << sign;
        }
    }

    return succeeded;
}
