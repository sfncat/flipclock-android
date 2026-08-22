#if defined(__ANDROID__)

#include <jni.h>
#include <string.h>

#include "flipclock.h"

JNIEXPORT void JNICALL
Java_com_stackof_flipclockv2_WeatherBridge_nativeUpdateWeather(JNIEnv *env,
							  jclass clazz,
							  jstring location,
							  jstring temperature,
							  jstring description)
{
	(void)clazz;

	struct flipclock *app = flipclock_get_global_app();
	if (app == NULL)
		return;

	const char *loc = location != NULL
				  ? (*env)->GetStringUTFChars(env, location, NULL)
				  : NULL;
	const char *temp = temperature != NULL
				   ? (*env)->GetStringUTFChars(env, temperature, NULL)
				   : NULL;
	const char *desc = description != NULL
				   ? (*env)->GetStringUTFChars(env, description, NULL)
				   : NULL;

	flipclock_update_weather(app, loc, temp, desc);

	if (loc != NULL)
		(*env)->ReleaseStringUTFChars(env, location, loc);
	if (temp != NULL)
		(*env)->ReleaseStringUTFChars(env, temperature, temp);
	if (desc != NULL)
		(*env)->ReleaseStringUTFChars(env, description, desc);
}

#endif
