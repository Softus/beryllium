#ifndef PLATFORM_H
#define PLATFORM_H

#if defined (Q_OS_WIN)
#define PLATFORM_SPECIFIC_VIDEO_SOURCE   "ksvideosrc"
#define PLATFORM_SPECIFIC_SCREEN_CAPTURE "gdiscreencapsrc"
#elif defined (Q_OS_LINUX)
#define PLATFORM_SPECIFIC_VIDEO_SOURCE   "v4l2src"
#define PLATFORM_SPECIFIC_SCREEN_CAPTURE "ximagesrc"
#elif defined (Q_OS_OSX)
#define PLATFORM_SPECIFIC_VIDEO_SOURCE   "avfvideosrc"
#define PLATFORM_SPECIFIC_SCREEN_CAPTURE "avfvideosrc"
#else
#error The platform is not supported.
#endif

extern QString getDeviceIdPropName(const QString& deviceType);

#endif // PLATFORM_H

