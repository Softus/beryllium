#include "enumsrc.h"
#include <QStringList>
#include <QGst/ElementFactory>
#include <QGst/PropertyProbe>

QStringList enumSources(const QString& elmName, const QString& propName)
{
    QStringList ret;
    auto src = QGst::ElementFactory::make(elmName);
    if (src)
    {
        QGst::PropertyProbePtr propertyProbe = src.dynamicCast<QGst::PropertyProbe>();
        if (propertyProbe && propertyProbe->propertySupportsProbe(propName.toUtf8()))
        {
            //get a list of devices that the element supports
            auto devices = propertyProbe->probeAndGetValues(propName.toUtf8());
            foreach (const QGlib::Value& deviceId, devices)
            {
                ret << deviceId.toString();
            }
        }
    }

    return ret;
}
